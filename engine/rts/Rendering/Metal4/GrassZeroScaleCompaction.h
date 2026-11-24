/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <bit>
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


struct GrassSpanPlan {
	std::array<GrassInstanceRange, 3> spans{};
	std::size_t count = 0;
	std::span<const GrassInstanceRange> Ranges() const { return {spans.data(), count}; }
};

struct GrassSpanPolicy {
	std::size_t blockSize;
	std::size_t zeroHaloBlocks;
	bool skipLeading;
	std::size_t maxInteriorGaps;
};

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


	GrassSpanPlan PlanEmptySpans(GrassInstanceRange original, std::size_t blockSize, bool skipLeading) const
	{
		return PlanEmptySpans(original, {blockSize, 1, skipLeading, 2});
	}

	GrassSpanPlan PlanEmptySpans(GrassInstanceRange original, GrassSpanPolicy policy) const
	{
		const auto trimmed = TrimTrailing(original);
		const auto block = policy.blockSize;
		const auto halo = policy.zeroHaloBlocks;
		const auto maxInteriorGaps = policy.maxInteriorGaps;
		const GrassSpanPlan fallback{{trimmed}, 1};
		if (!std::has_single_bit(block) || block < 8 || block > 512 || halo > 1 || maxInteriorGaps > 2)
			return fallback;

		const auto minimumGap = std::max(std::size_t{128}, 2 * block);
		if (trimmed.count <= minimumGap)
			return fallback;

		const auto first = std::lower_bound(potentiallyLive.begin(), potentiallyLive.end(), trimmed.base);
		const auto end = std::lower_bound(first, potentiallyLive.end(), trimmed.base + trimmed.count);
		const auto occupiedBlock = [&](std::size_t position) { return (position - trimmed.base) / block; };
		const auto firstBlock = occupiedBlock(*first);
		const auto prefix = firstBlock >= halo ? (firstBlock - halo) * block : 0;
		std::size_t begin = policy.skipLeading && prefix >= minimumGap ? prefix : 0;

		// Select the most profitable interior gaps without adding more than two draws.
		// Both cut points are aligned relative to the original draw base. Preserve a
		// full empty block on either side when requested to keep the live neighborhoods.
		std::array<GrassInstanceRange, 2> gaps{};
		for (auto previous = first, next = first + 1; next != end && maxInteriorGaps; ++previous, ++next) {
			const auto previousBlock = occupiedBlock(*previous);
			const auto nextBlock = occupiedBlock(*next);
			if (nextBlock <= previousBlock + 1 + 2 * halo)
				continue;
			const auto left = (previousBlock + 1 + halo) * block;
			const auto right = (nextBlock - halo) * block;
			if (right - left < minimumGap)
				continue;
			GrassInstanceRange gap{left, right - left};
			for (std::size_t i = 0; i < maxInteriorGaps; ++i) {
				if (gap.count > gaps[i].count || (gap.count && gap.count == gaps[i].count && gap.base < gaps[i].base))
					std::swap(gap, gaps[i]);
			}
		}
		std::ranges::sort(gaps, {}, &GrassInstanceRange::base);
		GrassSpanPlan plan;
		for (const auto gap : gaps) {
			if (!gap.count)
				continue;
			plan.spans[plan.count++] = {trimmed.base + begin, gap.base - begin};
			begin = gap.base + gap.count;
		}
		plan.spans[plan.count++] = {trimmed.base + begin, trimmed.count - begin};
		return plan;
	}

private:
	std::size_t rows = 0;
	std::vector<std::size_t> potentiallyLive;
};
}
