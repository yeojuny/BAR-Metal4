/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering {
struct NativeCASRaster {ScreenDrawState surface;NativeWorldDrawState world;};
}
namespace rendering::casProgram {
inline constexpr std::uint64_t VertexHash = 16905188423406352921ULL;
inline constexpr std::array<std::uint64_t,8> FragmentHashes{12976081733933028752ULL,1468446619052646899ULL,18018946393008269135ULL,8371756217883324756ULL,10825582919368884089ULL,3561856579442898442ULL,5765949180229104326ULL,4035089104224359485ULL};
inline std::uint64_t Hash(std::string_view bytes) {std::uint64_t value=14695981039346656037ULL;for(unsigned char c:bytes)value=(value^c)*1099511628211ULL;return value;}
}
