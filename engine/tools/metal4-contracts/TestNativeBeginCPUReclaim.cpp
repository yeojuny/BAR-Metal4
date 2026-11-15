#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include "Rendering/Backend/NativeRenderJobSettlement.h"
#include "Rendering/Metal4/Metal4SceneBeginRetirement.h"
#include <atomic>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>

using namespace rendering;
static_assert(!std::is_constructible_v<Metal4SceneBeginRetirement,
 NativeSourceFrameCredits::Lease,std::shared_ptr<NativeFrameBeginReceipt>,
 std::shared_ptr<NativeFrameCompletionMailbox>,std::function<void()>>);

int main() {
 // The real serial worker may publish Begin while its encoding tail is still
 // running. The next two-credit admission joins that tail and applies only the
 // predecessor's accepted GPU notice, leaving the current GPU credit held.
 for (unsigned i=0;i<1000;++i) {
  NativeSourceFrameCredits credits(2);
  NativeFrameCompletionMailbox mailbox(9);
  auto predecessor=credits.Acquire(100), current=credits.Acquire(101);
  credits.Hold(predecessor);credits.Hold(current);
  auto oldTicket=mailbox.Prepare(9,100,200);
  mailbox.Accept(oldTicket);
  auto currentTicket=mailbox.Prepare(9,101,201);
  std::weak_ptr<const NativeSourceFrameCredits::Token> oldLife=predecessor;
  predecessor.reset();
  std::atomic<bool> encodeTailAllowed=false;
  NativeSerialRenderWorker worker;
  NativeRendererAccessGate gate;
  unsigned targets=0,oldRetirements=0,currentSettlements=0,gpuDrains=0;
  const auto main=std::this_thread::get_id();
  auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,[&]{
   assert(std::this_thread::get_id()==main);
   auto notice=mailbox.Pop();
   assert(notice && notice->sourceDrawFrame==100 && notice->completedPostprocessSerial==200);
   credits.ValidateHeld(100);credits.ReleaseHeld(100);++oldRetirements;
   // Current GPU work has not published a completion and remains held.
   assert(!mailbox.Pop());credits.ValidateHeld(101);
  });
  NativeRenderJobSettlement<int> job(receipt,[&]{
   receipt->DeliverTarget([&](auto, std::uint64_t submission, auto) noexcept {
    assert(std::this_thread::get_id()==main && submission==77);++targets;
   });
  },[&](const int* result,std::exception_ptr failure){
   assert(std::this_thread::get_id()==main && !failure && result && *result==7);
   assert(oldRetirements==1);
   mailbox.Accept(currentTicket);++currentSettlements;
  });
  receipt->Reserve(gate);
  job.Submit(worker,[&]{
   NativeRendererAccessGate::Scope access(receipt->Handoff());
   receipt->MarkBegun(77);receipt->PublishBeginReady();
   while(!encodeTailAllowed.load(std::memory_order_acquire))std::this_thread::yield();
   // SubmitAndDefer retires the predecessor before committing this frame.
   mailbox.Publish(oldTicket,{true,100,200});
   receipt->MarkRetirementReady();
   return 7;
  });
  assert(job.ReleaseAfterBegin());
  assert(targets==1 && oldRetirements==0 && currentSettlements==0);
  assert(!credits.HasCapacity());
  encodeTailAllowed.store(true,std::memory_order_release);
  auto next=credits.AcquireWithCPUReclaim(102,true,[&]{
   job.Finish();assert(job.Consume()==7);
  },[&]{++gpuDrains;});
  assert(next && targets==1 && oldRetirements==1 && currentSettlements==1 && gpuDrains==0);
  assert(oldLife.expired());
  assert(next->Draw()==102 && !credits.HasCapacity());
  credits.ValidateHeld(101);
 }

 // A surviving CPU alias can keep the old weak slot occupied even after its
 // held GPU credit is released; the helper must recheck and take full drain.
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits credits(2);
  auto old=credits.Acquire(7),current=credits.Acquire(8);
  credits.Hold(old);credits.Hold(current);
  unsigned joins=0,drains=0;
  current.reset();
  auto next=credits.AcquireWithCPUReclaim(9,true,[&]{
   ++joins;credits.ReleaseHeld(7); // `old` is an intentional surviving alias.
   assert(!credits.HasCapacity());
  },[&]{++drains;credits.ReleaseHeld(8);});
  assert(next && joins==1 && drains==1 && next->Draw()==9);
 }

 // Gate 0 retains the original full-drain-only route. A settlement exception
 // never fabricates capacity or invokes the fallback drain after failure.
 {
  NativeSourceFrameCredits credits(2);
  auto old=credits.Acquire(1),current=credits.Acquire(2);
  credits.Hold(old);credits.Hold(current);old.reset();current.reset();
  unsigned joins=0,drains=0;
  auto next=credits.AcquireWithCPUReclaim(3,false,[&]{++joins;},[&]{
   ++drains;credits.ReleaseHeld(1);
  });
  assert(next && joins==0 && drains==1);
 }
 {
  NativeSourceFrameCredits credits(2);
  auto old=credits.Acquire(1),current=credits.Acquire(2);
  credits.Hold(old);credits.Hold(current);old.reset();current.reset();
  unsigned drains=0;
  bool failed=false;
  try{(void)credits.AcquireWithCPUReclaim(3,true,[]{throw std::runtime_error("CPU settlement failed");},[&]{++drains;});}
  catch(const std::runtime_error& e){failed=std::string_view(e.what())=="CPU settlement failed";}
  assert(failed && drains==0 && !credits.HasCapacity());
 }
 // A CPU join with no published predecessor completion cannot claim credit;
 // the helper must invoke its original full drain fallback.
 {
  NativeSourceFrameCredits credits(2);
  auto old=credits.Acquire(11),current=credits.Acquire(12);
  credits.Hold(old);credits.Hold(current);old.reset();current.reset();
  unsigned joins=0,drains=0;
  auto next=credits.AcquireWithCPUReclaim(13,true,[&]{++joins;assert(!credits.HasCapacity());},[&]{
   ++drains;credits.ReleaseHeld(11);
  });
  assert(next && joins==1 && drains==1);
 }
 // Quarantine rejects even an available slot before attempting any callback.
 {
  NativeSourceFrameCredits credits(2);
  auto old=credits.Acquire(21);credits.Hold(old);credits.QuarantineHeld(21);
  bool rejected=false;unsigned joins=0,drains=0;
  try{(void)credits.AcquireWithCPUReclaim(22,true,[&]{++joins;},[&]{++drains;});}
  catch(const std::logic_error&){rejected=true;}
  assert(rejected && joins==0 && drains==0);
 }
 std::cout<<"BEGIN CPU-only credit reclaim: real serial Begin/Join/notice/mailbox, 2-credit rotation, surviving alias fallback, gate0 and settlement failure PASS\n";
}
