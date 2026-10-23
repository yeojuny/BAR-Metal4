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
// The producer publishes a shared owner containing the real Metal library/PSO.
inline std::shared_ptr<const void>& SquadHullConsumerStorage(){static std::shared_ptr<const void> value;return value;}
inline std::shared_ptr<const void> NativeSquadHullConsumer(){return std::atomic_load(&SquadHullConsumerStorage());}
inline void SetNativeSquadHullConsumer(std::shared_ptr<const void> value){std::atomic_store(&SquadHullConsumerStorage(),std::move(value));}
struct NativeSquadHullInput {
 std::shared_ptr<const void> compiled;
 std::array<float,4> color{},stripe{},centroidRadius{},animation{}; // animation: pulse, gradientCenter
};
inline void ValidateNativeSquadHull(const NativeSquadHullInput& input){
 if(!input.compiled||input.compiled!=NativeSquadHullConsumer())throw std::runtime_error("Squad Hull has no current compiled original shader");
 for(const auto* values:{&input.color,&input.stripe,&input.centroidRadius,&input.animation})for(float x:*values)if(!std::isfinite(x))throw std::runtime_error("Squad Hull nonfinite uniform");
}
namespace squadHullProgram {
inline uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
inline constexpr uint64_t VertexHash=7242650464545504984ULL;
inline constexpr uint64_t FragmentHash=5196412396530154375ULL;
}
}
