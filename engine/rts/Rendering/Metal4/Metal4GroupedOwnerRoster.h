/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4TerrainOwnerRoster.h"
namespace rendering::metal4 {
// Borrow only while sealing. Flat indices preserve first occurrence across the
// before/after boundary without copying draws, images, callbacks or owners.
template<class Draws> TerrainOwnerRoster BuildGroupedOwnerRoster(const Draws& before,const Draws& after)
{
 struct View {
  const Draws& before;const Draws& after;
  size_t size()const{return before.size()+after.size();}
  const auto& operator[](size_t n)const{return n<before.size()?before[n]:after[n-before.size()];}
 };
 return BuildTerrainOwnerRoster(View{before,after},true);
}
}
