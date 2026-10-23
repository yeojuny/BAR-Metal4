/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4EngineShadow.h"
#include "Metal4OriginalShadowParticles.h"
#include "Metal4ShadowTextureNative.h"
#include "Rendering/Backend/NativeOriginalShadowData.h"
namespace rendering::metal4 {
// Shares the common lease, actual shadow attachments and original ordinals.
// Common recorder generic/clear commands interleave through the same encoder;
// this object does not invent a second clear owner or submission.
class PreparedOriginalShadowSource {
public:
 ~PreparedOriginalShadowSource();
 id<MTLTexture> Depth()const;id<MTLTexture> Color()const;
 const CUSSourceTarget& Target()const;
 uint64_t BeginSequence()const;uint64_t EndSequence()const;
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void EncodeDraw(size_t,id<MTL4RenderCommandEncoder>);
 // The common ordered dispatcher passes its FINAL actual shadow encoder.
 // This function ends that encoder before publishing encoded source versions.
 void EndEncoding(id<MTL4CommandBuffer>,id<MTL4RenderCommandEncoder>);
 void DidComplete(); // immediately after the SAME successful common wait
private:
 PreparedOriginalShadowSource();struct Impl;std::unique_ptr<Impl> impl;
 friend std::unique_ptr<PreparedOriginalShadowSource> PrepareOriginalShadowSource(RuntimeLease&,
  std::shared_ptr<const NativeOriginalShadowFrame>,std::shared_ptr<const NativeWorldDrawFrame>,CUSSourceGPU&,EngineShadowGPU&,OriginalShadowParticlesGPU&,
  const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,std::span<const NativeWorldAttachmentClear>,uint64_t,uint64_t,bool,double&);
};
std::unique_ptr<PreparedOriginalShadowSource> PrepareOriginalShadowSource(RuntimeLease&,
 std::shared_ptr<const NativeOriginalShadowFrame>,std::shared_ptr<const NativeWorldDrawFrame>,CUSSourceGPU&,EngineShadowGPU&,OriginalShadowParticlesGPU&,
 const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,std::span<const NativeWorldAttachmentClear> actualClearQueue,
 uint64_t actualDrawFrame,uint64_t actualSubmission,bool validation,double& milliseconds);
// Used BEFORE Lua construction too; this exact target comes from the real
// original allocated backing, not a hardcoded D32 profile or normal MRT.
CUSSourceTarget OriginalShadowSourceTarget(const NativeShadowTextureBacking& depth,
 const NativeShadowTextureBacking& color,const WorldCamera& drawing);
}
