/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering::ssaoProgram {
inline constexpr uint64_t VertexHash=0x08a37f6b8225a0f5ULL;
inline constexpr uint64_t FuseHash=0xb70ca6c961ce78e1ULL;
inline constexpr uint64_t SampleHash=0xd06222ded480396dULL;
inline constexpr uint64_t BlurHash=0xd86f67f00c651cffULL;
inline constexpr uint64_t TexRectHash=0xe0c9a56df8ce52c0ULL;
inline constexpr uint64_t CompositeHash=0xaff60de122c0107cULL;
inline constexpr uint64_t WidgetHash=0x2e8b860bd0597890ULL;
inline constexpr uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr uint64_t GeometryHelperHash=0x8cef105d28038367ULL;
}
