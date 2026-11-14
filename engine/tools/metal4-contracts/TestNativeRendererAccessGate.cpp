#include "Rendering/Backend/NativeRendererAccessGate.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <atomic>
#include <future>
#include <iostream>
#include <optional>
#include <chrono>
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
 // Reserved snapshot cannot be invalidated even before worker acquisition.
 for(unsigned i=0;i<100;++i){
  {
   NativeRendererAccessGate::Handoff token(gate);
   NativeRendererAccessGate::Scope gap(&gate,NativeRendererAccessGate::Try{});assert(!gap.Acquired());
   bool duplicate=false;try{NativeRendererAccessGate::Handoff other(gate);}catch(const std::logic_error&){duplicate=true;}assert(duplicate);
   worker.Invoke([&]{NativeRendererAccessGate::Scope admit(token);NativeRendererAccessGate::Scope nested(&gate);
    bool repeated=false;try{NativeRendererAccessGate::Scope again(token);}catch(const std::logic_error&){repeated=true;}assert(repeated);
   });
   NativeRendererAccessGate::Scope afterWorker(&gate,NativeRendererAccessGate::Try{});assert(!afterWorker.Acquired());
  }
  NativeRendererAccessGate::Scope restored(&gate);assert(restored.Acquired());
 }
 // Exceptions on either side of Invoke release reservation, and retirement
 // inside the worker retains its normal same-thread nesting behavior.
 for(unsigned i=0;i<100;++i){
  try{NativeRendererAccessGate::Handoff token(gate);throw 1;}catch(int){}
  try{NativeRendererAccessGate::Handoff token(gate);worker.Invoke([&]{NativeRendererAccessGate::Scope admit(token);throw 2;});}catch(int){}
  NativeRendererAccessGate::Scope restored(&gate);assert(restored.Acquired());
 }
 // Controlled contention at reservation handoff: worker waits for the source
 // mutex; duplicate token admission remains fail-fast while worker owns it.
 for(unsigned i=0;i<100;++i){
  std::optional<NativeRendererAccessGate::Scope> source;source.emplace(&gate);
  NativeRendererAccessGate::Handoff token(gate);
  std::promise<void> started,entered,release;auto finish=release.get_future();
  auto owned=std::async(std::launch::async,[&]{started.set_value();
   NativeRendererAccessGate::Scope admit(token);entered.set_value();finish.wait();});
  started.get_future().wait();auto ready=entered.get_future();
  assert(ready.wait_for(std::chrono::milliseconds(2))==std::future_status::timeout);
  source.reset(); // Never Join while holding the source gate.
  ready.get();
  auto duplicate=std::async(std::launch::async,[&]{
   try{NativeRendererAccessGate::Scope again(token);return false;}
   catch(const std::logic_error&){return true;}});
  assert(duplicate.wait_for(std::chrono::seconds(2))==std::future_status::ready);
  assert(duplicate.get());
  NativeRendererAccessGate::Scope denied(&gate,NativeRendererAccessGate::Try{});assert(!denied.Acquired());
  release.set_value();owned.get();
  // Original sequential token reuse semantics are preserved.
  worker.Invoke([&]{NativeRendererAccessGate::Scope next(token);assert(next.Acquired());});
 }
 // A runtime-owning rejected observer must never wait on a worker gate.
 {
  NativeRendererAccessGate::Handoff token(gate);
  std::promise<void> runtimeHeld,workerHeld;auto start=runtimeHeld.get_future();auto acquired=workerHeld.get_future();
  auto observer=std::async(std::launch::async,[&]{std::lock_guard lock(runtime);runtimeHeld.set_value();acquired.wait();
   NativeRendererAccessGate::Scope probe(&gate,NativeRendererAccessGate::Try{});assert(!probe.Acquired());});
  start.wait();
  worker.Invoke([&]{NativeRendererAccessGate::Scope admit(token);workerHeld.set_value();std::lock_guard lock(runtime);});
  observer.get();
 }
 // Rejected probes racing cancellation must not terminate the process.
 std::atomic<bool> stop=false;
 auto probe=std::async(std::launch::async,[&]{while(!stop.load()){NativeRendererAccessGate::Scope access(&gate,NativeRendererAccessGate::Try{});}});
 unsigned reserved=0;
 for(unsigned i=0;i<10000;++i){try{NativeRendererAccessGate::Handoff token(gate);++reserved;}catch(const std::logic_error&){} }
 stop=true;probe.get();assert(reserved>0);
 std::cout<<"Renderer gate:1000 nested/error cycles,100 cross-thread reject/recover, lock inversion and worker handoff passed\n";
}
