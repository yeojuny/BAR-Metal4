/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <optional>

namespace rendering {
template<std::size_t Count> struct NativeUploadLayout {
 std::array<std::size_t,Count> offsets{};
 std::size_t length=0;
};

// Describe disjoint byte ranges for one upload. This layout alone grants no
// right to reuse storage; its caller must retain every live prepared/GPU owner.
template<std::size_t Count>
std::optional<NativeUploadLayout<Count>> PlanNativeUploadLayout(
 const std::array<std::size_t,Count>& lengths,std::size_t maximum,std::size_t alignment=256)
{
 if(!alignment || (alignment & (alignment-1)))return std::nullopt;
 NativeUploadLayout<Count> result;
 for(std::size_t n=0;n<Count;++n) {
  if(!lengths[n] || result.length>maximum)return std::nullopt;
  const auto padding=(alignment-(result.length & (alignment-1))) & (alignment-1);
  if(padding>maximum-result.length)return std::nullopt;
  result.length+=padding;
  if(lengths[n]>maximum-result.length)return std::nullopt;
  result.offsets[n]=result.length;
  result.length+=lengths[n];
 }
 return result;
}
}
