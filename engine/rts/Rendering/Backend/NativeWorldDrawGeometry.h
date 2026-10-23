/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
namespace rendering {
struct NativeWorldRasterMesh {
	std::vector<NativeWorldDrawVertex> vertices;
	std::vector<std::uint32_t> indices;
	bool fullViewport = false; // Expanded MSAA coverage is in full attachment clip coordinates.
};
// Raster width is explicit framebuffer pixels. Camera/SDL logical coordinates
// have already been mapped to this viewport by the owning native world pass.
// inheritedClipPlane retains dot(plane, GL homogeneous position) >= 0. It clips
// original primitives before point/line expansion and preserves polygon edges.
NativeWorldRasterMesh BuildNativeWorldDrawGeometry(const NativeWorldDrawCommand& draw,
	const std::array<double,4>& viewport,std::uint32_t width,std::uint32_t height,unsigned samples=1);
}
