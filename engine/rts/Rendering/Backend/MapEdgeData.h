/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSUniformData.h"
#include "NativeCUSTextureHistory.h"
#include "NativeWorldDrawData.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace rendering {struct NativeCUSDeferredPhase;}
namespace rendering::mapedge {

using Mirror = std::array<float,4>; // Exact original flipX,flipZ,offsetX,offsetZ.
struct Parameters {
	float gridSize = 32;
	float brightness = .3f; // Original brightness multiplied by current nightFactor.
	bool curvature = true;
	bool fog = true;
	bool cullBackFaces = true; // Actual original forward state, not device inference.
	std::array<std::uint32_t,5> mesh{}; // Actual cols,rows,trimCols,trimRows,vertexCount from current31657.
	bool deferred = false; // Original DrawGroundDeferred uses grid64 and tightening0.5.
};
struct DrawPacket {
	std::uint64_t sequence = 0; // Actual original callin order among native families.
	Parameters parameters;
	std::vector<Mirror> mirrors; // Original current31657 order; no zero seam instance.
	std::uint64_t sourceDrawFrame = 0;
	// Deferred only: retained by the actual Ground invocation, never the later
	// PreUnit singleton. The original callback observes its existing mirrors.
	std::shared_ptr<const NativeCUSDeferredPhase> mapMRT;
	std::shared_ptr<const NativeCUSUniformPublication> original;
	std::array<NativeCUSTextureBinding,3> images; // colorTex, heightTex, mapNormalTex
	NativeWorldDrawState raster;
	ScreenDrawState surface;
};

// Main-thread unsynced recording only; never alters simulation. Frame reset
// discards the previous command so disabled/removed/failed widgets cannot leave
// a stale border visible. BeginFrame must run even when no widget submits.
void BeginFrame();
// Wrap only the original eventHandler.DrawWorldPreUnit call. Destruction closes
// the stage on exceptions. Submit outside this stage throws std::runtime_error.
class WorldPreUnitScope {
public:
	WorldPreUnitScope();
	~WorldPreUnitScope();
	WorldPreUnitScope(const WorldPreUnitScope&) = delete;
	WorldPreUnitScope& operator=(const WorldPreUnitScope&) = delete;
};
// Dedicated native family operation, not a GLSL/VBO/VAO capability. Lua bridge:
// NativeMapEdgeSubmit(flatMirrorTable, gridSize, effectiveBrightness, curvature,
//                     fogEffect, cullBackFaces, meshParameters) -> no Lua values.
// Flat table has 0..32 finite numbers, grouped as exact original four-tuples.
// Nonempty lists contain 1..8 unique mirrors. meshParameters is the original five-value tuple.
// Validation completes before publication; invalid input throws and preserves
// the previous valid submission. Multiple submissions in one frame are errors.
void Submit(Parameters parameters, std::span<const Mirror> mirrors);
// Shared validation for both Lua publication and external immutable GPU input.
void Validate(Parameters parameters, std::span<const Mirror> mirrors);
// Borrowed through an owned immutable shared snapshot. Capture after the stage
// closes, in the same frame/camera as terrain capture. Empty means no draw.
std::shared_ptr<const DrawPacket> Snapshot();

} // namespace rendering::mapedge
