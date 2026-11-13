/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <span>
namespace rendering {
struct NativeWorldRasterMesh {
	std::vector<NativeWorldDrawVertex> vertices;
	std::vector<std::uint32_t> indices;
	// Optional physical point center.xy, radius and signed sample count.
	// Positive count: disk; negative: square; zero: unmasked fill/line span.
	// Empty for geometry that needs no explicit point coverage.
	std::vector<std::array<float,4>> pointDisks;
	bool fullViewport = false; // Expanded MSAA coverage is in full attachment clip coordinates.
};
// Pure fill clipping kernel for already validated immutable input. Preconditions:
// indices.size()%3==0 (including empty) and every index addresses vertices;
// attributes and plane are
// finite. Callers retain span owners through completion. No validation, provider,
// diagnostics, TLS or scheduling is performed here; errors remain at the caller.
NativeWorldRasterMesh BuildValidatedNativeWorldClippedFill(std::span<const NativeWorldDrawVertex> vertices,
 std::span<const uint32_t> indices,std::array<float,4> plane);
// Structural closure only, not validation or permission to skip caller checks.
inline bool IsIndependentNativeWorldClippedFill(const NativeWorldDrawCommand& d) noexcept
{
 const auto& s=d.surface;
 return !(d.primitive!=NativeDrawPrimitive::Triangles||d.sourcePrimitive!=NativeDrawPrimitive::Triangles||
   d.world.frontPolygonMode!=NativePolygonMode::Fill||d.world.backPolygonMode!=NativePolygonMode::Fill||!d.world.inheritedClipPlane||
   s.material!=ScreenMaterial::Solid||s.texture||s.secondary.texture||s.minimapBackground||
   s.terraformCapture||s.diffusePainter||s.splatPainter||s.decalCapture||s.deferredVisualizer||
   d.originalShadowInput||d.easyFacing||d.squadHull||d.terraformOverlay||d.losView||d.waterOverlay||d.unitEffect||d.unitGlass||
   d.distortion||d.objectSpotlight||d.grass||d.resourceSpots||d.sensorLOS||d.sourceLines||d.reclaimField);
}
// Original geometry validation prefix, shared with the narrow caller-thread seal.
// Does not itself establish eligibility; generic wrapper still rejects GPU matrices.
void ValidateNativeWorldGeometryPrefix(const NativeWorldDrawCommand& draw,
 const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned samples);
// Raster width is explicit framebuffer pixels. Camera/SDL logical coordinates
// have already been mapped to this viewport by the owning native world pass.
// inheritedClipPlane retains dot(plane, GL homogeneous position) >= 0. It clips
// original primitives before point/line expansion and preserves polygon edges.
NativeWorldRasterMesh BuildNativeWorldDrawGeometry(const NativeWorldDrawCommand& draw,
	const std::array<double,4>& viewport,std::uint32_t width,std::uint32_t height,unsigned samples=1);
}
