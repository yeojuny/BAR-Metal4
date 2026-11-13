#include "Rendering/Backend/NativeRendererAccessGate.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <future>
#include <iostream>
using namespace rendering;
int main(){
 NativeRendererAccessGate gate;
 for(unsigned i=0;i<1000;++i){
  try{NativeRendererAccessGate::Scope draw(&gate);NativeRendererAccessGate::Scope retire(&gate);throw 1;}catch(int){}
  NativeRendererAccessGate::Scope control(&gate);assert(control.Acquired());
 }
 for(unsigned i=0;i<100;++i){
  std::promise<void> held,release;auto done=release.get_future();
  auto owner=std::async(std::launch::async,[&]{NativeRendererAccessGate::Scope draw(&gate);held.set_value();done.wait();});
  held.get_future().wait();
  bool rejected=false,mutated=false;
  try{NativeRendererAccessGate::Scope completion(&gate);mutated=true;}catch(const std::logic_error&){rejected=true;}
  assert(rejected&&!mutated);
  NativeRendererAccessGate::Scope abort(&gate,NativeRendererAccessGate::Try{});assert(!abort.Acquired());
  release.set_value();owner.get();
  NativeRendererAccessGate::Scope after(&gate);assert(after.Acquired());
 }
 // The runtime-owning thread never waits for a renderer-owning thread.
 std::mutex runtime;std::promise<void> locked,attempt;auto proceed=attempt.get_future();
 auto completion=std::async(std::launch::async,[&]{std::lock_guard lock(runtime);locked.set_value();proceed.wait();
  NativeRendererAccessGate::Scope access(&gate,NativeRendererAccessGate::Try{});assert(!access.Acquired());});
 locked.get_future().wait();
 {NativeRendererAccessGate::Scope draw(&gate);attempt.set_value();std::lock_guard lock(runtime);}
 completion.get();
 // Main snapshot gate must end before synchronous dispatch to worker.
 NativeSerialRenderWorker worker;
 {NativeRendererAccessGate::Scope snapshot(&gate);}
 worker.Invoke([&]{NativeRendererAccessGate::Scope draw(&gate);NativeRendererAccessGate::Scope retirement(&gate);});
 std::cout<<"Renderer gate:1000 nested/error cycles,100 cross-thread reject/recover, lock inversion and worker handoff passed\n";
}
