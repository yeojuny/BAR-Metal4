/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering {
inline constexpr std::array<std::string_view,8> NativeFireSmokeParameterNames={"SMOKE_GROWTH_MULT","SMOKE_GROWTH_RATE","SMOKE_WOBBLE_START","SMOKE_WOBBLE_RAMP","SMOKE_WOBBLE_RATE","WIND_SMOKE_MULT","WIND_FIRE_MULT","HIGHLIGHT_BRIGHT"};
inline constexpr std::uint64_t NativeFireSmokeVertexHash=0x4c941d01869b027eULL;
inline constexpr std::uint64_t NativeFireSmokeFragmentHash=0x7b0c28454342ce7fULL;
inline constexpr std::uint64_t NativeFireSmokeGadgetHash=0xff8bbf2cb123ae26ULL;
inline constexpr std::uint64_t NativeFireSmokeHelperHash=0xa3fc701948f45d37ULL;
inline constexpr std::uint64_t NativeFireSmokeGadgetHash31657=0xe3b87564c0fda585ULL;
inline constexpr std::uint64_t NativeFireSmokeHelperHash31657=0x8cef105d28038367ULL;
}
