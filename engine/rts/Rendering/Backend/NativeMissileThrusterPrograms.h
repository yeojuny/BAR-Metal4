/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering {
inline constexpr std::array<std::string_view,12> NativeMissileThrusterParameterNames={"SHIMMER_AMPLITUDE","SHIMMER_SPEED","SHIMMER_TAIL_BIAS","BREATHE_BASE","BREATHE_RANGE","BREATHE_SPEED","COLOR_GRADIENT_END","TAIL_FADE_START","TAIL_FADE_END","BRIGHTNESS_MULT","CROSS_SECTION_BRIGHTNESS","CROSS_SECTION_SIZE_MULT"};
inline constexpr std::array<std::string_view,4> NativeMissileThrusterProgramNames={"MissileThrusterFlameGL4","MissileThrusterCrossFlameGL4","MissileThrusterCrossSectionGL4","MissileThrusterGlowGL4"};
inline constexpr std::array<std::uint64_t,4> NativeMissileThrusterVertexHashes={0x3eda17a50c35dab1ULL,0xc502f401798248f2ULL,0x794c85262808a65cULL,0x9d045cfa4c500deULL};
inline constexpr std::array<std::uint64_t,4> NativeMissileThrusterFragmentHashes={0xede3ca71bad9239fULL,0xede3ca71bad9239fULL,0x71b9f4ef6d1ad69aULL,0x236d8dc80a5b114aULL};
inline constexpr std::uint64_t NativeMissileThrusterGadgetHash=0x960c284435096f60ULL;
inline constexpr std::uint64_t NativeMissileThrusterHelperHash=0xa3fc701948f45d37ULL;
// Pinned31657 merges both flame planes, uses true velocity and adds shapeData.
inline constexpr std::array<std::string_view,3> NativeMissileThrusterProgramNames31657={"MissileThrusterFlameGL4","MissileThrusterCrossSectionGL4","MissileThrusterGlowGL4"};
inline constexpr std::array<std::uint64_t,3> NativeMissileThrusterVertexHashes31657={0x31e7f95d0cd2e168ULL,0x367d48d90ec4888aULL,0x2e08107ec2bd99bdULL};
inline constexpr std::array<std::uint64_t,3> NativeMissileThrusterFragmentHashes31657={0xede3ca71bad9239fULL,0x71b9f4ef6d1ad69aULL,0x236d8dc80a5b114aULL};
inline constexpr std::uint64_t NativeMissileThrusterGadgetHash31657=0x0cb9e20f40b4ba6cULL;
inline constexpr std::uint64_t NativeMissileThrusterHelperHash31657=0x8cef105d28038367ULL;
}
