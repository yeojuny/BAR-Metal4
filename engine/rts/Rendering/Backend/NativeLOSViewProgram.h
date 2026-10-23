/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTerraformOverlayProgram.h"
#include "ScreenDrawList.h"
namespace rendering {
inline std::shared_ptr<const void>& LOSViewConsumerStorage(){static std::shared_ptr<const void> value;return value;}
inline std::shared_ptr<const void> NativeLOSViewConsumer(){return std::atomic_load(&LOSViewConsumerStorage());}
inline void SetNativeLOSViewConsumer(std::shared_ptr<const void> value){std::atomic_store(&LOSViewConsumerStorage(),std::move(value));}
struct NativeLOSViewInput {
 std::shared_ptr<const void> compiled;
 std::shared_ptr<const NativeCUSUniformPublication> engine;
 std::shared_ptr<const ScreenTexture> los;
 ScreenSampler losSampler;
 std::array<float,4> blendFactors{1,1,1,1}; // Original declared but inactive uniform.
};
inline void ValidateNativeLOSView(const NativeLOSViewInput& input){
 if(!input.compiled||input.compiled!=NativeLOSViewConsumer()||!input.engine||!input.los||input.los->incomplete)throw std::runtime_error("LOS View lacks actual program/engine/LOS image");
 ValidateNativeCUSUniformPublication(*input.engine);ValidateScreenSampler(input.losSampler);
 for(float value:input.blendFactors)if(!std::isfinite(value))throw std::runtime_error("LOS View nonfinite uniform");
}
namespace losViewProgram {
inline constexpr uint64_t VertexHash=1695105064347087127ULL;
inline constexpr uint64_t FragmentHash=553332490801139186ULL;
}
}
