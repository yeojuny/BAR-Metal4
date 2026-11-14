#include "Rendering/Backend/NativeBeginReadyPolicy.h"
#include "Rendering/Backend/NativeRenderJobSettlement.h"
#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include <cassert>
#include <iostream>
using namespace rendering;
template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){
 NativeBeginReadyPolicy valid{true,true,true,true,true,true,true,true,true};assert(valid.Eligible());
 for(auto flag:{&NativeBeginReadyPolicy::requested,&NativeBeginReadyPolicy::worker,&NativeBeginReadyPolicy::owned,&NativeBeginReadyPolicy::visible,&NativeBeginReadyPolicy::admitted,&NativeBeginReadyPolicy::sourceCredit,&NativeBeginReadyPolicy::deferred,&NativeBeginReadyPolicy::screen,&NativeBeginReadyPolicy::cpuSettlement}){auto bad=valid;bad.*flag=false;assert(!bad.Eligible());}
 for(auto flag:{&NativeBeginReadyPolicy::readback,&NativeBeginReadyPolicy::continuation,&NativeBeginReadyPolicy::dynamicWater,&NativeBeginReadyPolicy::legacyWater,&NativeBeginReadyPolicy::arbitraryCallback,&NativeBeginReadyPolicy::previousRetirement}){auto bad=valid;bad.*flag=true;assert(!bad.Eligible());}
 auto preparation=valid;preparation.previousRetirement=true;
 assert(!preparation.Eligible()&&preparation.EligibleForPreparation());
 preparation.readback=true;assert(!preparation.EligibleForPreparation());
 preparation.readback=false;preparation.dynamicWater=true;assert(!preparation.EligibleForPreparation());
 for(unsigned n=0;n<100;++n){
  NativeSourceFrameCredits credits;NativeSerialRenderWorker worker;NativeRendererAccessGate gate;
  auto frame=credits.Acquire(0);credits.Hold(frame);unsigned targets=0,settled=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  NativeRenderJobSettlement<int> job(receipt,[&]{receipt->DeliverTarget([&](auto,uint64_t id,auto)noexcept{assert(id==31);++targets;});},[&](const int* result,std::exception_ptr error){assert(result&&*result==17&&!error);++settled;});
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  std::promise<void> finish;auto wait=finish.get_future();
  job.Submit(worker,[receipt,wait=std::move(wait)]()mutable{struct End{decltype(receipt)& r;~End(){r->MarkWorkerFinished();}} end{receipt};NativeRendererAccessGate::Scope access(receipt->Handoff());receipt->MarkBegun(31);receipt->PublishBeginReady();wait.wait();return 17;});
  assert(job.ReleaseAfterBegin());assert(targets==1&&settled==0);
  auto nextCapture=credits.Acquire(1);assert(!credits.HasCapacity());Reject([&]{credits.Acquire(2);});
  // Reserved worker ownership persists while source capture proceeds.
  {NativeRendererAccessGate::Scope access(&gate,NativeRendererAccessGate::Try{});assert(!access.Acquired());}
  finish.set_value();job.Finish();assert(settled==1&&targets==1);assert(job.Consume()==17);frame.reset();
  assert(!credits.HasCapacity()); // Joined CPU != retired GPU.
  credits.ValidateHeld(0);credits.ReleaseHeld(0);assert(credits.HasCapacity());
 }
 {NativeSerialRenderWorker worker;auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  NativeRenderJobSettlement<int> job(receipt,[]{});job.Submit(worker,[receipt]()->int{receipt->MarkWorkerFinished();throw std::runtime_error("before Begin");});
  assert(!job.ReleaseAfterBegin());Reject([&]{job.Finish();});
 }
 std::cout<<"Begin-ready eligibility15 gates +100 actual worker release/capture/Join/source-credit cycles; pre-Begin fallback PASS (not native overlap/performance evidence)\n";
}
