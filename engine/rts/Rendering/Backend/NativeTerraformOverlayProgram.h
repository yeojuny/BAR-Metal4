/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSUniformData.h"
#include "NativeTargetPriorityProgram.h"
#include <atomic>
#include <cmath>
#include <stdexcept>
namespace rendering {
inline std::shared_ptr<const void>& TerraformOverlayConsumerStorage(){static std::shared_ptr<const void> value;return value;}
inline std::shared_ptr<const void> NativeTerraformOverlayConsumer(){return std::atomic_load(&TerraformOverlayConsumerStorage());}
inline void SetNativeTerraformOverlayConsumer(std::shared_ptr<const void> value){std::atomic_store(&TerraformOverlayConsumerStorage(),std::move(value));}
struct NativeTerraformOverlayInput {
 std::shared_ptr<const void> compiled;
 std::shared_ptr<const NativeCUSUniformPublication> engine;
 std::array<float,4> placement{1,1,0,0},fit{1,1,0,0};
};
inline void ValidateNativeTerraformOverlay(const NativeTerraformOverlayInput& input){
 if(!input.compiled||input.compiled!=NativeTerraformOverlayConsumer()||!input.engine)throw std::runtime_error("Terraform Overlay lacks actual program/engine publication");
 ValidateNativeCUSUniformPublication(*input.engine);
 for(const auto* values:{&input.placement,&input.fit})for(float value:*values)if(!std::isfinite(value))throw std::runtime_error("Terraform Overlay nonfinite parameter");
 if(input.fit[0]<=0||input.fit[1]<=0)throw std::runtime_error("Terraform Overlay original aspect divisors invalid");
}
namespace terraformOverlayProgram {
inline constexpr uint64_t VertexHash=12195850821012783697ULL;
inline constexpr uint64_t FragmentHash=8095003521360994526ULL;
}
}
