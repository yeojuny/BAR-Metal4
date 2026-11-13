#include "Rendering/Backend/NativePresentationPolicy.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace rendering;
int main() {
 unsigned checks=0;
 for(bool full:{false,true}) for(bool dual:{false,true})
  for(int x:{-44,0,43}) for(int y:{-29,0,28})
   for(int width:{0,1,1920,-1,std::numeric_limits<int>::max()})
    for(int height:{0,1,1080,-1,std::numeric_limits<int>::max()}) {
     const auto p=CaptureNativePresentationPolicy(full,dual,x,y,width,height);
     assert(p.drawMargins==(full&&!dual));
     assert(p.safeViewport.x==x && p.safeViewport.y==y);
     assert(p.safeViewport.width==std::uint32_t(width));
     assert(p.safeViewport.height==std::uint32_t(height)); checks+=5;
    }
 NativeSerialRenderWorker worker;
 auto prefix=CaptureNativePresentationPolicy(true,false,7,9,1200,800);
 const auto retained=prefix;
 prefix=CaptureNativePresentationPolicy(false,true,17,19,600,400);
 worker.Invoke([&]{assert(retained.drawMargins);assert(retained.safeViewport.x==7);
  assert(retained.safeViewport.width==1200);}); checks+=3;
 // A suffix samples new policy: it does not inherit the readback prefix.
 const auto suffix=CaptureNativePresentationPolicy(true,true,0,0,900,600);
 assert(!suffix.drawMargins && retained.drawMargins); ++checks;
 std::cout<<checks<<" presentation policy checks passed\n";
}
