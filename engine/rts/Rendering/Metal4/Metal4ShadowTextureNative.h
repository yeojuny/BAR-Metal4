/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeShadowTextureSource.h"

namespace rendering::metal4 {
enum class NativeShadowDepthComponent { UNorm, Float };
struct NativeShadowDepthStorage {
 uint32_t requestedGLInternalFormat=0;
 MTLPixelFormat actualMetalPixelFormat=MTLPixelFormatInvalid;
 NativeShadowDepthComponent actualComponentType=NativeShadowDepthComponent::UNorm;
 unsigned actualComponentStorageBits=0;
 bool exactRequestedFixedPointEncoding=false;
};
NativeShadowDepthStorage QueryNativeShadowDepthStorage(const NativeShadowTextureBacking&);
std::shared_ptr<const NativeShadowTextureResource> AllocateNativeShadowTexture(RuntimeLease&,const NativeShadowTextureRequest&);
id<MTLTexture> NativeShadowTextureImage(const NativeShadowTextureBacking&);
bool CheckNativeShadowFramebuffer(const NativeShadowTextureBacking&,const NativeShadowTextureBacking&);
// Common producer invokes these at the actual original shadow passes and
// after the existing shared submission retirement. No function submits.
void MarkNativeShadowTextureEncoded(const NativeShadowTextureBacking&,uint64_t actualSubmission);
void CompleteNativeShadowTexture(const NativeShadowTextureBacking&);
void EncodeNativeShadowInitialClear(const std::shared_ptr<const NativeShadowTextureBacking>&,
	id<MTL4CommandBuffer>,uint64_t actualSubmission);
CUSSourceImage ResolveNativeShadowTexture(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission,
 id<MTLSamplerState> actualSampler=nil);
} // namespace rendering::metal4
