/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace rendering::metal4 {
// The original GL viewport may be much larger than the Metal attachment,
// particularly for zoomed PiP. CommonClip133 maps it to bottom-left pixel
// coordinates; the actual Metal raster viewport remains the full attachment.
// Validate corners in that coordinate system, not each rectangle component.
// At 2^19 pixels float pixel coordinates retain at least 1/16-pixel spacing;
// deltas, sums, diamond products, and int conversions in CommonUnitRange133
// and CommonStipple133 remain comfortably in range, including line padding.
// This is a virtual coordinate numeric bound, not a render-target limit.
inline constexpr double CommonLineVirtualCornerLimit133=524288.0;
struct CommonLineViewportContract133 {bool valid=false,extended=false;};
inline CommonLineViewportContract133 ClassifyCommonLineViewport133(
 const std::array<double,4>& viewport,uint32_t targetHeight) noexcept
{
 if(!targetHeight)return {};
 bool extended=false;
 for(double x:viewport){if(!std::isfinite(x))return {};extended|=std::abs(x)>32768.0;}
 if(viewport[2]<=0||viewport[3]<=0)return {};
 const double left=viewport[0],right=left+viewport[2],top=double(targetHeight)-viewport[1],bottom=top-viewport[3];
 for(double x:{left,right,top,bottom})if(!std::isfinite(x)||std::abs(x)>CommonLineVirtualCornerLimit133)return {};
 return {true,extended};
}
}
