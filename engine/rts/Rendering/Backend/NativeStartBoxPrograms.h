/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering::startBoxProgram {
inline constexpr std::uint64_t WidgetHash = 0x9d89e1d174b269f0ULL;
inline constexpr std::uint64_t ShaderHelperHash = 0xd291eb46066d7b4aULL;
inline constexpr std::uint64_t InstanceHelperHash = 0x8cef105d28038367ULL;
inline constexpr std::uint64_t ConeFragmentHash = 0x1f89729909169e49ULL;
inline constexpr std::uint64_t ConeVertexHash = 0xeec8cce60b64d70dULL;
inline constexpr std::uint64_t PolygonFragmentHash = 0xe4ef19a541e1c8d4ULL;
inline constexpr std::uint64_t PolygonVertexHash = 0x458b5ee560718326ULL;
inline constexpr std::uint64_t SDFHelperHash = 0xf198966e07f46402ULL;
inline constexpr std::uint64_t SDFBakeVertexHash = 0x5ef4fdf8f5a8ef0aULL;
inline constexpr std::uint64_t SDFBakeFragmentHash = 0xc0644b76c9f4f788ULL;
// Exact31657 GetEngineUniformBufferDefs return value, without map-specific water helper.
inline constexpr std::uint64_t EngineDefinitionsHash = 0x011c877380a26a88ULL;
}
