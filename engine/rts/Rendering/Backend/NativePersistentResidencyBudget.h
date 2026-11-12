/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <cstdint>
namespace rendering {
struct NativePersistentResidencyBudget {
 static constexpr uint64_t ByteLimit=uint64_t{256}*1024*1024;
 static constexpr size_t AllocationLimit=8192,SetLimit=64;
 uint64_t bytes=0;
 size_t allocations=0,sets=0;
 bool CanAdd(uint64_t footprint)const noexcept {
  return footprint && bytes<=ByteLimit && footprint<=ByteLimit-bytes &&
   allocations<AllocationLimit && sets<SetLimit;
 }
 void Added(uint64_t footprint)noexcept{bytes+=footprint;++allocations;}
 void Sealed()noexcept{++sets;}
};
}
