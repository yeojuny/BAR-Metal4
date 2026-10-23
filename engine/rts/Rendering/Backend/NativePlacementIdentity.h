#pragma once
#include <cstdint>
#include <array>
#include <string_view>
namespace rendering {
inline constexpr std::uint64_t NativeBuildingGridVertexHash=0xd759151d52c6f348ULL;
inline constexpr std::uint64_t NativeBuildingGridFragmentHash=0x8a216ce8a1564ca0ULL;
inline constexpr std::uint64_t NativeBuildingGridWidgetHash=0x83d12bdea1ea3044ULL;
inline constexpr std::uint64_t NativeBuildingGridHelperHash=0x1d6a85914e01f693ULL;
// Pinned31657 widget uses gl_VertexID and mouseFade/lineParams uniforms.
inline constexpr std::uint64_t NativeBuildingGridVertexHash31657=0x53e4c1bee98ade6bULL;
inline constexpr std::uint64_t NativeBuildingGridFragmentHash31657=0x45187ba343e90671ULL;
inline constexpr std::uint64_t NativeBuildingGridEngineHash31657=0x011c877380a26a88ULL;
struct NativeBuildingGridSourceIdentity {std::string_view path;std::uint64_t hash;};
inline constexpr std::array<NativeBuildingGridSourceIdentity,2> NativeBuildingGridSources31657{{
	{"LuaUI/Widgets/gui_building_grid_gl4.lua",0xf281fe68cd4987baULL}, // SHA256 e636a900ec430a0063c3b88b6cdafec5088cdcb39f793a7e8c0cae546d588be3
	{"modules/graphics/luashader.lua",0xd291eb46066d7b4aULL}, // SHA256 61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23
}};
}
