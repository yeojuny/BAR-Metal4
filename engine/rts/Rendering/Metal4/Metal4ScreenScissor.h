/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "../Backend/ScreenDrawList.h"
#include <algorithm>

namespace rendering::metal4 {

struct FramebufferScissor {
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
};

// Empty result means skip this draw, never send an invalid zero/outside Metal
// scissor. Compute signed endpoints in int64 to include negative origins and
// uint32 extents without wrapping. Input origin is bottom-left; output top-left.
inline std::optional<FramebufferScissor> ClampScreenScissor(const std::optional<ScreenScissor>& source, uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0) return std::nullopt;
	if (!source) return FramebufferScissor{0, 0, width, height};
	const int64_t left = std::clamp<int64_t>(source->x, 0, width);
	const int64_t bottom = std::clamp<int64_t>(source->y, 0, height);
	const int64_t right = std::clamp<int64_t>(int64_t(source->x) + source->width, 0, width);
	const int64_t top = std::clamp<int64_t>(int64_t(source->y) + source->height, 0, height);
	if (right <= left || top <= bottom) return std::nullopt;
	return FramebufferScissor{uint32_t(left), uint32_t(height - top), uint32_t(right - left), uint32_t(top - bottom)};
}

} // namespace rendering::metal4
