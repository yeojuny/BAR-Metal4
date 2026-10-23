/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

class CSMFMapFile;

namespace smf {

enum class MissingTilePolicy { LegacyRed, Fail };

struct MissingTileFile {
	uint32_t index = 0;
	std::string path;
	uint32_t tileCount = 0;
};

struct TileData {
	uint32_t gridWidth = 0;
	uint32_t gridHeight = 0;
	uint32_t uniqueTileCount = 0;
	std::vector<uint32_t> tileIndices;
	std::vector<uint8_t> bc1Bytes;
	std::vector<std::string> sourcePaths;
	std::vector<MissingTileFile> missingFiles;
	uint32_t declaredFileCount = 0;
	bool ignoredFilenameOverrides = false;
};

// Shared CPU loader for the original ordered BC1 SMT tiles and four mips.
// No texture creation or GL state. LegacyRed retains the original 0xAA fallback;
// a native scene uses Fail so missing assets cannot masquerade as real terrain.
TileData LoadTileData(CSMFMapFile& mapFile, const std::string& mapPath,
	std::span<const std::string> filenameOverrides, MissingTilePolicy missingPolicy);

} // namespace smf
