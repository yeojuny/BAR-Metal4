/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4SceneInputProbe.h"
#include "System/SDL3Compat/SDL3Helpers.h"
#include "Game/Camera.h"
#include "Game/Game.h"
#include "Game/GameController.h"
#include "Game/GlobalUnsynced.h"
#include "Game/SelectedUnitsHandler.h"
#include "Game/TraceRay.h"
#include "Game/UI/GuiHandler.h"
#include "Game/UI/MouseHandler.h"
#include "Map/Ground.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/MoveTypes/MoveMath/MoveMath.h"
#include "Sim/Units/CommandAI/CommandAI.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitHandler.h"
#include "System/Log/ILog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace rendering::scene {
namespace {
using Clock = std::chrono::steady_clock;
constexpr int phaseFrameLimit = 180;
constexpr int movementFrameLimit = 450;
constexpr double phaseSecondsLimit = 12.0;
constexpr double movementSecondsLimit = 18.0;
constexpr float movementDistance = 16.0f;

bool Finite(const float3& p)
{
	return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
float DistanceXZSquared(const float3& a, const float3& b)
{
	const float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
}
}

struct Metal4SceneInputProbe::Impl {
	enum class Step { Waiting, SeedSelection, WheelZoom, Deselect, Reselect, MoveCommand, Movement, Terminal };
	SDL_Window* window;
	InputProbeResult result;
	Step step = Step::Waiting;
	Uint32 markerType = 0;
	int serial = 0;
	bool markerSeen = false;
	bool started = false;
	bool wheelEffectSeen = false;
	bool moveEvidenceLogged = false;
	float3 luaMoveTarget, verifiedMoveDestination;
	bool luaMoveTargetValid = false;
	int wheelStableFrame = -1;
	float3 wheelStableCamera, wheelStableDirection;
	CMatrix44f wheelStableProjection;
	int phaseFrame = 0;
	int stableFrame = -1;
	int firstFrame = 0;
	int nextWaitingEvidence = 0;
	Clock::time_point firstTime, phaseTime;
	float3 stableCamera, beforeCamera, beforeDirection, beforeUnit, moveTarget;
	CMatrix44f beforeProjection;

	explicit Impl(SDL_Window* value): window(value)
	{
		const char* option = std::getenv("RECOIL_METAL4_SCENE_INPUT_PROBE");
		if (option == nullptr || option[0] == '\0' || std::string_view(option) == "0") return;
		result.state = InputProbeState::Waiting;
		result.phase = "waiting_for_commander";
		if (std::string_view(option) != "1") { Fail("opt-in value must be exactly 1 (or 0 to disable)"); return; }
		if (window == nullptr || !SDL_IsMainThread()) { Fail("probe requires a real SDL3 window on the main thread"); return; }
		markerType = SDL_RegisterEvents(1);
		if (markerType == 0) { Fail("could not register SDL3 phase marker"); return; }
		LOG_L(L_WARNING, "[Metal4InputProbe] Enabled isolated SDL3 queue diagnostic; no direct engine state mutation, no human/gameplay acceptance");
	}

	~Impl()
	{
		// SDL3 has no unregister API. Retire this unique type, not other events.
		// Markers contain only copied scalar fields; never pointers to this object.
		if (markerType != 0) {
			SDL_FlushEvent(markerType);
			SDL_SetEventEnabled(markerType, false);
		}
	}

