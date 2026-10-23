/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace rendering {
inline std::shared_ptr<const void>& EasyFacingConsumerStorage(){static std::shared_ptr<const void> value;return value;}
inline std::shared_ptr<const void> NativeEasyFacingConsumer(){return std::atomic_load(&EasyFacingConsumerStorage());}
inline void SetNativeEasyFacingConsumer(std::shared_ptr<const void> value){std::atomic_store(&EasyFacingConsumerStorage(),std::move(value));}
inline std::uint64_t EasyFacingHash(std::string_view s){std::uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
struct NativeEasyFacingInput {
 std::shared_ptr<const void> compiled;
 std::array<float,4> shape{},detail{},fill{},outline{};
};
inline void ValidateNativeEasyFacing(const NativeEasyFacingInput& v){
 if(!v.compiled||v.compiled!=NativeEasyFacingConsumer())throw std::runtime_error("Easy Facing has no current compiled original shader");
 for(const auto* a:{&v.shape,&v.detail,&v.fill,&v.outline})for(float x:*a)if(!std::isfinite(x))throw std::runtime_error("Easy Facing nonfinite original uniform");
 if(v.shape[0]<=0)throw std::runtime_error("Easy Facing original worldScale divisor is not positive");
}
namespace easyfacing {
inline constexpr std::uint64_t WidgetHash=15668854811151828388ULL,VertexHash=14822577282021709204ULL,FragmentHash=7107287906622517976ULL;
}
}
