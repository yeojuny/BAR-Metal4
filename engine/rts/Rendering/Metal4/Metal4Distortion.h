/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4DeferredLights.h"
#include "Rendering/Backend/NativeDistortionData.h"
namespace rendering::metal4 {
class MainFrameAttachments;
// Uses the common original source image resolver only for actual independent
// depth publications and the archive's genuine sampler3D DDS noise.
using DistortionImageResolver=std::function<DeferredLightImage(RuntimeLease&,const NativeCUSTextureBinding&,unsigned)>;
class PreparedDistortion {
public:
 PreparedDistortion();~PreparedDistortion();
 size_t DrawCount()const;uint64_t SequenceAt(size_t)const;
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void EncodeDraw(size_t,id<MTL4CommandBuffer>);
 void DidComplete();
private:struct Impl;std::unique_ptr<Impl>impl;friend class DistortionGPU;
};
class DistortionGPU {
public:
 DistortionGPU();~DistortionGPU();
 std::shared_ptr<const NativeDistortionProgramConsumer> LinkOriginalStages(RuntimeLease&,const NativeDistortionProgramSource&,double&);
 std::unique_ptr<PreparedDistortion> Prepare(RuntimeLease&,std::shared_ptr<const NativeWorldDrawFrame>,uint64_t sourceDrawFrame,const WorldCamera&,
  uint32_t logicalWidth,uint32_t logicalHeight,id<MTLTexture> sceneColor,id<MTLTexture> sceneDepth,id<MTLTexture> sceneStencil,
  const DistortionImageResolver&,bool validation,double&,uint64_t actualSubmissionIdentity, std::shared_ptr<const MainFrameAttachments> main = {});
 void ClearResources();
private:struct Impl;std::unique_ptr<Impl>impl;
};
}
