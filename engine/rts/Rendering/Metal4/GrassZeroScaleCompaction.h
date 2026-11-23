/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace rendering::metal4 {
struct GrassInstanceRange {
	std::size_t base = 0;
	std::size_t count = 0;
};

using GrassInstanceRow = std::array<std::uint8_t, 16>;

class GrassZeroScaleIndex {
public:
	void Build(std::span<const std::uint8_t> bytes)
	{
		constexpr auto stride = sizeof(GrassInstanceRow);
		if (bytes.size() % stride != 0)
			throw std::invalid_argument("Grass instance snapshot contains a partial float4 row");

		rows = bytes.size() / stride;
		potentiallyLive.clear();
		for (std::size_t row = 0; row < rows; ++row) {
			std::uint32_t scaleBits;
			std::memcpy(&scaleBits, bytes.data() + row * stride + 12, sizeof(scaleBits));
			if ((scaleBits & 0x7fff'ffffu) != 0)
				potentiallyLive.push_back(row);
		}
	}

	GrassInstanceRange TrimTrailing(GrassInstanceRange range) const
	{
		if (range.base > rows || range.count > rows - range.base)
			throw std::out_of_range("Grass instance range exceeds the source snapshot");

		// Retain the original buffer, baseInstance and relative slot of every
		// live row. Removing leading or interior zero rows can change raster output.
		const auto end = std::lower_bound(potentiallyLive.begin(), potentiallyLive.end(), range.base + range.count);
		if (end == potentiallyLive.begin() || *(end - 1) < range.base)
			return {range.base, 0};
		return {range.base, *(end - 1) - range.base + 1};
	}

private:
	std::size_t rows = 0;
	std::vector<std::size_t> potentiallyLive;
};
}
