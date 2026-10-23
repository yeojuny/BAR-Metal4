/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTerrainSourceData.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rendering {
struct NativeCUSDeferredPhase;

// Written at the real original uniform locations, including link defaults and
// original Lua writes. An absent entry remains unobserved. Matrices retain the
// source shape and transpose argument; consumers must apply those semantics.
struct NativeTerrainUniformValue {
	uint32_t rows=1, columns=1, count=1;
	bool transpose=false;
	std::variant<std::vector<float>,std::vector<int32_t>,std::vector<uint32_t>> values;
};
struct NativeTerrainUniformSnapshot {
	uint64_t contextGeneration=0, originalProgramGeneration=0, revision=0;
	std::map<std::string,NativeTerrainUniformValue> values;
};
struct NativeTerrainProgramSnapshot {
	NativeTerrainProgramIdentity identity;
	std::shared_ptr<const NativeTerrainProgramConsumer> consumer;
	std::shared_ptr<const NativeTerrainUniformSnapshot> uniforms;
};

enum class NativeTerrainMeshSource : uint8_t { Basic, ROAM, Composite };
enum class NativeGroundCallback : uint8_t { PreForward, PostForward, PreDeferred, Deferred, PostDeferred };
struct NativeGroundSourceSpan {
	uint64_t begin=0, end=0; // Exclusive bounds in the existing common draw order.
	bool Contains(uint64_t sequence) const noexcept {return begin<sequence && sequence<end;}
};

// Copied at the existing Basic/ROAM actual submit, like the MAP shadow
// producer. This is the original selected mesh, not a generated SMT tile grid.
// Local coordinates and exact triangle order survive to the same frame/lease.
struct NativeTerrainMeshGeometry {
	bool indexed=true;
	std::vector<std::array<float,3>> positions;
	std::vector<std::array<uint8_t,4>> colors; // Original normalized UByte border attribute; empty for forward.
	std::vector<uint32_t> indices;
};

struct NativeTerrainSquareDraw {
	uint64_t sequence=0;
	uint32_t squareX=0, squareZ=0;
	NativeTerrainMeshSource mesh=NativeTerrainMeshSource::Basic;
	uint64_t originalIndexCount=0; // Provenance of the actual nonempty submission.
	bool border=false,deferred=false,composite=false;
	NativeTerrainMeshGeometry geometry;
	NativeTerrainProgramSnapshot program;
	NativeTerrainImageSnapshot images;
	NativeWorldDrawState world;
	ScreenDrawState surface;
	std::array<float,16> modelView{}, projection{};
	std::array<bool,3> clipEnabled{};
	std::array<std::array<float,4>,3> clipEyeEquations{};
	bool fogEnabled=false;
	std::array<float,4> fogColor{}, fogParameters{};
};

// One original Draw in its existing unsafe main, IBL or water stage. A missing frame means
// the outer reflection-mode predicate omitted Draw. A present frame with
// drawEntered=false records an original drawGround/void-water early return.
// Source state remains live in the common context; there is no exit-state
// reconstruction or resetting state between callbacks and original models.
struct NativeWaterGroundSourceFrame {
	uint64_t contextGeneration=0, sourceDrawFrame=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::WaterReflection;
	NativeGroundSourceSpan source;
	bool drawEntered=false, forwardEntered=false, borderEntered=false, deferredEntered=false;
	std::optional<NativeGroundSourceSpan> preForward, postForward, deferred, preDeferred, deferredCallback, postDeferred;
 std::optional<WorldCamera> sourceView;
 std::shared_ptr<const NativeCUSDeferredPhase> mapMRT;
 bool deferredClearOnly=false,combined=false,compositeEntered=false;
	std::vector<NativeTerrainSquareDraw> squares;
	// Original DrawGroundDeferred submissions at their own ordinals. This is
	// independent of the later DrawWorldPreUnit map-edge packet and state update.
	std::vector<std::shared_ptr<const mapedge::DrawPacket>> mapEdges;
};
// The existing common ordinal stream owns generic geometry and clears. This
// packet retains one actual caller/camera and its same-invocation typed owners.
struct NativeWorldSkyResource;
struct NativeTerrainSceneSourceFrame {
 uint64_t sourceDrawFrame=0,initialClearSequence=0;
 NativeWorldDrawStage stage=NativeWorldDrawStage::Ground;
 WorldCamera viewCamera;
 uint32_t reflectionFace=0;
 bool skyOnly=false,groundInvoked=false;
 NativeWorldDrawState debugCubeState;
 std::shared_ptr<const NativeWorldSkyResource> debugCubeSky; // Actual debug branch; only reflectionFace is drawn.
 std::shared_ptr<const NativeWaterGroundSourceFrame> ground;
 std::shared_ptr<const NativeCUSSourceFrame> cus;
 std::shared_ptr<const NativeWaterLuaFrame> lua;
};
} // namespace rendering
