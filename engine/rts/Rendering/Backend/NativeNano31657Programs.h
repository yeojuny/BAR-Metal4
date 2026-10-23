/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::nano31657Program {
struct SourceIdentity {std::string_view path;std::uint64_t hash;bool lua;};
inline constexpr std::array<SourceIdentity,3> Sources{{
	{"LuaRules/Gadgets/gfx_nano_particles_gl4.lua",0xdaa0e388f11dbdbaULL,true}, // SHA256 14384844c458b8df310a9674a5df9b5bfe756d23f015521a66ae8545eae0bc5c
	{"modules/graphics/instancevbotable.lua",0x8cef105d28038367ULL,true}, // SHA256 a295511e741d2965647697fce3ad3839835ad172e56909937f2ccf9dbbf80e47
	{"LuaUI/Shaders/nano_particles_gl4_nogs.vert.glsl",0x1ed0af2a7538ad19ULL,false}, // SHA256 7febc459b767dbd64dfb22e33808d3290f9ce3bac5cd31b19b974acdae0e9e4e
}};
inline constexpr std::uint64_t VertexHash=0x1ed0af2a7538ad19ULL;
inline constexpr std::uint64_t FragmentHash=0x7527fb7d38d2a2bbULL;
}
