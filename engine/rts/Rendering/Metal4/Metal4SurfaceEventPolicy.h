/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <SDL3/SDL_events.h>
namespace rendering {
enum class Metal4SurfaceEventAction { Ignore, Refresh, Close };
inline Metal4SurfaceEventAction ClassifyMetal4SurfaceEvent(const SDL_Event& event, SDL_WindowID window) noexcept {
 if(!window || event.type<SDL_EVENT_WINDOW_FIRST || event.type>SDL_EVENT_WINDOW_LAST || event.window.windowID!=window)
  return Metal4SurfaceEventAction::Ignore;
 switch(event.type) {
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:return Metal4SurfaceEventAction::Close;
  case SDL_EVENT_WINDOW_RESIZED:case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:
  case SDL_EVENT_WINDOW_MOVED:case SDL_EVENT_WINDOW_DISPLAY_CHANGED:case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
  case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
  case SDL_EVENT_WINDOW_MINIMIZED:case SDL_EVENT_WINDOW_MAXIMIZED:case SDL_EVENT_WINDOW_RESTORED:
  case SDL_EVENT_WINDOW_SHOWN:case SDL_EVENT_WINDOW_HIDDEN:return Metal4SurfaceEventAction::Refresh;
  default:return Metal4SurfaceEventAction::Ignore;
 }
}
// Topology events are global: SpringApp changes native geometry after forwarding
// them, even when the requested render size compares equal. Drain before that.
template<class Drain,class Apply>
void ApplyDrainedMetal4SurfaceEvent(const SDL_Event& event,SDL_WindowID window,Drain&& drain,Apply&& apply) {
 if(ClassifyMetal4SurfaceEvent(event,window)!=Metal4SurfaceEventAction::Ignore ||
    event.type==SDL_EVENT_DISPLAY_ADDED || event.type==SDL_EVENT_DISPLAY_REMOVED)drain();
 apply(); // A drain failure must never mutate surface state.
}
}
