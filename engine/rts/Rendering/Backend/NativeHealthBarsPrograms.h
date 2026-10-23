/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::healthBarsProgram {
struct SourceIdentity { std::string_view path; std::uint64_t hash; };
inline constexpr std::array<SourceIdentity,6> Sources{{
	{"LuaUI/Widgets/gui_healthbars_gl4.lua", 4169519044303386454ULL},
	{"modules/graphics/instancevbotable.lua", 10155353677033079655ULL},
	{"modules/graphics/luashader.lua", 15173167305575988042ULL},
	{"LuaUI/Shaders/HealthbarsGL4.vert.glsl", 13756297474863236165ULL},
	{"LuaUI/Shaders/HealthbarsGL4.geom.glsl", 5947468490965667473ULL},
	{"LuaUI/Shaders/HealthbarsGL4.frag.glsl", 11270429282072055188ULL},
}};
inline constexpr std::uint64_t EngineDefinitionsHash = 80087823534942856ULL;
inline constexpr std::string_view Atlas = "LuaUI/Images/healtbars_exo4.tga";
}
