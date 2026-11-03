/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <algorithm>
#include <cstdint>

namespace rendering {
// Only an actual initial full-attachment clear may discard prior pixels.
// The caller still executes the original validated clear, with its exact values,
// and must restore Load for every continuation of the same framebuffer.
inline unsigned NativeInitialClearDiscardMask(const NativeWorldAttachmentClear& clear,
 std::uint32_t logicalWidth,std::uint32_t logicalHeight) noexcept
{
 if(!logicalWidth||!logicalHeight)return 0;
 if(clear.scissor){
  const auto& s=*clear.scissor;
  if(s.x>0||s.y>0||std::int64_t(s.x)+s.width<logicalWidth||
   std::int64_t(s.y)+s.height<logicalHeight)return 0;
 }
 const auto mask=unsigned(clear.mask);unsigned discard=0;
 if((mask&ScreenAttachmentColor)&&std::all_of(clear.colorWrite.begin(),clear.colorWrite.end(),[](bool v){return v;}))discard|=ScreenAttachmentColor;
 if((mask&ScreenAttachmentDepth)&&clear.depthWrite)discard|=ScreenAttachmentDepth;
 if((mask&ScreenAttachmentStencil)&&(clear.frontStencilWriteMask&255u)==255u)discard|=ScreenAttachmentStencil;
 return discard;
}
} // namespace rendering