	int Frame() const { return gs == nullptr ? 0 : std::max(0, gs->frameNum); }
	void Fail(std::string reason)
	{
		if (result.state == InputProbeState::Failed || result.state == InputProbeState::Passed) return;
		result.reason = std::move(reason);
		result.state = InputProbeState::Failed;
		step = Step::Terminal;
		LOG_L(L_ERROR, "[Metal4InputProbe] FAILED phase=%s frame=%llu unit=%d marker=%d reason=%s",
			result.phase.c_str(), static_cast<unsigned long long>(result.simulationFrame), result.unitID, markerSeen, result.reason.c_str());
	}
	bool Push(SDL_Event& event)
	{
		if (SDL_PushEvent(&event)) return true;
		Fail(std::string("SDL3 event rejected/filtered: ") + SDL_GetError());
		return false;
	}
	bool Marker()
	{
		SDL_Event event{};
		event.type = markerType;
		event.user.windowID = SDL_GetWindowID(window);
		event.user.code = serial;
		return Push(event);
	}
	void Start(Step next, const char* name)
	{
		step = next;
		result.state = InputProbeState::Running;
		result.phase = name;
		phaseFrame = Frame(); phaseTime = Clock::now(); markerSeen = false; ++serial;
	}
	void Evidence(const char* message) const
	{
		const CUnit* unit = unitHandler.GetUnit(result.unitID);
		const float3 p = unit == nullptr ? float3{} : unit->pos;
		const float3 c = camera->GetPos();
		LOG("[Metal4InputProbe] phase=%s frame=%d unit=%d selected=%d camera=(%.3f,%.3f,%.3f) unitPos=(%.3f,%.3f,%.3f) marker=%d %s",
			result.phase.c_str(), Frame(), result.unitID, selectedUnitsHandler.IsUnitSelected(result.unitID),
			c.x, c.y, c.z, p.x, p.y, p.z, markerSeen, message);
	}
	bool Project(const float3& world, int& x, int& y) const
	{
		const float4 clip = camera->GetViewProjectionMatrix() * float4(world, 1.0f);
		if (!Finite(world) || !std::isfinite(clip.w) || clip.w <= 0.0f) return false;
		const float3 point = camera->CalcViewPortCoordinates(world);
		if (!Finite(point) || point.z < 0.0f || point.z > 1.0f || point.x < 12.0f || point.x >= globalRendering->viewSizeX - 12.0f ||
			point.y < 12.0f || point.y >= globalRendering->viewSizeY - 12.0f) return false;
		// CalcViewPortCoordinates omits viewport offsets and has bottom-left Y.
		x = static_cast<int>(std::round(point.x)) + globalRendering->viewPosX;
		y = static_cast<int>(std::round(globalRendering->viewSizeY - point.y)) + globalRendering->viewWindowOffsetY;
		return x >= globalRendering->viewPosX + 12 && x < globalRendering->viewPosX + globalRendering->viewSizeX - 12 &&
			y >= globalRendering->viewWindowOffsetY + 12 && y < globalRendering->viewWindowOffsetY + globalRendering->viewSizeY - 12;
	}
	float3 Ray(int x, int y) const
	{
		return camera->CalcPixelDir(x, y - globalRendering->viewWindowOffsetY);
	}
	bool UnitPixel(const CUnit& unit, int& x, int& y) const
	{
		// BAR Ignore Self intentionally shrinks the selected unit's volume;
		// its ordinary MousePress restores it before selection. A preflight
		// stock ray hit would wrongly prevent that real Lua input path.
		return Project(unit.GetObjDrawMidPos(), x, y);
	}
	void WaitingEvidence(const CUnit& unit)
	{
		const float3 mid = unit.GetObjDrawMidPos();
		const float3 projected = camera->CalcViewPortCoordinates(mid);
		const float4 clip = camera->GetViewProjectionMatrix() * float4(mid, 1.0f);
		const float3 scales = unit.selectionVolume.GetScales(), offsets = unit.selectionVolume.GetOffsets();
		int x = 0, y = 0;
		const bool projectedInside = Project(mid, x, y);
		const CUnit* hit = nullptr; const CFeature* feature = nullptr;
		float distance = -1.0f;
		if (projectedInside)
			distance = TraceRay::GuiTraceRay(camera->GetPos(), Ray(x, y), camera->GetFarPlaneDist() * 1.4f, nullptr, hit, feature, false);
		const float3 eye = camera->GetPos();
		LOG("[Metal4InputProbe] waiting evidence frame=%d stableFrames=%d camera=(%.6f,%.6f,%.6f) viewport=(%d,%d,%d,%d) logicalView=(%d,%d,%d,%d) drawPos=(%.3f,%.3f,%.3f) mid=(%.3f,%.3f,%.3f)",
			Frame(), Frame() - stableFrame, eye.x, eye.y, eye.z, camera->viewport[0], camera->viewport[1], camera->viewport[2], camera->viewport[3],
			globalRendering->viewPosX, globalRendering->viewWindowOffsetY, globalRendering->viewSizeX, globalRendering->viewSizeY,
			unit.drawPos.x, unit.drawPos.y, unit.drawPos.z, mid.x, mid.y, mid.z);
		LOG("[Metal4InputProbe] picking evidence projected=(%.3f,%.3f,%.6f) clipZW=(%.6f,%.6f) inside=%d SDLlogical=(%d,%d) far=%.3f hitUnit=%d feature=%d distance=%.3f icon=%d iconRadius=%.3f noSelect=%d void=%d pieceTree=%d ignoreHits=%d volumeScales=(%.3f,%.3f,%.3f) offsets=(%.3f,%.3f,%.3f)",
			projected.x, projected.y, projected.z, clip.z, clip.w, projectedInside, x, y, camera->GetFarPlaneDist(), hit ? hit->id : -1, feature != nullptr, distance,
			unit.GetIsIcon(), unit.iconRadius, unit.noSelect, unit.IsInVoid(), unit.selectionVolume.DefaultToPieceTree(), unit.selectionVolume.IgnoreHits(),
			scales.x, scales.y, scales.z, offsets.x, offsets.y, offsets.z);
	}
	bool Motion(int x, int y)
	{
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.windowID = SDL_GetWindowID(window);
		event.motion.x = x; event.motion.y = y;
		// Deliberately zero relative movement: position establishes the ordinary
		// click/wheel ray without a synthetic drag or camera-pan side effect.
		return Push(event);
	}
	bool Click(int x, int y, Uint8 button)
	{
		if (!Motion(x, y)) return false;
		SDL_Event event{};
		event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
		event.button.windowID = SDL_GetWindowID(window);
		event.button.button = button; event.button.down = true; event.button.clicks = 1;
		event.button.x = x; event.button.y = y;
		if (!Push(event)) return false;
		event.type = SDL_EVENT_MOUSE_BUTTON_UP; event.button.down = false;
		if (!Push(event)) return false;
		LOG("[Metal4InputProbe] queued phase=%s serial=%d button=%u logical=(%d,%d); SDL3 queue does not alter physical device state",
			result.phase.c_str(), serial, button, x, y);
		return Marker();
	}
	bool TerrainPassable(const CUnit& unit, const float3& point) const
	{
		const MoveDef& md = *unit.moveDef;
		MoveTypes::CheckCollisionQuery query(&unit); // Local value, no unit writes.
		const int sx = int(point.x / SQUARE_SIZE), sz = int(point.z / SQUARE_SIZE);
		for (int z = sz - md.zsizeh; z <= sz + md.zsizeh; ++z) {
			for (int x = sx - md.xsizeh; x <= sx + md.xsizeh; ++x) {
				if (x < 0 || z < 0 || x >= mapDims.mapx || z >= mapDims.mapy) return false;
				if (CMoveMath::GetPosSpeedMod(md, unsigned(x), unsigned(z)) <= 0.0f) return false;
				// SquareIsBlocked only updates our local query. RangeIsBlocked
				// writes engine object visit tags, so the probe never calls it.
				if (CMoveMath::SquareIsBlocked(md, x, z, &query) != CMoveMath::BLOCK_NONE) return false;
			}
		}
		return true;
	}
	bool GroundPixel(const CUnit& unit, int& x, int& y, float3& ground) const
	{
		constexpr std::array<std::array<float, 2>, 8> directions{{{1,0},{0,1},{-1,0},{0,-1},{0.7071f,0.7071f},{-0.7071f,0.7071f},{-0.7071f,-0.7071f},{0.7071f,-0.7071f}}};
		for (float distance: {128.0f, 192.0f, 256.0f}) {
			for (const auto& direction: directions) {
				float3 candidate = unit.pos + float3(direction[0] * distance, 0.0f, direction[1] * distance);
				if (!candidate.IsInBounds()) continue;
				candidate.y = CGround::GetHeightReal(candidate.x, candidate.z, false);
				if (!Project(candidate, x, y)) continue;
				const CUnit* hit = nullptr; const CFeature* feature = nullptr;
				const float range = camera->GetFarPlaneDist() * 1.4f;
				const float rayDistance = TraceRay::GuiTraceRay(camera->GetPos(), Ray(x, y), range, nullptr, hit, feature, true);
				if (hit != nullptr || feature != nullptr || rayDistance < 0.0f || rayDistance >= range) continue;
				ground = camera->GetPos() + Ray(x, y) * rayDistance;
				if (!ground.IsInBounds() || !Finite(ground) || DistanceXZSquared(ground, unit.pos) < 64.0f * 64.0f) continue;
				bool passable = true;
				for (int sample = 1; sample <= 32; ++sample) {
					if (!TerrainPassable(unit, unit.pos + (ground - unit.pos) * (float(sample) / 32.0f))) { passable = false; break; }
				}
				if (passable) return true;
			}
		}
		return false;
	}
	void CommandEvidence(const CUnit& unit, const char* moment) const
	{
		Evidence(moment);
		LOG("[Metal4InputProbe] move evidence queueSize=%u XZdisplacement=%.3f seabed=(%.3f,%.3f,%.3f) originalLuaWaterTarget=(%.3f,%.3f,%.3f)",
			unsigned(unit.commandAI->commandQue.size()), std::sqrt(DistanceXZSquared(unit.pos, beforeUnit)),
			moveTarget.x, moveTarget.y, moveTarget.z, luaMoveTarget.x, luaMoveTarget.y, luaMoveTarget.z);
		unsigned index = 0;
		for (const Command& command: unit.commandAI->commandQue) {
			if (index >= 4) break;
			const unsigned count = command.GetNumParams();
			LOG("[Metal4InputProbe] command[%u] id=%d params=%u firstXYZ=(%.3f,%.3f,%.3f)", index++, command.GetID(), count,
				count > 0 ? command.GetParam(0) : 0.0f, count > 1 ? command.GetParam(1) : 0.0f, count > 2 ? command.GetParam(2) : 0.0f);
		}
	}
	bool InputsReady()
	{
		if (gu->spectating || gu->fpsMode || mouse->locked || !SDL_CursorVisible() || SDL_GetWindowRelativeMouseMode(window)) {
			Fail("normal unlocked local-player input required; no diagnostic mode changes permitted"); return false;
		}
		if (gs->paused || guihandler->inCommand != -1) { Fail("paused game or active GUI command prevents ordinary click diagnostic"); return false; }
		if ((SDL_GetModState() & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0 || SDL_GetMouseState(nullptr, nullptr) != 0) {
			Fail("physical modifier/button input interferes with isolated probe"); return false;
		}
		return true;
	}
	void QueueSelection(const CUnit& unit, Step next, const char* name)
	{
		Start(next, name);
		int x = 0, y = 0;
		if (!UnitPixel(unit, x, y)) { Fail("commander projection is offscreen"); return; }
		Click(x, y, SDL_BUTTON_LEFT);
	}
	bool TimedOut() const
	{
		const bool moving = step == Step::Movement;
		return Frame() - phaseFrame > (moving ? movementFrameLimit : phaseFrameLimit) ||
			std::chrono::duration<double>(Clock::now() - phaseTime).count() > (moving ? movementSecondsLimit : phaseSecondsLimit);
	}
	const InputProbeResult& Update()
	{
		if (result.state == InputProbeState::Disabled || step == Step::Terminal) return result;
		if (!SDL_IsMainThread()) { Fail("probe update must run on the main thread"); return result; }
		result.simulationFrame = Frame();
		if (!started) { started = true; firstTime = Clock::now(); firstFrame = Frame(); }
		if (game == nullptr || gu == nullptr || gs == nullptr || mouse == nullptr || guihandler == nullptr || camera == nullptr || readMap == nullptr || activeController != game) {
			if (std::chrono::duration<double>(Clock::now() - firstTime).count() > 30.0) Fail("no active real game/input/camera within startup deadline");
			return result;
		}
		if (!InputsReady()) return result;
		if (step == Step::Waiting) {
			const CUnit* commander = nullptr;
			for (const CUnit* candidate: unitHandler.GetUnitsByTeam(gu->myTeam)) {
				if (candidate->unitDef->name == "armcom" && !candidate->isDead && !candidate->beingBuilt && candidate->moveDef != nullptr &&
					(commander == nullptr || candidate->id < commander->id)) commander = candidate;
			}
			if (commander != nullptr) {
				result.unitID = commander->id;
				if (stableFrame < 0 || (camera->GetPos() - stableCamera).SqLength() > 0.0001f) { stableCamera = camera->GetPos(); stableFrame = Frame(); }
				if (Frame() >= nextWaitingEvidence) { WaitingEvidence(*commander); nextWaitingEvidence = Frame() + 90; }
				int x, y;
				if (Frame() - stableFrame >= 10 && UnitPixel(*commander, x, y)) {
					if (!commander->commandAI->commandQue.empty()) { Fail("commander has pre-existing orders; isolated diagnostic required"); return result; }
					Evidence("startup camera stable; real own armcom located");
					QueueSelection(*commander, Step::SeedSelection, "seed_selection");
					return result;
				}
			}
			if (Frame() - firstFrame > 900 || std::chrono::duration<double>(Clock::now() - firstTime).count() > 30.0) {
				if (commander != nullptr) WaitingEvidence(*commander);
				Fail("no stable camera and visible own movable armcom within startup deadline");
			}
			return result;
		}
		const CUnit* unit = unitHandler.GetUnit(result.unitID);
		if (unit == nullptr || unit->isDead || unit->team != gu->myTeam || unit->commandAI == nullptr) { Fail("observed commander disappeared or changed ownership"); return result; }
		if (!Finite(unit->pos)) { Fail("observed commander position is non-finite; no state repair permitted"); return result; }
		if (!markerSeen) {
			if (TimedOut()) Fail("registered phase marker never reached original event dispatch");
			return result;
		}
		int x = 0, y = 0;
		switch (step) {
			case Step::SeedSelection:
				if (!selectedUnitsHandler.IsUnitSelected(result.unitID)) break;
				Evidence("selection click observed in real engine selection");
				beforeCamera = camera->GetPos(); beforeDirection = camera->GetDir(); beforeProjection = camera->GetProjectionMatrix();
				Start(Step::WheelZoom, "wheel_zoom");
				if (!UnitPixel(*unit, x, y)) { Fail("commander unavailable for wheel position"); break; }
				if (Motion(x, y)) {
					SDL_Event event{}; event.type = SDL_EVENT_MOUSE_WHEEL;
					event.wheel.windowID = SDL_GetWindowID(window); event.wheel.y = 2.0f;
					event.wheel.mouse_x = x; event.wheel.mouse_y = y;
					if (Push(event)) Marker();
				}
				return result;
			case Step::WheelZoom: {
				float matrixDelta = 0.0f;
				for (size_t i = 0; i < 16; ++i) matrixDelta = std::max(matrixDelta, std::abs(camera->GetProjectionMatrix()[i] - beforeProjection[i]));
				if (!wheelEffectSeen) {
					if ((camera->GetPos() - beforeCamera).SqLength() <= 1.0f && (camera->GetDir() - beforeDirection).SqLength() <= 0.000001f && matrixDelta <= 0.00001f) break;
					wheelEffectSeen = true;
					wheelStableCamera = camera->GetPos(); wheelStableDirection = camera->GetDir(); wheelStableProjection = camera->GetProjectionMatrix();
					wheelStableFrame = Frame();
					Evidence("queued wheel changed actual camera; waiting for ordinary smoothing to settle");
					break;
				}
				float projectionMotion = 0.0f;
				for (size_t i = 0; i < 16; ++i) projectionMotion = std::max(projectionMotion, std::abs(camera->GetProjectionMatrix()[i] - wheelStableProjection[i]));
				if ((camera->GetPos() - wheelStableCamera).SqLength() > 0.0001f || (camera->GetDir() - wheelStableDirection).SqLength() > 0.000001f || projectionMotion > 0.00001f) {
					wheelStableCamera = camera->GetPos(); wheelStableDirection = camera->GetDir(); wheelStableProjection = camera->GetProjectionMatrix(); wheelStableFrame = Frame();
				}
				if (Frame() - wheelStableFrame < 10) break;
				Evidence("wheel camera/projection settled before subsequent projected clicks");
				Start(Step::Deselect, "empty_ground_deselect");
				if (!GroundPixel(*unit, x, y, moveTarget)) { Fail("no visible empty/passable nearby ground for deselect"); break; }
				Click(x, y, SDL_BUTTON_LEFT);
				return result;
			}
			case Step::Deselect:
				if (!selectedUnitsHandler.selectedUnits.empty()) break;
				Evidence("empty-world click cleared actual selection");
				QueueSelection(*unit, Step::Reselect, "commander_reselect");
				return result;
			case Step::Reselect: {
				if (!selectedUnitsHandler.IsUnitSelected(result.unitID) || selectedUnitsHandler.selectedUnits.size() != 1) break;
				Evidence("commander reselected through ordinary left click");
				Start(Step::MoveCommand, "right_ground_move");
				if (!GroundPixel(*unit, x, y, moveTarget)) { Fail("no visible empty/passable legal nearby movement target"); break; }
				beforeUnit = unit->pos;
				// Active BAR CustomFormations2 uses Spring.TraceScreenRay with
				// onlyCoords=true and default ignoreWater=false for a simple RMB.
				const CUnit* ignoredUnit = nullptr; const CFeature* ignoredFeature = nullptr;
				const float luaDistance = TraceRay::GuiTraceRay(camera->GetPos(), Ray(x, y), camera->GetFarPlaneDist() * 1.4f,
					nullptr, ignoredUnit, ignoredFeature, true, true, false);
				luaMoveTarget = camera->GetPos() + Ray(x, y) * luaDistance;
				luaMoveTargetValid = luaDistance >= 0.0f && luaDistance < camera->GetFarPlaneDist() * 1.4f && Finite(luaMoveTarget) && luaMoveTarget.IsInBounds();
				LOG("[Metal4InputProbe] requested ground=(%.3f,%.3f,%.3f) from=(%.3f,%.3f,%.3f)", moveTarget.x, moveTarget.y, moveTarget.z, beforeUnit.x, beforeUnit.y, beforeUnit.z);
				Click(x, y, SDL_BUTTON_RIGHT);
				return result;
			}
			case Step::MoveCommand:
				if (!moveEvidenceLogged) { CommandEvidence(*unit, "first observed update after RMB phase marker"); moveEvidenceLogged = true; }
				for (const Command& command: unit->commandAI->commandQue) {
					if (command.GetID() != CMD_MOVE || command.GetNumParams() < 3) continue;
					const float3 destination = command.GetPos(0);
					if (!Finite(destination) || !destination.IsInBounds()) continue;
					// Original engine map commands use terrain; active BAR
					// CustomFormations2 uses the nearer water/terrain surface.
					// Match only these two exact source-derived canonical rays,
					// retaining the same 32-elmo XZ tolerance (not a wider net).
					const bool terrainMatch = DistanceXZSquared(destination, moveTarget) <= 32.0f * 32.0f;
					const bool luaMatch = luaMoveTargetValid && DistanceXZSquared(destination, luaMoveTarget) <= 32.0f * 32.0f;
					if (!terrainMatch && !luaMatch) continue;
					bool passable = true;
					for (int sample = 1; sample <= 32; ++sample) {
						if (!TerrainPassable(*unit, beforeUnit + (destination - beforeUnit) * (float(sample) / 32.0f))) { passable = false; break; }
					}
					if (!passable) { Fail("observed canonical move destination is not passable for the real commander footprint"); return result; }
					verifiedMoveDestination = destination;
					Evidence("actual synced command queue contains canonical matching CMD_MOVE");
					LOG("[Metal4InputProbe] observed CMD_MOVE destination=(%.3f,%.3f,%.3f) canonical=%s toleranceXZ=32",
						destination.x, destination.y, destination.z, luaMatch ? "original BAR Lua water/terrain ray" : "original engine terrain ray");
					step = Step::Movement; result.phase = "observe_synced_movement";
					phaseFrame = Frame(); phaseTime = Clock::now();
					return result;
				}
				break;
			case Step::Movement:
				if (DistanceXZSquared(unit->pos, beforeUnit) < movementDistance * movementDistance) break;
				if (DistanceXZSquared(unit->pos, verifiedMoveDestination) >= DistanceXZSquared(beforeUnit, verifiedMoveDestination)) { Fail("commander moved away from the canonically matched real command destination"); break; }
				Evidence("actual synced XZ position moved at least 16 elmos toward canonically matched command destination");
				result.state = InputProbeState::Passed; step = Step::Terminal;
				result.reason = "SDL3 wheel, empty deselect, commander reselect, synced CMD_MOVE and XZ displacement observed; automated diagnostic only";
				LOG("[Metal4InputProbe] PASSED frame=%llu unit=%d %s", static_cast<unsigned long long>(result.simulationFrame), result.unitID, result.reason.c_str());
				return result;
			default: break;
		}
		if (step != Step::Terminal && TimedOut()) {
			if (step == Step::MoveCommand || step == Step::Movement) CommandEvidence(*unit, "movement phase timeout");
			Fail("marker dispatched but expected engine effect absent; possible UI interception or rejected input/command");
		}
		return result;
	}
};

Metal4SceneInputProbe::Metal4SceneInputProbe(SDL_Window* window): impl(std::make_unique<Impl>(window)) {}
Metal4SceneInputProbe::~Metal4SceneInputProbe() = default;
const InputProbeResult& Metal4SceneInputProbe::Update() { return impl->Update(); }
const InputProbeResult& Metal4SceneInputProbe::Result() const { return impl->result; }
void Metal4SceneInputProbe::ObserveEvent(const SDL_Event& event)
{
	if (impl->markerType != 0 && event.type == impl->markerType && event.user.code == impl->serial &&
		event.user.windowID == SDL_GetWindowID(impl->window)) {
		impl->markerSeen = true;
		LOG("[Metal4InputProbe] marker dispatched phase=%s serial=%d frame=%d", impl->result.phase.c_str(), impl->serial, impl->Frame());
	}
}

} // namespace rendering::scene
