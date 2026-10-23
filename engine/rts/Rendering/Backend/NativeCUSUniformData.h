/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <span>

namespace rendering {

// The selected game's LuaShader declares 255 colors. The actual original
// engine UBO has 2048 rows at the same offset; preserve its backing/layout.
inline constexpr size_t NativeCUSMatrixBytes = 26 * 64;
inline constexpr size_t NativeCUSParamsPrefixBytes = 18 * 16;
inline constexpr size_t NativeCUSTeamRows = 2048;
inline constexpr size_t NativeCUSDeclaredTeamRows = 255;
inline constexpr size_t NativeCUSParamsBytes = NativeCUSParamsPrefixBytes + NativeCUSTeamRows * 16;

struct NativeCUSMatrixPublication {
	uint64_t contextGeneration = 0, sequence = 0, sourceDrawFrame = 0;
	std::array<uint8_t, NativeCUSMatrixBytes> bytes{};
};

struct NativeCUSParamsPublication {
	uint64_t contextGeneration = 0, sequence = 0, sourceDrawFrame = 0;
	std::array<uint8_t, NativeCUSParamsBytes> bytes{};
	// Zero backing is not a source value. Original UpdateParams leaves inactive
	// rows untouched; a row becomes defined only after its actual producer writes.
	std::bitset<NativeCUSTeamRows> writtenTeamRows;
};

struct NativeCUSUniformPublication {
	std::shared_ptr<const NativeCUSMatrixPublication> matrices;
	std::shared_ptr<const NativeCUSParamsPublication> params;
};

class NativeCUSUniformStorage {
public:
	void NewContext();
	void PublishMatrices(uint64_t drawFrame, std::span<const uint8_t> actualBytes);
	void PublishParams(uint64_t drawFrame, std::span<const uint8_t> actualBytes,
		std::span<const uint16_t> actualWrittenTeamRows);
	std::shared_ptr<const NativeCUSUniformPublication> Current() const { return current; }
private:
	uint64_t generation = 0, sequence = 0;
	std::shared_ptr<const NativeCUSUniformPublication> current;
};

void ValidateNativeCUSUniformPublication(const NativeCUSUniformPublication&);
std::array<uint8_t, 16> ReadNativeCUSTeamColor(const NativeCUSUniformPublication&, uint32_t row);

} // namespace rendering
