/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace rendering::metal4 {

struct ProjectileShadowRange {
 std::size_t firstVertex=0, vertexCount=0;
 std::vector<std::uint32_t> indices;
};

inline std::optional<ProjectileShadowRange> RebaseProjectileShadowRange(
 std::span<const std::uint32_t> source, std::size_t firstIndex, std::size_t indexCount,
 std::size_t sourceVertexCount, std::size_t vertexStride, std::size_t maxBufferLength)
{
 if(firstIndex>source.size()||indexCount>source.size()-firstIndex||!indexCount||indexCount%3)
  throw std::runtime_error("Original projectile shadow index range differs from its immutable geometry");
 const auto indices=source.subspan(firstIndex,indexCount);
 const auto [first,last]=std::minmax_element(indices.begin(),indices.end());
 if(*last>=sourceVertexCount)
  throw std::runtime_error("Original projectile shadow index exceeds actual vertices");

 constexpr std::size_t maxRangeBytes=4*1024*1024;
 const auto byteLimit=std::min(maxRangeBytes,maxBufferLength);
 const std::size_t vertexCount=std::size_t(*last)-*first+1;
 if(indexCount>byteLimit/sizeof(std::uint32_t))return std::nullopt;
 const auto indexBytes=indexCount*sizeof(std::uint32_t);
 // Keep the inexpensive contiguous copy to vertex windows with few gaps.
 if(!vertexStride||vertexCount>(byteLimit-indexBytes)/vertexStride||vertexCount>2*indexCount||
    (vertexCount==sourceVertexCount&&indexCount==source.size()))return std::nullopt;

 ProjectileShadowRange result{*first,vertexCount,{}};
 result.indices.reserve(indexCount);
 for(auto index:indices)result.indices.push_back(index-*first);
 return result;
}

} // namespace rendering::metal4
