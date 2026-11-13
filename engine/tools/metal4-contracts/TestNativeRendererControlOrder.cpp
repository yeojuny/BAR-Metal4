#include "Rendering/Backend/NativeRendererAccessGate.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace rendering;
int main(){
 const auto main=std::this_thread::get_id();std::thread::id render;
 NativeRendererAccessGate gate;auto worker=std::make_unique<NativeSerialRenderWorker>();
 unsigned draw=0,retired=0,cleared=0;
 for(unsigned n=1;n<=1000;++n){
  worker->Invoke([&]{NativeRendererAccessGate::Scope access(&gate);if(render==std::thread::id{})render=std::this_thread::get_id();assert(render!=main);draw=n;});
  {NativeRendererAccessGate::Scope mainAccess(&gate);assert(std::this_thread::get_id()==main);retired=draw;}
  {NativeRendererAccessGate::Handoff handoff(gate);worker->Invoke([&]{NativeRendererAccessGate::Scope access(handoff);assert(std::this_thread::get_id()==render&&retired==draw);cleared=draw;});}
  assert(cleared==n);
  try{NativeRendererAccessGate::Handoff handoff(gate);worker->Invoke([&]{NativeRendererAccessGate::Scope access(handoff);throw std::logic_error("control failure");});assert(false);}catch(const std::logic_error&){}
 }
 worker.reset(); // Joined before the renderer gate/resource owners are released.
 NativeRendererAccessGate::Scope after(&gate);assert(after.Acquired());
 std::cout<<"Renderer executor protocol:1000 draw/main-retire/same-worker-control sequences and control error recovery;owner join passed\n";
}
