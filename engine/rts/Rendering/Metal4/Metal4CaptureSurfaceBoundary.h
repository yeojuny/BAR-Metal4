/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SurfaceState.h"
namespace rendering {
// A no-write observation is deliberately stricter than matching an old revision:
// SDL can change the actual drawable size without changing our CPU snapshot.
inline bool NativeCaptureSurfaceUnchanged(const Metal4SurfaceSnapshot& prior,
 const Metal4SurfaceObservation& observed, double drawableWidth,double drawableHeight,
 bool displaySyncUnchanged) noexcept
{
 return prior.attached&&!prior.closeRequested&&displaySyncUnchanged&&
  prior.logicalWidth==observed.logicalWidth&&prior.logicalHeight==observed.logicalHeight&&
  prior.presentationBackingWidth==observed.presentationBackingWidth&&
  prior.presentationBackingHeight==observed.presentationBackingHeight&&
  prior.visible==observed.visible&&prior.minimized==observed.minimized&&
  drawableWidth==double(prior.renderWidth)&&drawableHeight==double(prior.renderHeight);
}
// Only the value-snapshot capture may overlap the previous encode tail. Unknown
// observations take the original drained Refresh path. A throwing capture drains
// before the caller attempts source rollback; a failed drain keeps quarantine.
template<class Observe,class Drain,class Refresh,class Capture>
void CaptureAtNativeSurfaceBoundary(bool pilot,Observe&& unchanged,Drain&& drain,
 Refresh&& refresh,Capture&& capture)
{
 if(!pilot){refresh();capture();return;}
 bool same=false;
 try{same=unchanged();}catch(...){/* Reobserve through original Refresh after drain. */}
 if(!same){drain();refresh();}
 try{capture();}catch(...){drain();throw;}
 drain(); // Before alpha, water, callbacks or source-owner settlement.
}
}
