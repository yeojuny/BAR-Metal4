/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTerraformOverlayProgram.h"
#include "ScreenDrawList.h"
#include "NativeShadowTextureSource.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
namespace rendering {
inline std::array<std::shared_ptr<const void>,2>& WaterOverlayConsumers(){static std::array<std::shared_ptr<const void>,2> value;return value;}
inline std::shared_ptr<const void> NativeWaterOverlayConsumer(bool fog){return std::atomic_load(&WaterOverlayConsumers()[fog]);}
inline void SetNativeWaterOverlayConsumer(bool fog,std::shared_ptr<const void> value){std::atomic_store(&WaterOverlayConsumers()[fog],std::move(value));}
struct NativeWaterOverlayInput {
 std::shared_ptr<const void> compiled;
 std::shared_ptr<const NativeCUSUniformPublication> engine;
 std::array<ScreenSecondaryTexture,6> images;
 std::array<NativeCUSTextureBinding,2> typedImages; // original shadow, original $info
 std::array<ScreenSampler,2> typedSamplers;
 bool fog=false,acid=false,clampShadowReference=false,clipZeroToOne=false;
 std::array<float,3> parameters{1,1,1}; // lavaHeight, heatdistortx, heatdistortz
};
inline void ValidateNativeWaterOverlay(const NativeWaterOverlayInput& i){
 if(!i.compiled||i.compiled!=NativeWaterOverlayConsumer(i.fog)||!i.engine)throw std::runtime_error("Water Overlay actual compiled consumer/engine missing");
 ValidateNativeCUSUniformPublication(*i.engine);
 for(float v:i.parameters)if(!std::isfinite(v))throw std::runtime_error("Water Overlay nonfinite uniform");
 for(unsigned n=0;n<(i.fog?3u:4u);++n){const auto& t=i.images[n];if(!t.texture||t.texture->incomplete)throw std::runtime_error("Water Overlay actual sampled image missing");ValidateScreenSampler(t.sampler);
  const auto f=t.texture->format;const bool depth=f==ScreenTextureFormat::Depth32Float||f==ScreenTextureFormat::Depth24Unorm||f==ScreenTextureFormat::Depth16Unorm;
  if(depth!=(i.fog?n<2:false))throw std::runtime_error("Water Overlay original sampled channel type differs");
 }
 if(!i.fog){
  const auto& shadow=i.typedImages[0];const auto& info=i.typedImages[1];
  if(!shadow.object||!std::dynamic_pointer_cast<const NativeShadowTextureBacking>(shadow.object->backing)||!info.object||!std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(info.object->backing))throw std::runtime_error("Water Overlay actual shadow/info backing missing");
  if(shadow.object->sampling.compareMode!=0x884E||shadow.object->sampling.compareFunction!=0x0203)throw std::runtime_error("Water Overlay original depth comparison differs");
  ValidateScreenSampler(i.typedSamplers[1]);
 }
}
namespace waterOverlayProgram {
inline constexpr std::array<uint64_t,4> SourceHashes={4045567503644266996ULL,12190189472686072439ULL,238463846630049189ULL,14420901465845788581ULL};
}
}
