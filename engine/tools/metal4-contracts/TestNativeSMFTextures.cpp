/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeSMFTextureData.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <span>

using namespace rendering;
namespace {
uint64_t checks = 0;
void Check(bool value, const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
template<class F> void Reject(F function) { bool rejected = false; try { function(); } catch (const std::exception&) { rejected = true; } Check(rejected, "Invalid original square input was accepted"); }
template<class T> std::vector<T> Read(const char* path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate); Check(bool(file), "Actual original fixture input is missing");
	const auto count = file.tellg(); Check(count > 0 && size_t(count) % sizeof(T) == 0, "Invalid fixture byte extent");
	std::vector<T> output(size_t(count) / sizeof(T)); file.seekg(0); file.read(reinterpret_cast<char*>(output.data()), count);
	Check(bool(file), "Actual original fixture input is truncated"); return output;
}

// The build supplies an unmodified extraction of the pinned original function.
// These test-only names satisfy its member references without any GL runtime.
constexpr uint32_t SQUARE_SIZE = 8;
constexpr size_t SMALL_TILE_SIZE = 680;
using GLint = int32_t;
#define RECOIL_DETAILED_TRACY_ZONE
struct CSMFGroundTextures {
	struct Map { uint32_t tileMapSizeX; };
	Map* smfMap;
	std::vector<uint32_t> tileMap;
	std::vector<uint8_t> tiles;
	void ExtractSquareTiles(int, int, int, GLint*) const;
};
#include NATIVE_SMF_EXTRACT_REFERENCE
}

int main(int argc, char** argv)
{
	try {
		Check(argc == 3, "Expected original Jade tile-grid and SMT payload paths");
		WorldTerrainTiles data; data.gridWidth = data.gridHeight = 512;
		data.tileIndices = Read<uint32_t>(argv[1]); data.bc1Bytes = Read<uint8_t>(argv[2]);
		data.uniqueTileCount = uint32_t(data.bc1Bytes.size() / WorldTerrainTiles::BYTES_PER_TILE);
		ValidateNativeSMFTiles(data);
		CSMFGroundTextures::Map map{data.gridWidth}; CSMFGroundTextures original{&map, data.tileIndices, data.bc1Bytes};
		for (uint32_t z = 0; z < 16; ++z) for (uint32_t x = 0; x < 16; ++x) {
			const auto actual = AssembleNativeSMFSquare(data, x, z);
			for (uint32_t level = 0; level < 4; ++level) {
				const uint32_t side = 1024u >> level;
				std::vector<GLint> expected(size_t(side) * side / 8);
				original.ExtractSquareTiles(int(x), int(z), int(level), expected.data());
				const auto bytes = std::as_bytes(std::span(expected));
				Check(actual[level].width == side && actual[level].height == side && actual[level].bytesPerRow == side * 2, "Original square mip geometry changed");
				Check(bytes.size() == actual[level].bytes.size(), "Original square mip byte count changed");
				for (size_t i = 0; i < bytes.size(); ++i) Check(uint8_t(bytes[i]) == actual[level].bytes[i], "Original spatial BC1 block order/mip bytes changed");
			}
		}
		Reject([&]{ AssembleNativeSMFSquare(data, 16, 0); });
		Reject([&]{ AssembleNativeSMFSquare(data, 0, 16); });
		WorldTerrainTiles bad; bad.gridWidth = 31; bad.gridHeight = 32; Reject([&]{ ValidateNativeSMFTiles(bad); });
		bad.gridWidth = bad.gridHeight = 32; bad.uniqueTileCount = 1; bad.tileIndices.resize(1024); bad.bc1Bytes.resize(680);
		ValidateNativeSMFTiles(bad); bad.tileIndices.back() = 1; Reject([&]{ ValidateNativeSMFTiles(bad); });
		bad.tileIndices.back() = 0; bad.bc1Bytes.pop_back(); Reject([&]{ ValidateNativeSMFTiles(bad); });
		for (const auto [input, expected]: {std::pair{0.f,1u}, {.5f,1u}, {1.f,1u}, {4.f,4u}, {3.25f,4u}, {16.f,16u}, {64.f,16u}})
			Check(NativeSMFAnisotropy(input) == expected, "Original anisotropy clamp/native integer realization changed");
		Reject([]{ NativeSMFAnisotropy(-1); }); Reject([]{ NativeSMFAnisotropy(std::numeric_limits<float>::infinity()); });
		Reject([]{ NativeSMFAnisotropy(std::numeric_limits<float>::quiet_NaN()); });
		std::cout << "PASS checks=" << checks << " actualJadeSquares=256 suppliedMips=4 originalExtractSquareTiles=verbatim allBytesCompared\n";
		return 0;
	} catch (const std::exception& error) { std::cerr << "FAIL checks=" << checks << " " << error.what() << '\n'; return 1; }
}
