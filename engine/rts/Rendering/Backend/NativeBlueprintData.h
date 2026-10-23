/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/WorldFrameData.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace rendering {

enum class NativeBlueprintProgram : std::uint8_t { Outline, Unit, UnitShape, FeatureShape };
enum class NativeBlueprintObjectKind : std::uint8_t { Unit, UnitDefinition, FeatureDefinition };

// Exact GetStaticPieceModelTransform/ApplyTransform storage. Do not substitute
// a bind-pose matrix or animate a different piece: the paired shader reads the
// low eight bone-ID bits from the exact instance's original transform stream.
struct NativeBlueprintTransform {
	std::array<float,4> quaternion{0,0,0,1};
	std::array<float,4> translationScale{0,0,0,1};
};
static_assert(sizeof(NativeBlueprintTransform) == 32);

struct NativeBlueprintModelSubmission {
	NativeBlueprintObjectKind kind = NativeBlueprintObjectKind::UnitDefinition;
	int objectId = -1;
	std::uint32_t instanceIndex = 0;
	std::shared_ptr<const WorldModel> model;
	// Own the actual source values addressed by instData.x + low8(pieceID).
	// They are rebased to zero only in the native draw's private argument table.
	std::uint32_t sourceTransformBase = 0;
	std::vector<NativeBlueprintTransform> transforms;
	std::array<float,4> selectedTeamColor{};
};

struct NativeBlueprintView {
	std::array<float,16> viewProjection{};
	std::array<float,3> cameraPosition{};
	std::array<float,4> timeInfo{};
	ClipDepthConvention clipDepth = ClipDepthConvention::Unsupported;
};

struct NativeBlueprintOutlineDraw {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::PreUnit;
	NativeBufferSnapshotRef vertices, instances;
	std::uint32_t vertexCount = 5, firstVertex = 0, instanceCount = 0, firstInstance = 0;
	std::shared_ptr<const WorldTerrainHeights> heights;
	std::array<float,2> mapWorldSize{};
	NativeBlueprintView view;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};

struct NativeBlueprintModelDraw {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::PreUnit;
	NativeBlueprintProgram program = NativeBlueprintProgram::UnitShape;
	NativeBufferSnapshotRef instances;
	std::vector<NativeBlueprintModelSubmission> submissions;
	// UnitShapeTextures chooses a texture bucket separately from ModelsVBO.
	// Use the actual bound pair even when a model has a different own reference.
	// Attached source draws retain the handler's immutable objects and sampler
	// versions. The legacy pixel pair is only for an unattached source owner.
	std::shared_ptr<const NativeS3OTexturePair> modelTextures;
	std::array<std::shared_ptr<const WorldTexture>,2> textures;
	float iconDistance = 1;
	NativeBlueprintView view;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};

// glClear ignores the viewport, color/depth masks, alpha test, culling, blend
// and stencil comparison. It keeps only the framebuffer's bounds, scissor,
// the selected stencil clear value and the front stencil write mask.
struct NativeWorldStencilClear {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::PreUnit;
	std::uint8_t value = 0, writeMask = 0xff;
	std::optional<ScreenScissor> scissor;
};

struct NativeBlueprintFrame {
	std::vector<NativeBlueprintOutlineDraw> outlines;
	std::vector<NativeBlueprintModelDraw> models;
	std::vector<NativeWorldStencilClear> clears;
};

void ValidateNativeBlueprintOutline(const NativeBlueprintOutlineDraw& draw);
void ValidateNativeBlueprintModel(const NativeBlueprintModelDraw& draw);
void ValidateNativeWorldStencilClear(const NativeWorldStencilClear& clear);

// Source math used by CPU diagnostics and the clip/line-width preparation.
// These are not a replacement shader or a visual acceptance result.
std::array<float,2> NativeBlueprintHeightUV(std::array<float,2> position, std::array<float,2> mapWorldSize);
float NativeBlueprintHeightLOD0(const WorldTerrainHeights& heights, std::array<float,2> uv);
std::array<float,3> NativeBlueprintApplyTransform(const NativeBlueprintTransform& transform, std::array<float,3> position);
std::array<float,3> NativeBlueprintRotateAndPlace(std::array<float,3> local, std::array<float,4> worldPositionRotation);
std::array<float,4> NativeBlueprintModelColor(std::array<float,4> base, std::array<float,4> extra,
	std::array<float,4> team, std::array<float,4> parameters, float worldY, const std::array<float,4>& timeInfo);
std::uint8_t NativeBlueprintClearStencil(std::uint8_t previous, const NativeWorldStencilClear& clear) noexcept;

class NativeBlueprintRecorder {
public:
	void Begin();
	void Record(NativeBlueprintOutlineDraw draw);
	void Record(NativeBlueprintModelDraw draw);
	void Record(NativeWorldStencilClear clear);
	std::shared_ptr<const NativeBlueprintFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeBlueprintFrame> frame;
};

// A factory remains unavailable until the paired real GPU consumer is wired
// into both source stages and every required pipeline compiled successfully.
void SetNativeBlueprintConsumerAvailable(bool available) noexcept;
bool IsNativeBlueprintConsumerAvailable() noexcept;

} // namespace rendering
