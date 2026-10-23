/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"

namespace rendering {struct WorldTerrainTiles;}
namespace rendering::metal4 {
// The caller supplies MapCache::owner and its actual uploaded TileBatch::texture
// from the SAME committed map cache. firstSquareIndex is batchIndex*2048.
// UploadNativeSMFSquares must already have completed every Shared replaceRegion
// before MapCache commits that owner/batch. This resolver performs no upload,
// decoding, pixel allocation, producer execution, geometry work or caching.
// The caller supplies the sampler for the exact original source state and keeps
// the returned owner in the enclosing graph through common GPU retirement.
CUSSourceImage ResolveNativeTerrainSquareImage(RuntimeLease&,const NativeCUSTextureBinding&,
	std::shared_ptr<const WorldTerrainTiles> actualCachedOwner,id<MTLTexture> actualBatchArray,
	uint32_t firstSquareIndex,id<MTLSamplerState> actualSampler);
} // namespace rendering::metal4
