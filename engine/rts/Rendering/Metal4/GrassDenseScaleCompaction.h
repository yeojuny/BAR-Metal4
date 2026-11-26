#pragma once
#include "GrassZeroScaleCompaction.h"

namespace rendering::metal4 {
class GrassDenseScaleIndex {
public:
	bool Repacked() const noexcept { return !removedBefore.empty(); }

	GrassInstanceRange Remap(std::size_t base, std::size_t count) const
	{
		if (base > rows || count > rows - base)
			throw std::out_of_range("Grass compacted instance range exceeds the source snapshot");
		if (!Repacked()) return {base, count};
		const std::size_t mappedBase = base - removedBefore[base];
		const std::size_t end = base + count;
		return {mappedBase, end - removedBefore[end] - mappedBase};
	}

	std::vector<GrassInstanceRow> Build(std::span<const std::uint8_t> bytes)
	{
		constexpr std::size_t stride = sizeof(GrassInstanceRow);
		if (bytes.size() % stride != 0)
			throw std::invalid_argument("Grass instance snapshot contains a partial float4 row");
		rows = bytes.size() / stride;
		removedBefore.clear();
		std::vector<GrassInstanceRow> packed;
		std::size_t removed = 0;
		for (std::size_t i = 0; i < rows; ++i) {
			const std::uint8_t* row = bytes.data() + i * stride;
			std::uint32_t scaleBits;
			std::memcpy(&scaleBits, row + 12, sizeof(scaleBits));
			if ((scaleBits & 0x7fff'ffffu) == 0) {
				if (!removed) {
					// No allocations or bulk copy for the no-zero case. Earlier rows
					// were already inspected, so copy that initial run in one operation.
					removedBefore.resize(rows + 1);
					packed.reserve(rows - 1);
					packed.resize(i);
					if (i) std::memcpy(packed.data(), bytes.data(), i * stride);
				}
				++removed;
			} else if (removed) {
				GrassInstanceRow entry;
				std::memcpy(entry.data(), row, stride);
				packed.push_back(entry);
			}
			if (removed) removedBefore[i + 1] = removed;
		}
		return packed;
	}

private:
	std::size_t rows = 0;
	std::vector<std::size_t> removedBefore;
};
}
