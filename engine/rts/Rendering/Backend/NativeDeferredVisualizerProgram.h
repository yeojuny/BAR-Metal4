/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <stdexcept>
#include <string_view>
namespace rendering {
inline constexpr uint64_t NativeDeferredVisualizerVS=7380983429918708591ull,NativeDeferredVisualizerFS=2196167387933199823ull;
inline uint64_t HashNativeDeferredVisualizer(std::string_view s){uint64_t h=14695981039346656037ull;for(unsigned char c:s)h=(h^c)*1099511628211ull;return h;}
inline std::weak_ptr<const void>& NativeDeferredVisualizerOwner(){static std::weak_ptr<const void> owner;return owner;}
struct NativeDeferredVisualizerInput {
 std::shared_ptr<const void> compiled;
 std::array<ScreenSecondaryTexture,13> images;
};
inline void ValidateNativeDeferredVisualizer(const NativeDeferredVisualizerInput& input){
 if(!input.compiled)throw std::runtime_error("Deferred visualizer actual compiled program missing");
 for(unsigned slot=0;slot<13;++slot){
  const auto& image=input.images[slot];if(!image.texture||image.texture->incomplete||image.uvMode!=ScreenUVMode::Normalized)throw std::runtime_error("Deferred visualizer actual complete image missing");
  ValidateScreenSampler(image.sampler);
  const auto f=image.texture->format;const bool depth=f==ScreenTextureFormat::Depth16Unorm||f==ScreenTextureFormat::Depth24Unorm||f==ScreenTextureFormat::Depth32Float;
  if(depth!=(slot==5||slot==11||slot==12))throw std::runtime_error("Deferred visualizer original depth/color slot type differs");
 }
}
} // namespace rendering
