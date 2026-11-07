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
 // Inspection has no producer/readability side effects. The scheduler must
 // also prove the exact callback frame and following ordinary scene suffix.
 enum class SSAOSceneRejection218 : unsigned {
  Eligible=0, NoDescriptor, NoOwner, Index, NotFinal, SourceCount,
  SourceFrame, SourceSequence, AlreadyEncoded, NoRaster, Clear, LoadClear,
  ClearProducer, PreparedOutput, PreparedPrevious, SourceOutput,
  SourcePrevious, Program, SourceDefines, Geometry, Bindings,
  NonlocalInput, ExternalInput, InputCount, LocalReadCount, LocalVersion,
  LocalStorage, DepthSource, MaskSource, MaskOwner, MissingInput,
  InputView, InputBuffer, InputHeap, InputStorage, InputSharedStorage, InputType,
  InputSamples, InputDevice, MissingColor, MissingDepth, MissingStencil,
  AttachmentAlias, AttachmentView, AttachmentBuffer, AttachmentHeap, AttachmentStorage,
  AttachmentType, AttachmentSamples, AttachmentExtent, AttachmentDevice,
  PassSamples, PassExtent, PassArray, PassImageblock, PassThreadgroup,
  PassTile, PassRasterRate, PassVisibility, PassColorMapping,
  PassSamplePositions, ColorDescriptor, DepthDescriptor, StencilDescriptor,
  ExtraColor
 };
 SSAOSceneRejection218 InspectSceneDraw218(size_t,MTL4RenderPassDescriptor* __unsafe_unretained)const;
 bool CanEncodeSceneDraw218(size_t,MTL4RenderPassDescriptor* __unsafe_unretained)const;
 // Borrow only: no encoder open/end, submission, wait or resource retirement.
 // On failure the scheduler closes its encoder once and never replays the draw.
 void EncodeSceneDraw218(size_t,id<MTL4RenderCommandEncoder> __unsafe_unretained)const;
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
