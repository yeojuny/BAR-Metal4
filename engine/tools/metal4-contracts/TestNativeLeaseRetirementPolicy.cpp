#include "Rendering/Backend/NativeLeaseRetirementPolicy.h"
#include "Rendering/Backend/NativeRendererControl.h"
#include <cassert>
#include <iostream>
#include <mutex>
using namespace rendering;
template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
int main(){
 const auto main=std::this_thread::get_id();int calls=0;
 for(bool pending:{false,true})for(bool preparation:{false,true}){
  int before=calls;ApplyNativeLeaseRetirementPolicy(false,preparation,pending,[&]{assert(std::this_thread::get_id()==main);++calls;});
  assert(calls==before+!preparation);
  before=calls;if(pending)Reject([&]{ApplyNativeLeaseRetirementPolicy(true,preparation,pending,[&]{++calls;});});
  else ApplyNativeLeaseRetirementPolicy(true,preparation,pending,[&]{++calls;});assert(calls==before);
 }
 Reject([&]{ApplyNativeLeaseRetirementPolicy(false,false,true,[]{throw std::logic_error("retirement failed");});});
 NativeRendererAccessGate gate;std::unique_ptr<NativeSerialRenderWorker> worker;std::mutex runtime;
 int linked=0,drained=0;
 for(int i=0;i<1000;++i){
  bool queue=true,compat=true;
  {NativeRendererAccessGate::Scope source(&gate);std::lock_guard lock(runtime);
   ApplyNativeLeaseRetirementPolicy(false,false,queue||compat,[&]{assert(std::this_thread::get_id()==main);queue=compat=false;++drained;});}
  auto invoke=[&](bool fail){return InvokeNativeRendererControl(worker,gate,i,[&](int request,const auto& owner){
   assert(owner.Acquired()&&std::this_thread::get_id()!=main);std::lock_guard lock(runtime);
   ApplyNativeLeaseRetirementPolicy(true,false,queue||compat,[]{assert(false&&"control drained a source callback");});
   if(fail)throw std::logic_error("compiler failure");++linked;return request;
  });};
  assert(invoke(false)==i);Reject([&]{invoke(true);});
  for(int which=0;which<2;++which){queue=which==0;compat=which==1;int before=linked;Reject([&]{invoke(false);});assert(linked==before);}
  queue=compat=false;assert(invoke(false)==i);
 }
 assert(linked==2000&&drained==1000);
 std::cout<<"Actual retirement policy/control/worker:1000 main-drain, queue/compat rejection, throw recovery; default/preparation policy matrix PASS. Runtime mutex is fixture, no Metal execution.\n";
}
