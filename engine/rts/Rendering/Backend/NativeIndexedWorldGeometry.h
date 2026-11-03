/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
namespace rendering {
struct NativeIndexedWorldGeometry {
 std::vector<NativeWorldDrawVertex> vertices;
 std::vector<uint32_t> indices;
};
// Compact only referenced vertices, preserving index order and exact per-vertex
// arithmetic. Unreferenced source attributes were not observed by the old path.
template<class Transform>
NativeIndexedWorldGeometry TransformNativeIndexedWorldGeometry(
 std::span<const ScreenVertex> vertices,std::span<const ScreenTexCoord> uv,
 std::span<const uint32_t> indices,Transform transform,size_t prefix=0)
{
 if(vertices.size()!=uv.size())throw std::runtime_error("Native world font UV stream mismatch");
 if(indices.size()>4u*1024u*1024u)throw std::runtime_error("Native world bounded geometry capacity exceeded");
 NativeIndexedWorldGeometry out;
 std::vector<uint32_t> mapping(vertices.size(),UINT32_MAX);
 if(prefix>3)throw std::runtime_error("Native world geometry prefix exceeds typed header");
 out.vertices.reserve(prefix+std::min(vertices.size(),indices.size()));out.vertices.resize(prefix);out.indices.reserve(indices.size());
 for(auto index:indices){
  if(index>=vertices.size())throw std::runtime_error("Native world font index out of range");
  auto& mapped=mapping[index];
  if(mapped==UINT32_MAX){mapped=uint32_t(out.vertices.size());out.vertices.push_back(transform(vertices[index],uv[index]));}
  out.indices.push_back(mapped);
 }
 // GL ignores the final incomplete triangle. Retain the transformed vertices,
 // as the original expanded command did when any complete triangle existed.
 out.indices.resize(out.indices.size()/3*3);
 return out;
}
}
