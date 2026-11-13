#include "Rendering/Backend/NativeRenderDispatchReceipt.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <cassert>
#include <iostream>
using namespace rendering;
struct Owner {
 std::thread::id main;
 unsigned* destroyed;
 ~Owner(){assert(std::this_thread::get_id()==main);++*destroyed;}
};
int main(){
 NativeSerialRenderWorker worker;NativeRendererAccessGate gate;
 const auto main=std::this_thread::get_id();unsigned destroyed=0;
 for(unsigned i=0;i<1000;++i){
  const unsigned failure=i%4;unsigned targets=0,retired=0;
  ScreenFrameView screen;std::vector<ScreenRenderTargetPass> passes(2);screen.targetPasses=passes;
  auto owner=std::make_shared<Owner>();owner->main=main;owner->destroyed=&destroyed;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(&screen,[owner=std::move(owner),&targets,&retired,failure,main]{
   assert(std::this_thread::get_id()==main&&targets==1);++retired;
   if(failure==3)throw std::runtime_error("main retirement");
  });
  passes.clear();receipt->Reserve(gate);
  bool repeated=false;try{receipt->Reserve(gate);}catch(const std::logic_error&){repeated=true;}assert(repeated);
  auto deliver=[&]{
   receipt->ReleaseHandoff();
   NativeRendererAccessGate::Scope source(&gate); // No runtime/gate ownership remains.
   receipt->DeliverTarget([&](auto owned,uint64_t submission,auto)noexcept{assert(std::this_thread::get_id()==main&&owned.size()==2&&submission==17);++targets;});
   receipt->DeliverRetirement();
  };
  try{
   try{worker.Invoke([receipt,failure,main]{
    assert(std::this_thread::get_id()!=main);
    NativeRendererAccessGate::Scope admitted(receipt->Handoff());
    if(failure==1)throw std::runtime_error("pre Begin");
    receipt->MarkBegun(17);receipt->MarkRetirementReady();
    if(failure==2)throw std::runtime_error("post Begin");
   });deliver();}catch(...){deliver();throw;}
  }catch(const std::runtime_error&){}
  // Retry settlement never duplicates either notice, including thrown callback.
  deliver();assert(targets==(failure==1?0u:1u)&&retired==(failure==1?0u:1u));
  receipt.reset();assert(destroyed==i+1);
 }
 // Preparation/enqueue failure before worker execution releases reservation on main.
 {auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});receipt->Reserve(gate);}
 NativeRendererAccessGate::Scope recovered(&gate);
 std::cout<<"1000 owned dispatch receipts: exact target-before-retire, pre/post Begin errors, throwing callback, main destruction; reservation recovery PASS\n";
}
