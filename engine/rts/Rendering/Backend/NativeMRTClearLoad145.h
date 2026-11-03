/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeInitialClearLoad.h"
namespace rendering {
// Bits0..4 are the five MRT colors, bit5 is optional combined color, bit6 depth.
// Only the original FIRST ordered clear may prove an attachment fully replaced.
// The caller executes that clear unchanged; this never aliases storage versions.
inline unsigned NativeMRTClearDiscard145(const NativeWorldAttachmentClear& clear,
 std::uint32_t width,std::uint32_t height,bool combined) noexcept {
 const auto ordinary=NativeInitialClearDiscardMask(clear,width,height);
 unsigned mask=(ordinary&ScreenAttachmentColor)?31u:0u;
 if(ordinary&ScreenAttachmentDepth)mask|=64u;
 if(combined){auto indexed=clear;indexed.colorWrite=clear.combinedColorWrite;
  if(NativeInitialClearDiscardMask(indexed,width,height)&ScreenAttachmentColor)mask|=32u;}
 return mask;
}
}
