/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace rendering::metal4 {

struct WorldRasterViewport {
	std::array<double, 4> viewport{};
	std::array<uint32_t, 4> scissor{};
};

// Camera is bottom-left logical SDL coordinates; target is explicit pixels.
// No backing dimensions or content scale enter this mapping. Viewport remains
// unmodified; only its framebuffer scissor is clamped. Null is fully offscreen.
inline std::optional<WorldRasterViewport> MapWorldViewport(const std::array<int32_t, 4>& camera,
	uint32_t logicalWidth, uint32_t logicalHeight, uint32_t renderWidth, uint32_t renderHeight)
{
	if (!logicalWidth || !logicalHeight || !renderWidth || !renderHeight || camera[2] <= 0 || camera[3] <= 0)
		throw std::runtime_error("World viewport requires positive logical and explicit render extents");
	const int64_t left = camera[0], bottom = camera[1];
	const int64_t right = left + int64_t(camera[2]), top = bottom + int64_t(camera[3]);
	const double scaleX = double(renderWidth) / logicalWidth, scaleY = double(renderHeight) / logicalHeight;
	WorldRasterViewport result;
	result.viewport = {double(left) * scaleX, (double(logicalHeight) - double(top)) * scaleY,
		double(camera[2]) * scaleX, double(camera[3]) * scaleY};
	const int64_t clippedLeft = std::clamp(left, int64_t(0), int64_t(logicalWidth));
	const int64_t clippedRight = std::clamp(right, int64_t(0), int64_t(logicalWidth));
	const int64_t clippedBottom = std::clamp(bottom, int64_t(0), int64_t(logicalHeight));
	const int64_t clippedTop = std::clamp(top, int64_t(0), int64_t(logicalHeight));
	if (clippedRight <= clippedLeft || clippedTop <= clippedBottom) return std::nullopt;
	const uint32_t x0 = uint32_t(std::floor(double(clippedLeft) * scaleX));
	const uint32_t x1 = std::min(renderWidth, uint32_t(std::ceil(double(clippedRight) * scaleX)));
	const uint32_t y0 = uint32_t(std::floor((double(logicalHeight) - double(clippedTop)) * scaleY));
	const uint32_t y1 = std::min(renderHeight, uint32_t(std::ceil((double(logicalHeight) - double(clippedBottom)) * scaleY)));
	result.scissor = {x0, y0, x1 - x0, y1 - y0};
	return result;
}

} // namespace rendering::metal4
