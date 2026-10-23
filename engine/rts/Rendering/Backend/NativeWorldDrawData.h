/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace rendering {
struct NativeGenericShadowInput;
struct NativeEasyFacingInput;
struct NativeSquadHullInput;
struct NativeTerraformOverlayInput;
struct NativeLOSViewInput;
struct NativeWaterOverlayInput;
struct NativeUnitEffectInput;
struct NativeReclaimFieldInput;
struct NativeDistortionCommand;
struct NativeObjectSpotlightInput;
struct NativeGrassInput;
struct NativeResourceSpotsInput;
struct NativeSensorLOSDraw;

// First four values deliberately match ScreenPrimitive, without extending
// the screen renderer's accepted primitives or its orthographic contract.
enum class NativeDrawPrimitive : std::uint8_t {
	Triangles, TriangleStrip, TriangleFan, Quads, Lines, LineStrip, LineLoop, Points, Polygon,
};
enum class NativeWorldDrawStage : std::uint8_t {
	PreUnit, World, WaterReflection, WaterRefraction, WaterPost, AlphaBelowWater, AlphaAboveWater, AlphaAll, ShadowTransparent, PreDecals, ShadowOpaque,
	Ground, TerrainReflection, // Append; retain all existing source-stage ordinals.
	ModelDeferred, // Phase-local model MRT clear only; never a generic world draw.
};
enum class NativeWorldCull : std::uint8_t { None, Front, Back, FrontAndBack };
enum class NativePolygonMode : std::uint8_t { Point, Line, Fill };
struct NativeWorldDrawState {
	bool depthTest = false;
	bool depthWrite = false;
	// Original GL_DEPTH_CLAMP keeps volume caps across the near/far planes.
	bool depthClamp = false;
	// Original draw intent; the paired target must separately retain its actual
	// startup-effective sample count. This flag does not create MSAA coverage.
	bool alphaToCoverage = false;
	ScreenAlphaCompare depthCompare = ScreenAlphaCompare::LessEqual;
	NativeWorldCull cull = NativeWorldCull::Back;
	bool cullEnabled = false;
	NativePolygonMode frontPolygonMode = NativePolygonMode::Fill;
	NativePolygonMode backPolygonMode = NativePolygonMode::Fill;
	bool polygonOffset = false;
	float offsetFactor = 0, offsetUnits = 0;
	bool stencilTest = false;
	NativeWorldStencilFace frontStencil, backStencil;
	std::array<bool,4> colorWrite{true,true,true,true};
	std::array<bool,4> combinedColorWrite{true,true,true,true}; // Original indexed Map color5 mask.
	// Original inherited engine plane2/3, expressed in homogeneous GL clip coordinates.
	std::optional<std::array<float,4>> inheritedClipPlane;
	float lineWidth = 1, pointSize = 1;
	bool lineStipple = false;
	std::uint16_t stipplePattern = 0xffff;
	std::uint16_t stippleFactor = 1;
	bool operator==(const NativeWorldDrawState&) const = default;
};

// Preserve homogeneous GL clip coordinates. GPU converts z to (z+w)/2;
// clipping and perspective interpolation happen before the viewport divide.
struct NativeWorldDrawVertex {
	std::array<float,4> position;
	ScreenColor color;
	ScreenTexCoord uv;
};
static_assert(std::is_standard_layout_v<NativeWorldDrawVertex>);
static_assert(sizeof(NativeWorldDrawVertex)==48);
static_assert(offsetof(NativeWorldDrawVertex,color)==16);
static_assert(offsetof(NativeWorldDrawVertex,uv)==32);

struct NativeWorldDrawCommand {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::World;
	NativeDrawPrimitive primitive = NativeDrawPrimitive::Triangles;
	NativeDrawPrimitive sourcePrimitive = NativeDrawPrimitive::Triangles;
	std::vector<NativeWorldDrawVertex> vertices;
	std::vector<std::uint32_t> indices;
	ScreenDrawState surface;
	NativeWorldDrawState world;
	// Original raw vertex/program snapshot for the selected shadow consumer.
	// The common recorder still owns the single command and global ordinal.
	std::shared_ptr<const NativeGenericShadowInput> originalShadowInput;
 std::shared_ptr<const NativeEasyFacingInput> easyFacing;
 std::shared_ptr<const NativeSquadHullInput> squadHull;
 std::shared_ptr<const NativeTerraformOverlayInput> terraformOverlay;
 std::shared_ptr<const NativeLOSViewInput> losView;
 std::shared_ptr<const NativeWaterOverlayInput> waterOverlay;
 std::shared_ptr<const NativeUnitEffectInput> unitEffect;
	std::shared_ptr<const NativeDistortionCommand> distortion; // Original private clear/volume/screen-copy/combine source operation.
	std::shared_ptr<const NativeObjectSpotlightInput> objectSpotlight;
	std::shared_ptr<const NativeGrassInput> grass;
 std::shared_ptr<const NativeResourceSpotsInput> resourceSpots;
 std::shared_ptr<const NativeSensorLOSDraw> sensorLOS;
	std::shared_ptr<const NativeReclaimFieldInput> reclaimField; // Actual family input retained by the common source lifetime.
};
// Scalar source event only. glClear ignores viewport, blend, depth/stencil
// tests and raster state; it retains attachment masks and actual scissor.
// ShadowOpaque and the actual water-ground invocation have ordered target
// consumers. Other stages must retain their existing selected source route.
struct NativeWorldAttachmentClear {
 std::uint64_t sequence=0;
 NativeWorldDrawStage stage=NativeWorldDrawStage::ShadowOpaque;
 ScreenAttachmentMask mask=static_cast<ScreenAttachmentMask>(0);
 ScreenClearValues values;
 std::array<bool,4> colorWrite{true,true,true,true};
	std::array<bool,4> combinedColorWrite{true,true,true,true}; // Original indexed Map color5 mask.
 bool depthWrite=false;
 // Original glClear uses the FRONT stencil write mask. Shadow has no
 // stencil attachment and rejects that bit; retain its actual scalar state.
 std::uint32_t frontStencilWriteMask=~0u;
 std::optional<ScreenScissor> scissor;
};
struct NativeWorldDrawFrame {
	std::uint64_t serial = 0;
	std::vector<NativeWorldDrawCommand> draws;
 std::vector<NativeWorldAttachmentClear> clears;
};
void ValidateNativeWorldDrawCommand(const NativeWorldDrawCommand& command);

} // namespace rendering
