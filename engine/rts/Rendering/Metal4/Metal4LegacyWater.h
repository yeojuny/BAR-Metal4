/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeLegacyWaterData.h"
namespace rendering::metal4 {
class PreparedLegacyWater {
public:
 ~PreparedLegacyWater();
 void Encode(id<MTL4RenderCommandEncoder>);
 void EncodeDependencies(id<MTL4CommandBuffer>);
 void DidComplete();
 void EncodeCopy(id<MTL4CommandBuffer>,id<MTLTexture> source,id<MTLBuffer> origin);
private:
 struct Impl;std::unique_ptr<Impl> impl;
 explicit PreparedLegacyWater(std::unique_ptr<Impl>);
 friend class LegacyWaterGPU;
};
class LegacyWaterGPU {
public:
 LegacyWaterGPU();~LegacyWaterGPU();
 std::unique_ptr<PreparedLegacyWater> Prepare(RuntimeLease&,std::shared_ptr<const NativeLegacyWaterFrame>,
  id<MTLTexture> reflection,id<MTLTexture> bump,bool bumpFlipV,MTLPixelFormat color,unsigned samples,bool validation,double& compileMs,
 CUSSourceImage shading={},uint32_t renderHeight=0,uint64_t submission=0);
 CUSSourceImage ResolveCopy(RuntimeLease&,const NativeCUSTextureBinding&,uint64_t submission);
private:
 struct Impl;std::unique_ptr<Impl> impl;
};
}
