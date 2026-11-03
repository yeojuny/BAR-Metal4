/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePlacementGridGPU.h"
namespace rendering::metal4 {
// Already-owned shared original R32F height texture; no second upload/cache.
struct PlacementGridHeight128 {std::shared_ptr<const WorldTerrainHeights> owner;__strong id<MTLTexture> texture=nil;};
}
