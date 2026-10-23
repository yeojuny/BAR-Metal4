/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4WorldViewport.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace rendering::metal4 {

// The donor copies camera-local logical texels. Independent native raster
// extents need an explicit nearest pixel-center adaptation before those same
// logical consumers run. Never stretch the floor/ceil-rounded raster scissor.
// Pixel centers and nearest tie-breaking are evaluated bottom-left, then
// converted to Metal top-down rows. This matters at 2x/4x exact boundaries.
struct LogicalCopyMapping {
	std::uint32_t width = 0, height = 0;
	std::array<std::uint32_t, 2> sourceExtent{};
	std::array<std::uint32_t, 4> crop{};
	std::uint64_t drawFrame = 0, attachmentGeneration = 0;
	std::uint32_t samples = 0;
	// GPU ABI: [destination width, x coordinates..., y coordinates...].
	// Coordinates address the full main read image, not the cropped scissor.
	std::vector<std::uint32_t> coordinates;
	bool unitScale = false;

	void RequireSource(std::uint64_t frame, std::uint64_t generation,
		std::uint32_t sourceWidth, std::uint32_t sourceHeight, std::uint32_t sourceSamples) const
	{
		if (frame != drawFrame || generation != attachmentGeneration ||
			sourceWidth != sourceExtent[0] || sourceHeight != sourceExtent[1] || sourceSamples != samples)
			throw std::runtime_error("Logical copy source frame/attachment generation/extent/samples changed");
	}
};

inline LogicalCopyMapping MakeLogicalCopyMapping(const std::array<std::int32_t, 4>& camera,
	std::uint32_t logicalWidth, std::uint32_t logicalHeight,
	std::uint32_t renderWidth, std::uint32_t renderHeight,
	const std::array<std::uint32_t, 4>& crop,
	std::uint64_t drawFrame, std::uint64_t attachmentGeneration, std::uint32_t samples)
{
	// Bound allocations and arithmetic independently of the device. A logical
	// copy cannot exceed the native 2D texture contract even when raster is small.
	constexpr std::uint32_t maximum = 16384;
	if (!logicalWidth || !logicalHeight || !renderWidth || !renderHeight ||
		logicalWidth > maximum || logicalHeight > maximum || renderWidth > maximum || renderHeight > maximum ||
		camera[2] <= 0 || camera[3] <= 0 || !attachmentGeneration || !samples)
		throw std::runtime_error("Logical copy requires bounded positive source and destination extents");
	if (camera[0] < 0 || camera[1] < 0 ||
		std::uint64_t(camera[0]) + camera[2] > logicalWidth ||
		std::uint64_t(camera[1]) + camera[3] > logicalHeight)
		throw std::runtime_error("Logical copy of a clipped camera requires original destination preservation semantics");
	if (!crop[2] || !crop[3] || crop[0] > renderWidth || crop[1] > renderHeight ||
		crop[2] > renderWidth - crop[0] || crop[3] > renderHeight - crop[1])
		throw std::runtime_error("Logical copy crop exceeds its actual raster source");

	const auto expected = MapWorldViewport(camera, logicalWidth, logicalHeight, renderWidth, renderHeight);
	if (!expected || expected->scissor != crop)
		throw std::runtime_error("Logical copy crop differs from the actual camera raster mapping");

	LogicalCopyMapping result;
	result.width = std::uint32_t(camera[2]); result.height = std::uint32_t(camera[3]);
	result.sourceExtent = {renderWidth, renderHeight}; result.crop = crop;
	result.drawFrame = drawFrame; result.attachmentGeneration = attachmentGeneration; result.samples = samples;
	result.unitScale = logicalWidth == renderWidth && logicalHeight == renderHeight;
	// Preserve the existing constant-size origin/blit path at exact 1:1.
	if (result.unitScale) return result;
	result.coordinates.reserve(1 + result.width + result.height);
	result.coordinates.push_back(result.width);
	const auto axis = [&](std::uint32_t start, std::uint32_t count, std::uint32_t logical,
		std::uint32_t raster, std::uint32_t cropStart, std::uint32_t cropSize, bool reverse) {
		for (std::uint32_t i = 0; i < count; ++i) {
			const auto logicalPixel = start + (reverse ? count - 1 - i : i);
			const auto nearest = std::uint32_t(((std::uint64_t(logicalPixel) * 2 + 1) * raster) / (std::uint64_t(logical) * 2));
			const auto value = reverse ? raster - 1 - nearest : nearest;
			if (value < cropStart || value - cropStart >= cropSize)
				throw std::runtime_error("Logical copy pixel center escapes the original raster crop");
			result.coordinates.push_back(value);
		}
	};
	axis(std::uint32_t(camera[0]), result.width, logicalWidth, renderWidth, crop[0], crop[2], false);
	axis(std::uint32_t(camera[1]), result.height, logicalHeight, renderHeight, crop[1], crop[3], true);
	return result;
}

} // namespace rendering::metal4
