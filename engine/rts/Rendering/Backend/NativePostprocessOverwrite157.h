/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePostprocessData.h"

namespace rendering {
// Classify only the fixed CopyTexSubImage consumer, after normal frame/source
// validation. Its shader has no discard/blend/mask and uses a full destination
// viewport/scissor. This never removes the actual framebuffer snapshot, aliases
// immutable versions, or changes their publication/retirement order.
inline bool NativePostprocessOverwrite157(const NativePostprocessOperation& op,
 std::uint32_t sourceWidth, std::uint32_t sourceHeight)
{
 if(op.kind!=NativePostprocessOperationKind::CopyColor &&
    op.kind!=NativePostprocessOperationKind::CopyDepth)return false;
 if(op.screenOutput||op.viewport||op.outputs.size()!=1||op.previous.size()!=1||
    !op.outputs[0]||!op.outputs[0]->image)return false;
 const auto& image=*op.outputs[0]->image;
 if(!image.width||!image.height)return false;
 if(op.kind==NativePostprocessOperationKind::CopyColor?
    image.format!=NativePostprocessFormat::RGBA8:!IsNativePostprocessDepth(image.format))return false;
 const auto& dst=op.destination;const auto& src=op.source;
 // Exact equality also excludes partial, zero-sized and oversized rectangles.
 // Widen before addition so malformed source extents cannot wrap into bounds.
 return dst.x==0&&dst.y==0&&dst.width==image.width&&dst.height==image.height&&
  src.width==dst.width&&src.height==dst.height&&src.x>=0&&src.y>=0&&
  std::uint64_t(src.x)+src.width<=sourceWidth&&
  std::uint64_t(src.y)+src.height<=sourceHeight;
}
}
