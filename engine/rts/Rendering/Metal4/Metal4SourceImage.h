/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeCUSTextureHistory.h"
#include <functional>
namespace rendering::metal4 {
struct CUSSourceImage {
 __strong id<MTLTexture> texture=nil;
 __strong id<MTLSamplerState> sampler=nil;
 std::shared_ptr<const void> owner;
 bool flipV=false,depthImage=false;
 // Texture views expose the original storage channels before GL swizzle and
 // legacy depth mode. The generated sampler adapter applies those exactly once.
 // Shared owners enforce their actual producer, original sampling params,
 // format and immutable version. Neither callback may submit or wait itself.
 std::function<void(id<MTL4CommandBuffer>)> encodeDependencies;
 std::function<void()> requireReadable;
 std::function<void()> didComplete;
 // Immutable logical fixed-depth request semantics, independent of physical
 // floating-point storage. Only comparison references use this flag.
 bool clampDepthCompare=false;
};
using CUSSourceImageProvider=std::function<CUSSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,unsigned)>;
} // namespace rendering::metal4
