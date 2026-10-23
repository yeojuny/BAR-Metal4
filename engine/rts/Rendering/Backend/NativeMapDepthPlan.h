/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering {
enum class NativeMapDepthAction : std::uint8_t { Retain, ClearOne, DrawTerrain };
enum class NativeMapDepthPass : std::uint8_t { Main, WaterReflection, WaterRefraction, TerrainReflection };
// Actual source branch observations, not inferred generic shader capability.
// geometryBufferValid describes this concrete native GBuffer allocation path;
// requestedDeferred retains original init/SetDrawGroundDeferred state separately
// from the old GL-only FBO validity bit.
struct NativeMapDepthSelection {
	bool drawGround=true, onlyVoidWater=false;
	bool requestedDeferred=true, geometryBufferValid=false;
	bool selectedDeferredShaderValid=false, advancedShading=true;
	// Independent actual Create event: an invalid/absent FBO is not permission
	// to invent a new named texture or change an existing binding.
	bool namedDepthStorageExists=false;
	std::uint32_t storageWidth=0,storageHeight=0; // Actual GeometryBuffer WantedSize/Create observation.
	NativeMapDepthPass pass=NativeMapDepthPass::Main;
};
NativeMapDepthAction SelectNativeMapDepthAction(const NativeMapDepthSelection&);
} // namespace rendering
