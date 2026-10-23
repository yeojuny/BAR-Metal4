/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <array>
#include <string_view>

namespace rendering {
inline constexpr std::uint64_t NativeBlueprintOutlineVertexSourceHash = 0x4934c981c1219b87ULL;
inline constexpr std::uint64_t NativeBlueprintOutlineFragmentSourceHash = 0xdcdbeaf4f86da88cULL;
inline constexpr std::uint64_t NativeBlueprintModelVertexSourceHash = 0x9c02b9c241bd646eULL;
inline constexpr std::uint64_t NativeBlueprintModelFragmentSourceHash = 0x95238fe8d998b0e8ULL;
// Pinned game31657 full producers and helpers, checked before Tracy removal.
struct NativeBlueprintSourceIdentity {std::string_view path;std::uint64_t hash;};
inline constexpr std::array<NativeBlueprintSourceIdentity,6> NativeBlueprintSources31657{{
 {"LuaUI/Widgets/gfx_drawfeatureshape_gl4.lua",17316156156891615917ULL},
	{"LuaUI/Widgets/api_blueprint.lua",0x975785f1e97249dcULL}, // SHA256 1f4a3007878e8b42dc890eaaa4594429e75fb0ce9324bcbeac9285f483c1335f
	{"LuaUI/Widgets/gfx_DrawUnitShape_GL4.lua",0x77bf37063ccebf4aULL}, // SHA256 90a01f32b5899c441732d66301741f3ad34a2b7d8105882dd284ace0e4f8e646
	{"modules/graphics/instancevbotable.lua",0x8cef105d28038367ULL}, // SHA256 a295511e741d2965647697fce3ad3839835ad172e56909937f2ccf9dbbf80e47
	{"modules/graphics/instancevboidtable.lua",0xa7437ae92257b0fbULL}, // SHA256 6a2d472ead54430e0125de3f51d0cbc0f490ea0930c09fccb127166e239b3d84
	{"modules/graphics/luashader.lua",0xd291eb46066d7b4aULL}, // SHA256 61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23
}};
inline constexpr std::uint64_t NativeFeatureShapeVertexHash=7547804107023889020ULL;
inline constexpr std::uint64_t NativeFeatureShapeFragmentHash=5637768792871534473ULL;
inline constexpr std::uint64_t NativeBlueprintModelVertexSourceHash31657=0x33924749c696e818ULL;
inline constexpr std::uint64_t NativeBlueprintModelFragmentSourceHash31657=0x464df04456be1368ULL;
inline constexpr std::uint64_t NativeBlueprintEngineDefinitionsHash31657=0x011c877380a26a88ULL;
inline constexpr std::uint64_t NativeBlueprintQuaternionHash31657=12853114528900136756ULL;
} // namespace rendering
