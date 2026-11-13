/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"

namespace rendering {
// Value-only main submission policy. Surface size/revision remains owned by
// Metal4SurfaceSnapshot; this does not authorize reinterpreting an old surface.
struct NativePresentationPolicy {
 bool drawMargins = false;
 ScreenScissor safeViewport{};
};
inline NativePresentationPolicy CaptureNativePresentationPolicy(bool fullScreen, bool dualScreen,
 int viewX, int viewY, int viewWidth, int viewHeight)
{
 // Preserve the original unsigned extent conversion and eligibility predicate.
 return {fullScreen && !dualScreen,
  {viewX,viewY,std::uint32_t(viewWidth),std::uint32_t(viewHeight)}};
}
}
