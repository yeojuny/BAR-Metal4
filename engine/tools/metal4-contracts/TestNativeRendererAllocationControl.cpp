#include "Rendering/Backend/NativeRendererControl.h"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace rendering;
template<class F>void Reject(F f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
int main(){
 NativeRendererAccessGate gate;std::unique_ptr<NativeSerialRenderWorker> worker;
 const auto main=std::this_thread::get_id();std::thread::id executor;
 for(unsigned i=0;i<1000;++i){
  auto request=std::make_shared<const unsigned>(i);
  auto result=InvokeNativeRendererControl(worker,gate,request,[&](const auto& exact,const NativeRendererAccessGate::Scope& owner){
   assert(owner.Acquired()&&std::this_thread::get_id()!=main);executor=std::this_thread::get_id();
   NativeRendererAccessGate::Scope recursive(&gate);assert(recursive.Acquired());return exact;
  });
  assert(result==request&&*result==i);
  Reject([&]{InvokeNativeRendererControl(worker,gate,request,[](const auto&,const auto&)->int{throw std::logic_error("allocation failure");});});
  assert(InvokeNativeRendererControl(worker,gate,i,[](auto n,const auto&){return n;})==i);
 }
 // Foreign cache ownership fails before changing/creating the worker.
 std::promise<void> entered,release;auto released=release.get_future();
 std::thread foreign([&]{NativeRendererAccessGate::Scope scope(&gate);entered.set_value();released.wait();});
 entered.get_future().wait();auto* previous=worker.get();
 Reject([&]{InvokeNativeRendererControl(worker,gate,1,[](auto,const auto&){return 1;});});
 assert(worker.get()==previous);release.set_value();foreign.join();
 // An unsupported outer main scope cannot be handed across threads. It rejects
 // rather than deadlocking, and the outer owner remains usable afterward.
 {NativeRendererAccessGate::Scope outer(&gate);
  Reject([&]{InvokeNativeRendererControl(worker,gate,1,[](auto,const auto&){return 1;});});
  NativeRendererAccessGate::Scope again(&gate);assert(again.Acquired());}
 bool destroyed=false;std::thread::id destroyedOn;
 auto source=std::shared_ptr<const int>(new int(7),[&](const int* p){destroyedOn=std::this_thread::get_id();destroyed=true;delete p;});
 auto result=InvokeNativeRendererControl(worker,gate,std::move(source),[](const auto& exact,const auto&){return exact;});
 assert(!destroyed&&*result==7);result.reset();assert(destroyed&&destroyedOn==main);
 destroyed=false;
 auto failureSource=std::shared_ptr<const int>(new int(8),[&](const int* p){destroyedOn=std::this_thread::get_id();destroyed=true;delete p;});
 Reject([&]{InvokeNativeRendererControl(worker,gate,std::move(failureSource),[](const auto&,const auto&)->int{throw std::logic_error("failure");});});
 assert(destroyed&&destroyedOn==main);
 assert(InvokeNativeRendererControl(worker,gate,1,[&](auto,const auto&){return std::this_thread::get_id()==executor;}));
 std::cout<<"Actual allocation control/gate/worker:1000 success+failure recoveries; concurrent+nested rejection; exact owning result/main destruction PASS\n";
}
