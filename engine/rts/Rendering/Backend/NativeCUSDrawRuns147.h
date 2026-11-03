/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSModelInputs.h"
#include <limits>
#include <stdexcept>

namespace rendering {
// Each caller is ONE original source draw: program, uniforms, raster state,
// material and instance buffer remain fixed. Never sort or cross that boundary.
// Only adjacent identical immutable geometry/index ranges share one draw.
template<class Consumer>
void ForEachNativeCUSDrawRun147(
 std::span<const std::shared_ptr<const NativeCUSModelSubmission>> submissions,
 Consumer&& consume)
{
 if(submissions.size()>std::numeric_limits<uint32_t>::max())
  throw std::runtime_error("CUS original instance range exceeds native draw capacity");
 for(size_t first=0;first<submissions.size();){
  const auto& source=submissions[first];
  if(!source||!source->geometry)
   throw std::runtime_error("CUS original draw run lacks immutable model geometry");
  size_t end=first+1;
  while(end<submissions.size()&&submissions[end]&&
    submissions[end]->geometry==source->geometry&&
    submissions[end]->originalIndexFirst==source->originalIndexFirst&&
    submissions[end]->originalIndexCount==source->originalIndexCount)++end;
  consume(uint32_t(first),uint32_t(end-first),*source);
  first=end;
 }
}
}
