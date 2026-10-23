/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <string_view>
#include <cstdint>
namespace rendering::resourcespots {
struct ArchiveSource {std::string_view path;uint64_t hash;};
inline constexpr ArchiveSource ArchiveSources[]={
 {"LuaUI/Widgets/gui_metalspots.lua",3227736578950002387ULL},
 {"LuaUI/Widgets/gui_geothermalspots.lua",6890935614421135535ULL},
 {"modules/graphics/luashader.lua",15173167305575988042ULL},
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL},
 {"LuaUI/Shaders/metalspots_gl4.vert.glsl",11410245162135241606ULL},
 {"LuaUI/Shaders/metalspots_gl4.frag.glsl",6422574825998095214ULL},
};
inline constexpr uint64_t MetalVertexHash=11017852194967602293ULL;
inline constexpr uint64_t MetalFragmentHash=7623306818359381113ULL;
inline constexpr uint64_t GeoVertexHash=15648886499677810171ULL;
inline constexpr uint64_t GeoFragmentHash=15430956651627105511ULL;
}
