/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace rendering {
struct NativeTerrainMeshGeometry {
 bool indexed=true;
 std::vector<std::array<float,3>> positions;
 std::vector<std::array<uint8_t,4>> colors;
 std::vector<uint32_t> indices;
};

// Published only by the original ROAM producer, after its index generation.
// The immutable copy survives later tessellation and GPU completion. Pointer
// identity is a version proof only because the producer invalidates BEFORE
// every position/index writer, not because its mutable vector address is stable.
class NativeTerrainMeshSnapshot {
public:
 const NativeTerrainMeshGeometry& Geometry() const noexcept {return geometry;}
 bool MatchesSource(const void* positions,size_t count,size_t stride,std::span<const uint32_t> indices) const noexcept {
  return sourcePositions==positions && geometry.positions.size()==count && sourceStride==stride &&
         sourceIndices==indices.data() && geometry.indices.size()==indices.size();
 }
private:
 friend class NativeROAMGeometryOwner;
 NativeTerrainMeshSnapshot()=default;
 NativeTerrainMeshGeometry geometry;
 const void* sourcePositions=nullptr;
 const uint32_t* sourceIndices=nullptr;
 size_t sourceStride=0;
};

class NativeROAMGeometryOwner {
public:
 void Invalidate() noexcept {snapshot.reset();}
 std::shared_ptr<const NativeTerrainMeshSnapshot> Capture(const void* positions,size_t count,size_t stride,
                                                         std::span<const uint32_t> indices) const {
  if(auto previous=snapshot.lock()) {
   if(!previous->MatchesSource(positions,count,stride,indices))
    throw std::logic_error("ROAM geometry writer omitted version invalidation");
   return previous;
  }
  if(!positions || !count || stride<sizeof(std::array<float,3>) || count>SIZE_MAX/stride ||
     indices.empty() || indices.size()%3 || indices.size()>uint64_t(INT32_MAX))
   throw std::runtime_error("Original ROAM geometry has an invalid triangle extent");
  auto next=std::shared_ptr<NativeTerrainMeshSnapshot>(new NativeTerrainMeshSnapshot);
  next->sourcePositions=positions;next->sourceIndices=indices.data();next->sourceStride=stride;
  next->geometry.positions.reserve(count);
  const auto* bytes=static_cast<const std::byte*>(positions);
  for(size_t i=0;i<count;++i) {
   std::array<float,3> p;std::memcpy(p.data(),bytes+i*stride,sizeof(p));
   for(float v:p)if(!std::isfinite(v))throw std::runtime_error("Original terrain vertex is nonfinite");
   next->geometry.positions.push_back(p);
  }
  for(uint32_t index:indices)if(index>=count)throw std::runtime_error("Original terrain index exceeds its actual source vertices");
  next->geometry.indices.assign(indices.begin(),indices.end());
  snapshot=next;
  return next;
 }
private:
 // The map does not retain a second permanent mesh per visited patch. Existing
 // bounded geometry caches and in-flight source frames own published versions.
 mutable std::weak_ptr<const NativeTerrainMeshSnapshot> snapshot;
};
} // namespace rendering
