/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include <cstdint>
#include <string_view>
namespace rendering::targetPriorityProgram {
inline std::atomic_uint compiledConsumers{0};
inline bool Available(){return compiledConsumers.load()!=0;}
inline uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s){h^=c;h*=1099511628211ULL;}return h;}
inline constexpr uint64_t VertexHash=6138931488374252474ULL;
inline constexpr uint64_t FragmentHash=10996182650683652108ULL;
inline constexpr uint64_t EngineDefinitionsHash=80087823534942856ULL;
inline constexpr std::string_view WidgetSHA512="df8048c0a001086207593e381ccb85136a1b689d63470965671e2f98e957d822d6b3c6b0b6d2acac236b715bf7e71b6f9d792870a21b263d7e9093d53136a137";
}
