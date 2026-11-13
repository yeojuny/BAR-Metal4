/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstddef>
#include <set>
#include <utility>
#include <vector>
namespace rendering::metal4 {
using TerrainOwnerRoster=std::vector<std::pair<size_t,size_t>>;
// Preserve the original nested-loop first occurrence, even when its callback is
// empty (and even for nullptr owners). Do not deduplicate by texture or query any
// dynamic producer state. Indices avoid borrowing addresses from movable vectors.
template<class Draws> TerrainOwnerRoster BuildTerrainOwnerRoster(const Draws& draws,bool skipNull=false)
{
 TerrainOwnerRoster result;
 std::set<const void*> seen;
 for(size_t draw=0;draw<draws.size();++draw)
  for(size_t image=0;image<draws[draw].images.size();++image)
   if((!skipNull||draws[draw].images[image].owner)&&seen.insert(draws[draw].images[image].owner.get()).second)
    result.emplace_back(draw,image);
 return result; // Publication happens only after the complete census succeeds.
}
}
