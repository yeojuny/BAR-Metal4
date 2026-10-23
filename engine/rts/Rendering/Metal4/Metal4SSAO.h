/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4SSAO.h is a private Objective-C++ renderer interface"
#endif
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeSSAOData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <functional>
#include "Rendering/Backend/NativeDeferredPostprocessEpoch.h"
namespace rendering::metal4 {
struct ResolvedSSAOImage {
 id<MTLTexture> texture=nil;
 id<MTLSamplerState> sampler=nil;
 std::shared_ptr<const void> owner;
 // Required for every sampled image: validate the actual producer at Encode.
 std::function<void()> requireReadable;
 bool flipV=false;
 // Actual model MRT or independent map-depth producer; never a scene estimate.
 NativeDeferredPostprocessEpoch deferredEpoch;
};
using SSAOImageResolver=std::function<ResolvedSSAOImage(RuntimeLease&,const NativeCUSTextureBinding&)>;
class PreparedSSAO {
public:
 PreparedSSAO();~PreparedSSAO();
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 // Original callback stage, after source color/depth stores. This encodes one
 // actual original FBO operation, never a command submission or host wait.
 void EncodeDraw(size_t,id<MTL4CommandBuffer>)const;
 void DidComplete()const;
private:struct Impl;std::unique_ptr<Impl>impl;friend class SSAOGPU;
};
class SSAOGPU {
public:
 SSAOGPU();~SSAOGPU();
 // Genuine library and all original program/attachment PSOs only. This does
 // not publish readiness: the same-frame deferred normal producer is required.
 void Initialize(RuntimeLease&,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,double&,unsigned samples=1);
 bool Initialized()const noexcept;
 void ClearResources();
 std::unique_ptr<PreparedSSAO> Prepare(RuntimeLease&,std::shared_ptr<const NativeSSAOFrame>,
  const WorldCamera&,uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture> sceneColor,
  id<MTLTexture> sceneDepth,id<MTLTexture> sceneStencil,const SSAOImageResolver&,bool,double&,uint64_t actualSubmissionIdentity);
 // Shared UI/history consumers must retain the returned token and validate
 // encoded-or-completed contents at actual encode; allocation is not content.
 ResolvedSSAOImage Resolve(RuntimeLease&,std::shared_ptr<const NativeSSAOImageVersion>,uint64_t actualSubmissionIdentity);
private:struct Impl;std::unique_ptr<Impl>impl;
};
}
