#include "Rendering/Backend/NativeDynamicWaterReceipts.h"
#include "Rendering/Backend/NativeFrameAdmissionTransaction.h"
#include <cassert>
#include <iostream>
using namespace rendering;
using Frame=NativeDynamicWaterFrame;
template<class F>void Reject(F&& f){try{f();assert(false);}catch(const std::logic_error&) {}}
static std::shared_ptr<const Frame> Source(std::shared_ptr<const NativeDynamicWaterMaterial> material,uint64_t draw,uint64_t through){
 auto f=std::make_shared<Frame>();f->material=std::move(material);f->sourceDrawFrame=draw;f->throughUpdate=through;return f;
}
static std::shared_ptr<const Frame> Reserve(NativeDynamicWaterReceipts& r,std::shared_ptr<const NativeDynamicWaterMaterial> m,uint64_t draw,uint64_t through){
 auto f=Source(std::move(m),draw,through);r.ValidateReserve(f);r.Reserve(f);return f;
}
static void Finalize(NativeDynamicWaterReceipts& r,const std::shared_ptr<const Frame>& f){r.SetFinalized(f,std::make_shared<const Frame>(*f));}
// The first three adapters model ownership movement; transaction and water
// ledger are production implementations. No UI recorder runtime is claimed.
struct OtherOwner {
 int held=0,moves=0;bool quarantine=false;
 void ValidateSubmittedCapacity(uint64_t)const{if(held)throw std::logic_error("busy");}
 void TransferPreparedFrame(uint64_t)noexcept{held=1;++moves;}
 void ValidateRestoreHeldFrame(uint64_t)const{if(held!=1||quarantine)throw std::logic_error("not held");}
 void RestoreHeldFrame(uint64_t)noexcept{held=0;}
 void QuarantineHeldFrame(uint64_t)noexcept{quarantine=true;}
};
struct WaterOwner {
 NativeDynamicWaterReceipts& receipts;bool refuseRestore=false;
 void ValidateSubmittedCapacity(uint64_t d)const{receipts.ValidateHold(d);}
 void TransferPreparedFrame(uint64_t d)noexcept{receipts.Hold(d);}
 void ValidateRestoreHeldFrame(uint64_t d)const{if(refuseRestore)throw std::logic_error("injected restore reject");receipts.ValidateRestore(d);}
 void RestoreHeldFrame(uint64_t d)noexcept{receipts.Restore(d);}
 void QuarantineHeldFrame(uint64_t d)noexcept{receipts.Quarantine(d);}
};
int main(){
 static_assert(noexcept(std::declval<NativeDynamicWaterReceipts&>().Hold(1)));
 static_assert(noexcept(std::declval<NativeDynamicWaterReceipts&>().Restore(1)));
 static_assert(noexcept(std::declval<NativeDynamicWaterReceipts&>().Quarantine(1)));
 static_assert(noexcept(std::declval<NativeDynamicWaterReceipts&>().Complete(1)));
 for(unsigned n=0;n<1000;++n){
  auto material=std::make_shared<NativeDynamicWaterMaterial>();material->generation=17;
  NativeDynamicWaterReceipts r;assert(!r.Any());
  r.ValidateReset();auto a=Reserve(r,material,10,3);r.ValidateReset();assert(r.Current(10)==a&&!r.Finalized(10));
  Reject([&]{r.ValidateHold(10);});Reject([&]{r.ValidateCompletion(17,3,10,0,3);});
  auto forged=std::make_shared<Frame>(*a);Reject([&]{r.SetFinalized(forged,forged);});assert(r.Current(10)==a&&!r.Finalized(10));
  forged->throughUpdate=4;Reject([&]{r.SetFinalized(a,forged);});
  Finalize(r,a);auto finalA=r.Finalized(10);r.ValidateHold(10);r.Hold(10);r.ValidateHeld(10);Reject([&]{r.ValidateReset();});
  Reject([&]{r.ValidateHold(10);});r.ValidateRestore(10);r.Restore(10);assert(r.Finalized(10)==finalA);
  r.ValidateHold(10);r.Hold(10); // pre-Begin rollback followed by retry
  auto b=Reserve(r,material,11,5);Finalize(r,b);r.ValidateHold(11);r.Hold(11);
  auto c=Reserve(r,material,12,5);Finalize(r,c); // synchronous completion may have no new update
  Reject([&]{r.ValidateReserve(Source(material,13,6));});
  Reject([&]{r.ValidateReserve(Source(material,10,6));});
  Reject([&]{r.ValidateCompletion(18,3,10,0,5);});Reject([&]{r.ValidateCompletion(17,4,10,0,5);});
  Reject([&]{r.ValidateCompletion(17,3,10,4,5);});Reject([&]{r.ValidateCompletion(17,3,10,0,2);});
  Reject([&]{r.ValidateCompletion(17,5,11,0,5);});assert(r.Current(10)==a&&r.Current(11)==b);
  r.ValidateCompletion(17,3,10,0,5);r.Complete(10);assert(!r.Current(10)&&r.Current(11)==b&&r.Current(12)==c);
  Reject([&]{r.ValidateCompletion(17,3,10,3,5);});
  r.ValidateCompletion(17,5,11,3,5);r.Complete(11);
  r.ValidateCompletion(17,5,12,5,5);r.Complete(12);assert(!r.Any());
  auto d=Reserve(r,material,13,6);Finalize(r,d);r.ValidateHold(13);r.Hold(13);r.Quarantine(13);r.Quarantine(13);
  Reject([&]{r.ValidateCompletion(17,6,13,5,6);});Reject([&]{r.ValidateRestore(13);});Reject([&]{r.ValidateHeld(13);});
  Reject([&]{r.ValidateReset();});Reject([&]{r.ValidateReserve(Source(material,14,7));});assert(r.Current(13)==d&&r.Any());
 }
 NativeDynamicWaterReceipts r;Reject([&]{r.ValidateReserve({});});
 auto invalid=std::make_shared<NativeDynamicWaterMaterial>();Reject([&]{r.ValidateReserve(Source(invalid,1,0));});
 invalid->generation=1;auto a=Reserve(r,invalid,1,0);Finalize(r,a);
 auto changed=std::make_shared<NativeDynamicWaterMaterial>();changed->generation=2;
 Reject([&]{r.ValidateReserve(Source(changed,2,1));});assert(r.Current(1)==a);
 // Exact pre-Begin abort: no tuple-only guesses and no unrelated retirement.
 for(unsigned n=0;n<1000;++n){
  NativeDynamicWaterReceipts aborts;
  assert(aborts.CanAbort({})&&aborts.Abort({}));
  auto partial=Reserve(aborts,invalid,30,1);
  auto wrong=std::make_shared<const Frame>(*partial);
  assert(!aborts.CanAbort(wrong)&&!aborts.Abort(wrong)&&aborts.Current(30)==partial);
  assert(aborts.CanAbort(partial)&&aborts.Abort(partial)&&!aborts.Any());
  auto retry=Reserve(aborts,invalid,30,1);Finalize(aborts,retry);auto final=aborts.Finalized(30);
  assert(!aborts.Abort(partial)&&aborts.Current(30)==retry); // Same draw, wrong exact owner.
  auto younger=Reserve(aborts,invalid,31,2);Finalize(aborts,younger);
  assert(aborts.Abort(final)&&!aborts.Current(30)&&aborts.Current(31)==younger);
  assert(aborts.Abort(partial)&&aborts.Abort(final)&&aborts.Current(31)==younger); // stale no-op
  aborts.ValidateHold(31);aborts.Hold(31);
  assert(!aborts.CanAbort(younger)&&!aborts.Abort(younger));aborts.ValidateHeld(31);
  aborts.Quarantine(31);assert(!aborts.Abort(younger)&&aborts.Current(31)==younger);
 }
 // Zero draw was historically allowed by capture; retain that contract.
 NativeDynamicWaterReceipts zero;auto z=Reserve(zero,invalid,0,0);Finalize(zero,z);zero.ValidateReset();
 for(unsigned mode=0;mode<3;++mode){
  NativeDynamicWaterReceipts ledger;OtherOwner ui,post,cloud;WaterOwner water{ledger};
  auto f=Reserve(ledger,invalid,20,0);
  NativeFrameAdmissionTransaction transaction(ui,post,cloud,water,20,30);
  Reject([&]{transaction.Validate();});assert(ui.moves==0&&post.moves==0&&cloud.moves==0);
  Finalize(ledger,f);transaction.Validate();transaction.TransferValidated();ledger.ValidateHeld(20);
  assert(ui.held&&post.held&&cloud.held);
  water.refuseRestore=true;Reject([&]{transaction.Restore();});assert(ui.held&&post.held&&cloud.held);ledger.ValidateHeld(20);
  water.refuseRestore=false;
  if(mode==0){
   transaction.Restore();assert(!ui.held&&!post.held&&!cloud.held);
   transaction.Validate();transaction.TransferValidated();transaction.Restore(); // retry then synchronous completion
   ledger.ValidateCompletion(1,0,20,0,0);ledger.Complete(20);assert(!ledger.Any());
  }else if(mode==1){
   ledger.ValidateCompletion(1,0,20,0,0);ledger.Complete(20);assert(!ledger.Any()); // deferred completion from Held
  }else{
   transaction.Quarantine();assert(ui.quarantine&&post.quarantine&&cloud.quarantine);
   Reject([&]{ledger.ValidateCompletion(1,0,20,0,0);});Reject([&]{ledger.ValidateReset();});
  }
 }
 std::cout<<"Actual dynamic-water receipt ledger1000:exact owners,capacity,restore/retry,sync/held ordered retirement,younger preservation,duplicate/tuple rejection,quarantine,exact pre-Begin abort;no gates relaxed\n";
}
