/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "ScreenDrawList.h"
#include <algorithm>

namespace rendering {
struct WorldTerrainTiles;

// Definitions belong to one immutable contents version of the existing name.
// Keep the exact old owner/subresource even when a new level0 makes its mip
// chain incomplete. A later write must not discard those other definitions.
struct ScreenTextureLevelDefinition {
	std::uint32_t width = 0, height = 0, originalInternalFormat = 0;
	bool defined = false, allocatedUndefined = false;
	std::shared_ptr<const ScreenTexture> owner;
	std::uint32_t ownerLevel = 0;
	// Actual compressed source provenance; no texture cache or decoder here.
	std::shared_ptr<const WorldTerrainTiles> terrainTiles;
	std::uint32_t squareX = 0, squareZ = 0, sourceMip = 0;
};
struct ScreenTextureLevelDefinitions {
	std::uint64_t contextGeneration = 0, objectGeneration = 0;
	std::vector<ScreenTextureLevelDefinition> levels;
};
inline bool NativeUITextureLevelsComplete(const ScreenTextureLevelDefinitions& storage,
	bool mipmapped, int baseLevel = 0, int maxLevel = 1000)
{
	if (baseLevel < 0 || maxLevel < baseLevel || size_t(baseLevel) >= storage.levels.size()) return false;
	const auto& base = storage.levels[baseLevel];
	if (!base.defined || !base.width || !base.height) return false;
	if (!mipmapped) return true;
	unsigned width = base.width, height = base.height;
	for (unsigned level = unsigned(baseLevel); ; ++level) {
		if (level >= storage.levels.size()) return false;
		const auto& mip = storage.levels[level];
		if (!mip.defined || mip.width != width || mip.height != height || mip.originalInternalFormat != base.originalInternalFormat)
			return false;
		if (level == unsigned(maxLevel) || (width == 1 && height == 1)) return true;
		width = std::max(1u,width/2); height = std::max(1u,height/2);
	}
}

// The writer forms only the contiguous compatible CPU chain in image->mips.
// Sampling with a mip filter requires every level through the original q.
// Non-mip sampling still reads the real base when other definitions differ.
inline std::shared_ptr<const ScreenTexture> SelectNativeUITextureSample(
	const std::shared_ptr<const ScreenTexture>& image, const ScreenSampler& sampler, int maxLevel = 1000)
{
	if (!image || !image->levelDefinitions || image->incomplete || sampler.mipFilter == ScreenMipFilter::None)
		return image;
	if (NativeUITextureLevelsComplete(*image->levelDefinitions,true,0,maxLevel)) return image;
	auto incomplete = std::make_shared<ScreenTexture>();
	incomplete->sourceName = image->sourceName;
	incomplete->generation = image->generation;
	incomplete->incomplete = true;
	incomplete->levelDefinitions = image->levelDefinitions;
	return incomplete;
}
} // namespace rendering
