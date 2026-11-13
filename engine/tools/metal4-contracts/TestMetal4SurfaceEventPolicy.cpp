#include "Rendering/Metal4/Metal4SurfaceEventPolicy.h"
#include <cassert>
#include <stdexcept>
#include <vector>
using namespace rendering;
int main(){
 const SDL_EventType changes[]={SDL_EVENT_WINDOW_RESIZED,SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
 SDL_EVENT_WINDOW_MOVED,SDL_EVENT_WINDOW_DISPLAY_CHANGED,SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
 SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,SDL_EVENT_WINDOW_SAFE_AREA_CHANGED,SDL_EVENT_WINDOW_MINIMIZED,SDL_EVENT_WINDOW_MAXIMIZED,
 SDL_EVENT_WINDOW_RESTORED,SDL_EVENT_WINDOW_SHOWN,SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_CLOSE_REQUESTED};
 for(int n=0;n<1000;++n)for(auto type:changes){
  SDL_Event e{};e.type=type;e.window.windowID=7;
  assert(ClassifyMetal4SurfaceEvent(e,7)==(type==SDL_EVENT_WINDOW_CLOSE_REQUESTED?Metal4SurfaceEventAction::Close:Metal4SurfaceEventAction::Refresh));
  std::vector<int> order;
  ApplyDrainedMetal4SurfaceEvent(e,7,[&]{order.push_back(1);order.push_back(2);},[&]{order.push_back(3);});
  assert((order==std::vector<int>{1,2,3}));
  bool applied=false;
  try{ApplyDrainedMetal4SurfaceEvent(e,7,[]{throw std::runtime_error("unretired");},[&]{applied=true;});assert(false);}catch(const std::runtime_error&){}
  assert(!applied);
  ApplyDrainedMetal4SurfaceEvent(e,8,[]{assert(false);},[&]{applied=true;});assert(applied);
 }
 for(auto type:{SDL_EVENT_KEY_DOWN,SDL_EVENT_MOUSE_MOTION,SDL_EVENT_QUIT,SDL_EVENT_WINDOW_FOCUS_GAINED}){
  SDL_Event e{};e.type=type;e.window.windowID=7;bool applied=false;
  assert(ClassifyMetal4SurfaceEvent(e,7)==Metal4SurfaceEventAction::Ignore);
  ApplyDrainedMetal4SurfaceEvent(e,7,[]{assert(false);},[&]{applied=true;});assert(applied);
 }
 for(auto type:{SDL_EVENT_DISPLAY_ADDED,SDL_EVENT_DISPLAY_REMOVED}){
  SDL_Event e{};e.type=type;bool drained=false;
  ApplyDrainedMetal4SurfaceEvent(e,7,[&]{drained=true;},[&]{assert(drained);});
 }
}
