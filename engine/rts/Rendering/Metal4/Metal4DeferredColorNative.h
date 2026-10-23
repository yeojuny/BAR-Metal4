/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Metal4ShieldNative.h"
#include "Rendering/Backend/NativeDeferredColorData.h"
namespace rendering::metal4 {
struct DeferredColorImage {
	id<MTLTexture> texture=nil;
	std::shared_ptr<const void> storageOwner;
	// SAME producer as actual models-only depth in the five-MRT render pass.
	std::shared_ptr<const ShieldDepthProducerState> producer;
	NativeDeferredColorMetadata metadata;
};
void FulfillDeferredColorBacking(const NativeDeferredColorBacking&,const DeferredColorImage&);
const DeferredColorImage& GetDeferredColorBackingImage(const NativeDeferredColorBacking&);
bool IsDeferredColorReadable(const DeferredColorImage&,uint64_t actualSubmission)noexcept;
// Consumes actual original texture/sampler state; the returned texture exposes
// original RGBA storage. CUS or other source sampler transports apply swizzle.
CUSSourceImage ResolveDeferredColorImage(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t actualSubmission);
} // namespace rendering::metal4
