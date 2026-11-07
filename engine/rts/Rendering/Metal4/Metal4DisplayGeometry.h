/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>

namespace rendering::scene {
struct NativeRenderExtent {
	std::uint32_t width, height;
};

// Window, camera and input coordinates remain in their original logical units.
// Only an explicit fullscreen render request changes the surface's pixel extent.
constexpr NativeRenderExtent SelectNativeRenderExtent(std::uint32_t logicalWidth,
	std::uint32_t logicalHeight, bool fullscreen, bool borderless, int width, int height)
{
	if (fullscreen && !borderless && width > 0 && height > 0 && width <= 4096 && height <= 4096)
		return {std::uint32_t(width), std::uint32_t(height)};
	return {logicalWidth, logicalHeight};
}

struct NativeDisplayGeometry {
	bool available = false;
	std::uint32_t logicalWidth = 0, logicalHeight = 0;
	std::uint32_t renderWidth = 0, renderHeight = 0;
	std::uint32_t backingWidth = 0, backingHeight = 0;
};
// Read cached scalar surface state only; never retire GPU work or resolve images.
NativeDisplayGeometry GetNativeDisplayGeometry();
}
