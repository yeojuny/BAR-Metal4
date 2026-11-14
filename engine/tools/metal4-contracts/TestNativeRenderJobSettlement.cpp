#include "Rendering/Backend/NativeRenderJobSettlement.h"
#include <cassert>
#include <iostream>
using namespace rendering;
template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){
 for(unsigned i=0;i<1000;++i){
  NativeSerialRenderWorker worker;NativeRendererAccessGate gate;
  std::vector<int> order;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,[&]{NativeRendererAccessGate::Scope access(&gate);order.push_back(3);});
  NativeRenderJobSettlement<int>* current=nullptr;
  NativeRenderJobSettlement<int> job(receipt,[&]{NativeRendererAccessGate::Scope access(&gate);order.push_back(2);Reject([&]{current->Finish();});});current=&job;
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  job.Submit(worker,[&]{NativeRendererAccessGate::Scope access(receipt->Handoff());order.push_back(1);receipt->MarkRetirementReady();return 29;});
  job.Finish();assert((order==std::vector<int>{1,2,3}));
  // Draining before a control preserves the result for the scene owner.
  {NativeRendererAccessGate::Scope access(&gate);order.push_back(4);}
  job.Finish();assert(job.Consume()==29);Reject([&]{job.Consume();});
  assert(worker.Invoke([]{return 5;})==5);
 }
 for(unsigned mode=0;mode<4;++mode){
  NativeSerialRenderWorker worker;NativeRendererAccessGate gate;unsigned target=0,retired=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,[&]{++retired;if(mode==2||mode==3)throw std::runtime_error("retirement");});
  NativeRenderJobSettlement<int> job(receipt,[&]{NativeRendererAccessGate::Scope access(&gate);++target;if(mode==1)throw std::runtime_error("target");});
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  job.Submit(worker,[&]()->int{NativeRendererAccessGate::Scope access(receipt->Handoff());receipt->MarkRetirementReady();if(mode==0||mode==3)throw std::runtime_error("worker");return 7;});
  for(unsigned attempt=0;attempt<3;++attempt){try{job.Finish();assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())==((mode==0||mode==3)?"worker":mode==1?"target":"retirement"));}}
  assert(target==1&&retired==1);Reject([&]{job.Consume();});
  {NativeRendererAccessGate::Scope access(&gate);}
 }
 // Enqueue rejected before publication is terminal with the original cause,
 // and its reserved handoff is released without any callback delivery.
 {NativeSerialRenderWorker worker;auto first=worker.Enqueue([]{return 0;});NativeRendererAccessGate gate;unsigned delivered=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  NativeRenderJobSettlement<int> job(receipt,[&]{++delivered;});
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  Reject([&]{job.Submit(worker,[]{return 1;});});Reject([&]{job.Finish();});
  {NativeRendererAccessGate::Scope access(&gate);}assert(!delivered);first->Join();
 }
 // A discarded running job retains its reservation through task completion.
 {NativeSerialRenderWorker worker;NativeRendererAccessGate gate;unsigned notices=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  auto job=std::make_unique<NativeRenderJobSettlement<int>>(receipt,[&]{++notices;});
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  auto* handoff=&receipt->Handoff();receipt.reset();
  job->Submit(worker,[handoff]{NativeRendererAccessGate::Scope access(*handoff);return 1;});
  job.reset();{NativeRendererAccessGate::Scope access(&gate);}assert(!notices);Reject([&]{worker.Invoke([]{});});
 }
 // Exact scene notification follows both receipts, and first error dominates.
 for(bool workerFails:{false,true}){
  NativeSerialRenderWorker worker;unsigned called=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,std::function<void()>{});
  NativeRenderJobSettlement<int>* live=nullptr;
  NativeRenderJobSettlement<int> job(receipt,[]{},[&](const int* result,std::exception_ptr failure){
   ++called;Reject([&]{live->Finish();});assert(bool(failure)==workerFails);assert(bool(result)!=workerFails);
   if(result)assert(*result==33);throw std::runtime_error("scene");
  });live=&job;
  job.Submit(worker,[&]{if(workerFails)throw std::runtime_error("worker");return 33;});
  for(unsigned i=0;i<2;++i){try{job.Finish();assert(false);}catch(const std::runtime_error& e){assert(std::string(e.what())==(workerFails?"worker":"scene"));}}
  assert(called==1);
 }
 // Actual direct-fallback helper: scopes unwind before notification; failure
 // notification cannot replace the render exception or repeat a source notice.
 for(unsigned mode=0;mode<3;++mode){unsigned notified=0;bool scope=false;
  try{InvokeNativeSynchronousCPUJob([&]{struct Scope{bool& active;Scope(bool& b):active(b){active=true;}~Scope(){active=false;}} live(scope);if(mode==0)throw std::runtime_error("direct");return 7;},
   [&](const int* result,std::exception_ptr error){++notified;assert(!scope);assert(bool(error)==(mode==0));assert(bool(result)==(mode!=0));if(mode!=2)throw std::runtime_error("notice");});assert(mode==2);}
  catch(const std::runtime_error& e){assert(std::string(e.what())==(mode==0?"direct":"notice"));}
  assert(notified==1);
 }
 // Early preparation release does not wait for Begin or deliver notices.
 // Main advances a bounded capture prefix while the worker owns preparation.
 for(bool failBeforeBegin:{false,true}){
  NativeSerialRenderWorker worker;NativeRendererAccessGate gate;
  std::atomic<bool> prefixAdvanced=false;unsigned target=0,retired=0,settled=0;
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,[&]{++retired;});
  NativeRenderJobSettlement<int> job(receipt,[&]{++target;},[&](const int* result,std::exception_ptr error){++settled;assert(bool(error)==failBeforeBegin);assert(bool(result)!=failBeforeBegin);});
  {NativeRendererAccessGate::Scope access(&gate);receipt->Reserve(gate);}
  job.Submit(worker,[&]{NativeRendererAccessGate::Scope access(receipt->Handoff());
   while(!prefixAdvanced.load(std::memory_order_acquire))std::this_thread::yield();
   receipt->MarkRetirementReady();if(failBeforeBegin)throw std::runtime_error("prepare");return 11;
  });
  assert(job.ReleaseForPreparation());assert(target==0&&retired==0&&settled==0);
  Reject([&]{NativeRendererAccessGate::Scope access(&gate);});
  prefixAdvanced.store(true,std::memory_order_release);
  if(failBeforeBegin)Reject([&]{job.Finish();});else {job.Finish();assert(job.Consume()==11);}
  assert(target==1&&retired==1&&settled==1);
 }
 std::cout<<"1000 CPU Join/handoff/notice/control cycles; recursive drain, result preservation, persistent first failure, single callback attempt, enqueue rejection PASS\n";
}
