#include "Rendering/Metal4/Metal4CaptureSurfaceBoundary.h"
#include <cassert>
#include "Rendering/Backend/NativeDeferredParticleCapture.h"
#include <limits>
#include <string>
#include <stdexcept>
using namespace rendering;
int main(){
 {
  NativeDeferredParticleCapture state;state.RequireComplete();int writes=0;
  try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}assert(writes==0);
  state.Defer();try{state.RequireComplete();assert(false);}catch(const std::logic_error&){}
  state.Complete([&]{++writes;try{state.RequireComplete();assert(false);}catch(const std::logic_error&){}
   try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){} });
  state.RequireComplete();assert(writes==1);
  try{state.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}
  try{state.Defer();assert(false);}catch(const std::logic_error&){}
  NativeDeferredParticleCapture failed;failed.Defer();
  try{failed.Complete([&]{++writes;throw std::runtime_error("writer");});assert(false);}catch(const std::runtime_error&){}
  try{failed.Complete([&]{++writes;});assert(false);}catch(const std::logic_error&){}
  try{failed.RequireComplete();assert(false);}catch(const std::logic_error&){}assert(writes==2);
 }

 Metal4SurfaceSnapshot p; p.logicalWidth=800;p.logicalHeight=600;
 p.presentationBackingWidth=1600;p.presentationBackingHeight=1200;
 p.renderWidth=1024;p.renderHeight=768;p.visible=true;
 Metal4SurfaceObservation o{800,600,1600,1200,true,false};
 for(int n=0;n<1000;++n){
  assert(NativeCaptureSurfaceUnchanged(p,o,1024,768,true));
  for(int field=0;field<6;++field){auto changed=o;
   switch(field){case 0:++changed.logicalWidth;break;case 1:++changed.logicalHeight;break;
    case 2:++changed.presentationBackingWidth;break;case 3:++changed.presentationBackingHeight;break;
    case 4:changed.visible=false;break;case 5:changed.minimized=true;break;}
   assert(!NativeCaptureSurfaceUnchanged(p,changed,1024,768,true));
  }
  assert(!NativeCaptureSurfaceUnchanged(p,o,1023,768,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,1024,767,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,std::numeric_limits<double>::quiet_NaN(),768,true));
  assert(!NativeCaptureSurfaceUnchanged(p,o,1024,768,false));
  auto closed=p;closed.closeRequested=true;assert(!NativeCaptureSurfaceUnchanged(closed,o,1024,768,true));
  closed=p;closed.attached=false;assert(!NativeCaptureSurfaceUnchanged(closed,o,1024,768,true));
  std::string order;
  auto observe=[&]{order+='O';return true;};auto drain=[&]{order+='D';};
  auto refresh=[&]{order+='R';};auto capture=[&]{order+='C';};
  CaptureAtNativeSurfaceBoundary(true,observe,drain,refresh,capture);assert(order=="OCD");
  order.clear();CaptureAtNativeSurfaceBoundary(false,observe,drain,refresh,capture);assert(order=="RC");
  order.clear();CaptureAtNativeSurfaceBoundary(true,[&]{order+='O';return false;},drain,refresh,capture);assert(order=="ODRCD");
  order.clear();CaptureAtNativeSurfaceBoundary(true,[&]()->bool{order+='O';throw std::runtime_error("observe");},drain,refresh,capture);assert(order=="ODRCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,observe,drain,refresh,[&]{order+='C';throw std::runtime_error("capture");});assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="capture");}assert(order=="OCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,observe,[&]{order+='D';throw std::runtime_error("quarantine");},refresh,[&]{order+='C';throw std::runtime_error("capture");});assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())=="quarantine");}assert(order=="OCD");
  order.clear();try{CaptureAtNativeSurfaceBoundary(true,[&]{order+='O';return false;},[&]{order+='D';throw std::runtime_error("drain");},refresh,capture);assert(false);}catch(const std::runtime_error&){}assert(order=="OD");
  // Observe cannot become a stale write plan: original refresh reobserves after drain.
  int live=1,applied=0;CaptureAtNativeSurfaceBoundary(true,[]{return false;},[&]{live=2;},[&]{applied=live;},[]{});assert(applied==2);
 }
}
