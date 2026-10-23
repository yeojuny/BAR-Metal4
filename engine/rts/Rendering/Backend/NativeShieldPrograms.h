/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering {
// current31657/2f0a840. Full archive identities precede the private native scope.
// Shader identities include the original LuaShader engine UBO text and binding6.
struct NativeShieldArchiveSource { std::string_view path; std::uint64_t hash; bool lua; };
inline constexpr std::array<NativeShieldArchiveSource,6> NativeShieldArchiveSources{{
	{"luarules/gadgets/gfx_unit_shield_effects.lua",0x4e11d24b53643f85ULL,true},
	{"shaders/shieldspherecolorgl4.vert.glsl",0x5a762c08d3dd5128ULL,false},
	{"shaders/shieldspherecolorgl4.frag.glsl",0x447e28db5ef060b8ULL,false},
	{"modules/graphics/luashader.lua",0xd291eb46066d7b4aULL,true},
	{"modules/graphics/instancevbotable.lua",0x8cef105d28038367ULL,true},
	{"luarules/gadgets/include/iterablemap.lua",0xf60e32fa6c0a2291ULL,true},
}};
inline constexpr std::uint64_t NativeShieldGadgetHash=0x4e11d24b53643f85ULL;
inline constexpr std::uint64_t NativeShieldVertexHash=0x266e12c15d4fec41ULL;
inline constexpr std::array<std::uint64_t,2> NativeShieldFragmentHashes{0xbf7b350c761e7a9cULL,0x7a1d8bfd0f911fefULL};
} // namespace rendering
