/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeCubeTextureSource.h"
namespace rendering::metal4 {
// The same storage is exposed to the original named binding and to the real
// environment face pass. An allocation is not a rendered/cleared cube.
std::shared_ptr<const NativeCubeTextureResource> AllocateNativeCubeTexture(
 RuntimeLease&,NativeCubeTextureKind,uint32_t size,uint32_t levels);
id<MTLTexture> NativeCubeTexture(const NativeCubeTextureBacking&);
id<MTLTexture> NativeCubeDepth(const NativeCubeTextureBacking&);
class PreparedCubeTexture {
public:
 ~PreparedCubeTexture();
 void EncodeDependencies(id<MTL4CommandBuffer>);
 // Called only after the actual sky/terrain face encoder has ended.
 void FinishReflection(id<MTL4CommandBuffer>);
 void DidComplete();
 CUSSourceImage Image()const;
private:
 struct Impl;std::unique_ptr<Impl> impl;
 explicit PreparedCubeTexture(std::unique_ptr<Impl>);
 friend std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(RuntimeLease&,
  std::shared_ptr<const NativeCubeTextureBacking>,const NativeTextureSampling&,uint64_t,id<MTLSamplerState>);
};
std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(RuntimeLease&,
 std::shared_ptr<const NativeCubeTextureBacking>,const NativeTextureSampling&,uint64_t actualSubmission,
 id<MTLSamplerState> actualSampler=nil);
CUSSourceImage ResolveNativeCubeTexture(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission,
 id<MTLSamplerState> actualSampler=nil);
} // namespace rendering::metal4
