/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SurfaceState.h"
namespace rendering {
// Source-frame transaction marker, not a resource owner or GPU completion.
// A later callback failure must drain before any source journal rolls back.
class NativeCaptureRollbackFence {
 bool pending=false;
public:
 bool Armed()const noexcept{return pending;}
 void Arm()noexcept{pending=true;}
 void Dismiss()noexcept{pending=false;}
 template<class Drain>void BeforeRollback(Drain&& drain){
  if(!pending)return;
  drain(); // Leave armed on failure; do not authorize rollback.
  pending=false;
 }
 template<class Observe,class Drain,class Refresh>
 void RefreshForDispatch(Observe&& unchanged,Drain&& drain,Refresh&& refresh){
  if(!pending){refresh();return;}
  bool same=false;
  try{same=unchanged();}catch(...){/* Unknown surface requires full retirement. */}
  if(same)return; // No CAMetalLayer property writes while predecessor is pending.
  drain();
  refresh(); // Reobserve after the drain, never apply the earlier observation.
 }
};
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
// Unknown observations retain the drained Refresh path. Only an exactly stable
// surface and a caller-proved, successfully CPU-settled suffix may retain the
// predecessor GPU ticket. This is the ordinary joined-OFF source-capture state,
// not GPU completion or permission to reuse its physical storage. The existing
// runtime retirement gate still precedes the successor GPU commit.
// A throwing capture/readiness check drains before source rollback; failed
// drains keep the existing quarantine semantics.
template<class Observe,class Drain,class Refresh,class Capture,class Ready>
bool CaptureAtNativeSurfaceBoundary(bool pilot,Observe&& unchanged,Drain&& drain,
 Refresh&& refresh,Capture&& capture,Ready&& cpuSettledSuffix)
{
 if(!pilot){refresh();capture();return false;}
 bool same=false;
 try{same=unchanged();}catch(...){/* Reobserve through original Refresh after drain. */}
 if(!same){drain();refresh();}
 try{capture();if(same&&cpuSettledSuffix())return true;}
 catch(...){drain();throw;}
 drain(); // Required/unknown suffix, changed surface, or conservative fallback.
 return false;
}
template<class Observe,class Drain,class Refresh,class Capture>
void CaptureAtNativeSurfaceBoundary(bool pilot,Observe&& unchanged,Drain&& drain,
 Refresh&& refresh,Capture&& capture)
{
 (void)CaptureAtNativeSurfaceBoundary(pilot,unchanged,drain,refresh,capture,[]{return false;});
}
}
