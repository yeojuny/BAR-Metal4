/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4Bloom.h is a private Objective-C++ renderer interface"
#endif
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeBloomData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <functional>
#include "Rendering/Backend/NativeDeferredPostprocessEpoch.h"
namespace rendering::metal4 {
struct ResolvedBloomImage {
 id<MTLTexture> texture=nil;
 id<MTLSamplerState> sampler=nil;
 std::shared_ptr<const void> owner;
 // Required for every sampled image: validate the actual producer at Encode.
 std::function<void()> requireReadable;
 bool flipV=false;
 // Actual model MRT or independent map-depth producer; never a scene estimate.
 NativeDeferredPostprocessEpoch deferredEpoch;
};
using BloomImageResolver=std::function<ResolvedBloomImage(RuntimeLease&,const NativeCUSTextureBinding&)>;
class PreparedBloom {
public:
 PreparedBloom();~PreparedBloom();
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 // Original callback stage, after source color/depth stores. This encodes one
 // actual original FBO operation, never a command submission or host wait.
 void EncodeDraw(size_t,id<MTL4CommandBuffer>)const;
  // The scheduler inspects the exact prepared operation/target and independently
  // admits its next consumer before opening an encoder. Inspection has no effects.
  bool CanEncodeSceneDraw(size_t,MTL4RenderPassDescriptor* __unsafe_unretained)const;
  // The boolean gate and bounded preparation diagnostics share one inspection.
  // Values describe the first unmet condition; Eligible does not encode a draw.
  enum class SceneDrawRejection : unsigned {
   Eligible=0, NoDescriptor, NoOwner, Index, NotFinal, SourceCount,
   AlreadyEncoded, NoRaster, Clear, PreparedOutput, PreparedPrevious,
   SourceOutput, SourcePrevious, Program, NonlocalInput, ExternalInput,
   InputCount, LocalReadCount, LocalVersion, MissingInput, InputView,
   InputBuffer, InputHeap, InputStorage, MissingColor, MissingDepth,
   MissingStencil, AttachmentAlias, AttachmentView, AttachmentBuffer,
   AttachmentType, AttachmentSamples, AttachmentExtent, PassSamples,
   PassExtent, PassArray, PassImageblock, PassThreadgroup, PassTile,
   PassRasterRate, PassVisibility, PassColorMapping, PassSamplePositions,
   ColorDescriptor, DepthDescriptor, StencilDescriptor, ExtraColor
  };
  SceneDrawRejection InspectSceneDraw(size_t,MTL4RenderPassDescriptor* __unsafe_unretained)const;
  // Borrow only: never opens/ends/submits the scheduler-owned encoder or retires
  // resources. Errors propagate; the scheduler closes once and never replays.
  void EncodeSceneDraw(size_t,id<MTL4RenderCommandEncoder> __unsafe_unretained)const;
 void DidComplete()const;
private:struct Impl;std::unique_ptr<Impl>impl;friend class BloomGPU;
};
class BloomGPU {
public:
 BloomGPU();~BloomGPU();
 // Genuine library and all original HDR program/attachment PSOs only. This does
 // not publish readiness: the actual deferred diffuse/emission/depth producer is required.
 void Initialize(RuntimeLease&,MTLPixelFormat,MTLPixelFormat,MTLPixelFormat,bool,double&,unsigned samples=1);
 bool Initialized()const noexcept;
 void ClearResources();
 std::unique_ptr<PreparedBloom> Prepare(RuntimeLease&,std::shared_ptr<const NativeBloomFrame>,
  const WorldCamera&,uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture> sceneColor,
  id<MTLTexture> sceneDepth,id<MTLTexture> sceneStencil,const BloomImageResolver&,bool,double&,uint64_t actualSubmissionIdentity);
 // Shared UI/history consumers must retain the returned token and validate
 // encoded-or-completed contents at actual encode; allocation is not content.
 ResolvedBloomImage Resolve(RuntimeLease&,std::shared_ptr<const NativeBloomImageVersion>,uint64_t actualSubmissionIdentity);
private:struct Impl;std::unique_ptr<Impl>impl;
};
}
