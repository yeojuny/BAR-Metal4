/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4Pass160Policy.h"
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
 // Appended to preserve existing aggregate field order. A present callback
 // without an explicit producer query remains Unknown.
 SourceDependencyQuery160 dependencyQuery160;
};
inline id<MTLTexture> SourceTextureRoot160(id<MTLTexture> image) {
 while(image.parentTexture)image=image.parentTexture;
 return image;
}
inline SourceDependencyResult160 InspectSourceDependency160(const CUSSourceImage& image) {
 // No callback means there is no dependency invocation to omit. Readability
 // still runs at the original draw; this does not move the external producer.
 if(!image.encodeDependencies)return {SourceDependencyWork160::NoCommands,false};
 return image.dependencyQuery160.Inspect();
}
using CUSSourceImageProvider=std::function<CUSSourceImage(RuntimeLease&,const NativeCUSTextureBinding&,unsigned)>;
} // namespace rendering::metal4
