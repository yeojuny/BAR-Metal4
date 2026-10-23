/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <functional>

namespace rendering {
// Source primitive and vertex order remain available for polygon boundary
// rasterization; canonical fill indices do not define a quad's outline.
NativeWorldDrawCommand MakeNativeWorldDrawCommand(NativeWorldDrawStage stage, NativeDrawPrimitive primitive,
	std::span<const NativeWorldDrawVertex> vertices, const ScreenDrawState& surface, const NativeWorldDrawState& world,
	const std::function<std::shared_ptr<const NativeGenericShadowInput>()>& snapshotShadowInput = {});
// Receives evaluated relative commands from the common UI/list interpreter.
// No Lua, GL, renderer handles, or gameplay state belongs to this recorder.
class NativeWorldDrawRecorder {
public:
	void BeginFrame();
	std::shared_ptr<const NativeWorldDrawFrame> EndFrame();
	// Copy only already-recorded commands; retain the active recorder and its
	// serial for the original callback's suffix. No replay or source capture.
	std::shared_ptr<const NativeWorldDrawFrame> SnapshotPrefix(std::uint64_t cutoff) const;
	void AbortFrame() noexcept;
	void Reset() noexcept;
	bool IsActive() const noexcept {return bool(frame);}
	void Record(NativeWorldDrawStage stage, NativeDrawPrimitive primitive,
		std::span<const NativeWorldDrawVertex> vertices,
		const ScreenDrawState& surface, const NativeWorldDrawState& world,
		const std::function<std::shared_ptr<const NativeGenericShadowInput>()>& snapshotShadowInput = {});
 // Receives the one actual clear event from the existing common UI/list
 // interpreter. No GL mask conversion, source observation or replay here.
 std::uint64_t RecordClear(NativeWorldDrawStage stage,ScreenAttachmentMask mask,
  const ScreenClearValues& values,const NativeWorldDrawState& world,
  std::optional<ScreenScissor> scissor);
 void RecordDistortion(std::shared_ptr<const NativeDistortionCommand>);
 void RecordObjectSpotlight(std::shared_ptr<const NativeObjectSpotlightInput>,const ScreenDrawState&,const NativeWorldDrawState&);
 void RecordUnitGlass(std::shared_ptr<const NativeUnitGlassInput>,const ScreenDrawState&,const NativeWorldDrawState&);
 void RecordUnitEffect(NativeWorldDrawStage,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeUnitEffectInput>);
 void RecordWaterOverlay(std::span<const NativeWorldDrawVertex>,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeWaterOverlayInput>);
 void RecordLOSView(std::span<const NativeWorldDrawVertex>,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeLOSViewInput>);
 void RecordTerraformOverlay(std::span<const NativeWorldDrawVertex>,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeTerraformOverlayInput>);
 void RecordSquadHull(NativeDrawPrimitive,std::span<const NativeWorldDrawVertex>,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeSquadHullInput>);
 void RecordEasyFacing(NativeWorldDrawStage,std::span<const NativeWorldDrawVertex>,const ScreenDrawState&,const NativeWorldDrawState&,std::shared_ptr<const NativeEasyFacingInput>);
 void RecordSensorLOS(std::shared_ptr<const NativeSensorLOSDraw>);
 void RecordResourceSpots(std::shared_ptr<const NativeResourceSpotsInput>,NativeWorldDrawStage,const ScreenDrawState&,const NativeWorldDrawState&);
 void RecordGrass(std::shared_ptr<const NativeGrassInput>,const ScreenDrawState&,const NativeWorldDrawState&);
 void RecordReclaimField(std::shared_ptr<const NativeReclaimFieldInput>,const ScreenDrawState&,const NativeWorldDrawState&);
private:
	std::shared_ptr<NativeWorldDrawFrame> frame;
	std::uint64_t serial = 0;
};
NativeWorldDrawRecorder& GetWorldDrawRecorder();
} // namespace rendering
