/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace rendering::metal4 {
// Indices name EXISTING GroundDecalVertex IDs, before its cyclic provoking
// rotation. Each slot preserves the old effective CUBE_VERT corner exactly.
inline constexpr std::array<std::uint16_t,36> GroundDecalIndicesG01{
 0,1,2, 1,0,5, 6,7,8, 7,6,11,
 0,6,5, 6,0,11, 1,8,7, 2,1,7,
 0,7,11, 7,0,2, 1,6,8, 6,1,5
};
inline constexpr std::size_t GroundDecalIndexBytesG01=sizeof(GroundDecalIndicesG01);
static_assert(GroundDecalIndexBytesG01==72);
constexpr bool GroundDecalIndexEligibleG01(bool enabled,unsigned samples,std::size_t instances)
{
 return enabled&&samples==1&&instances!=0;
}
constexpr bool GroundDecalIndexBufferValidG01(std::uint64_t address,std::uint64_t length)
{
 return address!=0&&address%2==0&&length>=GroundDecalIndexBytesG01&&length%2==0&&
        address<=std::numeric_limits<std::uint64_t>::max()-GroundDecalIndexBytesG01;
}
}
