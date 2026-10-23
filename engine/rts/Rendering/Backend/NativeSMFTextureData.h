/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "WorldFrameData.h"

namespace rendering {

// Same spatial sampling domain as CSMFGroundTextures::ExtractSquareTiles:
// 32 x 32 ordered SMT tiles, four original BC1 levels, no recompression.
inline constexpr uint32_t NATIVE_SMF_TILES_PER_SQUARE = 32;
inline constexpr uint32_t NATIVE_SMF_SQUARE_TEXELS = 1024;

inline void ValidateNativeSMFTiles(const WorldTerrainTiles& tiles)
{
	if (tiles.gridWidth == 0 || tiles.gridHeight == 0 || tiles.gridWidth % NATIVE_SMF_TILES_PER_SQUARE != 0 ||
		tiles.gridHeight % NATIVE_SMF_TILES_PER_SQUARE != 0 || tiles.gridWidth > 2048 || tiles.gridHeight > 2048 ||
		tiles.tileIndices.size() != size_t(tiles.gridWidth) * tiles.gridHeight || tiles.uniqueTileCount == 0 ||
		tiles.uniqueTileCount > 1'000'000 || tiles.bc1Bytes.size() != size_t(tiles.uniqueTileCount) * WorldTerrainTiles::BYTES_PER_TILE)
		throw std::runtime_error("Original spatial SMF square dimensions/payload are invalid");
	for (uint32_t index: tiles.tileIndices) {
		if (index >= tiles.uniqueTileCount) throw std::runtime_error("Original spatial SMF square tile index exceeds its source");
	}
}

// Caller validates the immutable owner once before assembling its squares.
inline std::array<WorldTextureMip, 4> AssembleNativeSMFSquare(const WorldTerrainTiles& tiles, uint32_t squareX, uint32_t squareZ)
{
	if (squareX >= tiles.gridWidth / NATIVE_SMF_TILES_PER_SQUARE || squareZ >= tiles.gridHeight / NATIVE_SMF_TILES_PER_SQUARE)
		throw std::runtime_error("Original spatial SMF square coordinate is outside the tile grid");
	std::array<WorldTextureMip, 4> output;
	for (size_t level = 0; level < output.size(); ++level) {
		auto& mip = output[level];
		mip.width = mip.height = NATIVE_SMF_SQUARE_TEXELS >> level;
		mip.bytesPerRow = size_t(mip.width / 4) * 8;
		mip.bytes.resize(mip.bytesPerRow * (mip.height / 4));
		const uint32_t blocksPerTile = 8u >> level;
		const size_t sourceRowBytes = size_t(blocksPerTile) * 8;
		for (uint32_t tileZ = 0; tileZ < NATIVE_SMF_TILES_PER_SQUARE; ++tileZ) {
			for (uint32_t tileX = 0; tileX < NATIVE_SMF_TILES_PER_SQUARE; ++tileX) {
				const size_t gridIndex = size_t(squareZ * NATIVE_SMF_TILES_PER_SQUARE + tileZ) * tiles.gridWidth + squareX * NATIVE_SMF_TILES_PER_SQUARE + tileX;
				const uint32_t tile = tiles.tileIndices.at(gridIndex);
				if (tile >= tiles.uniqueTileCount) throw std::runtime_error("Original spatial SMF square tile index exceeds its source");
				const size_t source = size_t(tile) * WorldTerrainTiles::BYTES_PER_TILE + WorldTerrainTiles::MIP_OFFSETS[level];
				if (source + sourceRowBytes * blocksPerTile > tiles.bc1Bytes.size()) throw std::runtime_error("Original spatial SMF square source is truncated");
				for (uint32_t row = 0; row < blocksPerTile; ++row) {
					const size_t destination = size_t(tileZ * blocksPerTile + row) * mip.bytesPerRow + size_t(tileX) * sourceRowBytes;
					std::memcpy(mip.bytes.data() + destination, tiles.bc1Bytes.data() + source + size_t(row) * sourceRowBytes, sourceRowBytes);
				}
			}
		}
	}
	return output;
}

// Metal's sampler descriptor represents an integer tap limit. Preserve the
// requested float in the frame; round a non-integral limit upward after the
// original <1 disables / physical maximum clamp rules. No quality reduction.
inline uint32_t NativeSMFAnisotropy(float requested)
{
	if (!std::isfinite(requested) || requested < 0) throw std::runtime_error("Invalid original SMF anisotropy request");
	// Apple Metal feature tables: maximum sampler anisotropy is 16 for Metal4
	// Apple GPUs. The runtime already requires that actual supported family.
	return requested < 1 ? 1 : static_cast<uint32_t>(std::ceil(std::min(requested, 16.0f)));
}

} // namespace rendering
