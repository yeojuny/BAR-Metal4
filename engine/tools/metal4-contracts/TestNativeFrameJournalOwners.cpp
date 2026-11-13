#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeFrameAdmissionTransaction.h"
#include "Rendering/Backend/NativeFrameBeginReceipt.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeVolumetricCloudContext.h"
#include <iostream>
#include <stdexcept>
#include <functional>
using namespace rendering;
static unsigned checks=0;
static void Check(bool ok){++checks;if(!ok)throw std::runtime_error("frame journal owner mismatch");}
static void Reject(const std::function<void()>& operation){bool rejected=false;try{operation();}catch(const std::runtime_error&){rejected=true;}Check(rejected);}
int main(){
 NativePostprocessRecorder recorder;
 const NativePostprocessRect rect{0,0,8,8};
 auto binding=CreateNativePostprocessBinding(8,8,NativePostprocessFormat::RGBA8,{},false,16384);
 auto capture=[&](std::uint64_t source){
  recorder.BeginFrame(8,8,source);
  auto version=recorder.Copy(binding,NativePostprocessOperationKind::CopyColor,rect,rect,0);
  auto frame=recorder.EndFrame();recorder.RecordSubmission(*frame,source+100);
  return std::pair{frame,version};
 };
 auto [a,va]=capture(10);std::weak_ptr<const NativePostprocessFrame> wa=a;
 recorder.ValidateSubmittedCapacity(a->serial);Reject([&]{recorder.ValidateHeldFrame(a->serial);});
 recorder.HoldSubmittedFrame(a->serial);recorder.ValidateHeldFrame(a->serial);Check(recorder.OwnsPendingVersion(va));
 auto [b,vb]=capture(20);std::weak_ptr<const NativePostprocessFrame> wb=b;
 const auto aid=a->serial,bid=b->serial;
 recorder.HoldSubmittedFrame(bid);a.reset();b.reset();
 Check(!wa.expired()&&!wb.expired());Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vb));
 // Rejection must not consume pending publications; after exact retirement it
 // can be held. Abort of a new capture must not retire either old frame.
 auto [c,vc]=capture(30);const auto cid=c->serial;Check(!recorder.HasSubmittedCapacity(cid));Reject([&]{recorder.ValidateSubmittedCapacity(cid);});Reject([&]{recorder.ValidateHeldFrame(cid);});Reject([&]{recorder.HoldSubmittedFrame(cid);});
 Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vb)&&recorder.OwnsPendingVersion(vc));
 recorder.RetireCompletedFrame(bid);Check(recorder.HasSubmittedCapacity(cid));Check(wb.expired()&&!wa.expired());Check(!recorder.OwnsPendingVersion(vb)&&recorder.OwnsPendingVersion(va));
 recorder.HoldSubmittedFrame(cid);c.reset();
 Reject([&]{recorder.RetireCompletedFrame(bid);});Reject([&]{recorder.RetireCompletedFrame(0);});
 recorder.BeginFrame(8,8,40);auto aborted=recorder.Copy(binding,NativePostprocessOperationKind::CopyColor,rect,rect,0);
 Check(recorder.OwnsPendingVersion(aborted));recorder.AbortFrame();
 Check(!recorder.OwnsPendingVersion(aborted));Check(binding->current==vc);
 Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vc));
 recorder.Reset();Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vc));
 recorder.RetireCompletedFrame(aid);Check(wa.expired()&&!recorder.OwnsPendingVersion(va));
 recorder.RetireCompletedFrame(cid);Check(!recorder.OwnsPendingVersion(vc));
 // Existing synchronous pending completion remains legal.
 auto [d,vd]=capture(50);recorder.RetireCompletedFrame(d->serial);Check(!recorder.OwnsPendingVersion(vd));
 NativeVolumetricCloudRecorder clouds;
 clouds.BeginFrame(10);auto ca=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> cwa=ca;
 clouds.ValidateSubmittedCapacity(10);Reject([&]{clouds.ValidateHeldFrame(10);});clouds.HoldSubmittedFrame(10);clouds.ValidateHeldFrame(10);ca.reset();
 clouds.BeginFrame(20);auto cb=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> cwb=cb;
 clouds.HoldSubmittedFrame(20);cb.reset();Check(!cwa.expired()&&!cwb.expired());
 clouds.BeginFrame(30);auto cc=clouds.EndFrame();Check(!clouds.HasSubmittedCapacity(30));Reject([&]{clouds.ValidateSubmittedCapacity(30);});Reject([&]{clouds.ValidateHeldFrame(30);});Reject([&]{clouds.HoldSubmittedFrame(30);});
 clouds.RetireCompletedFrame(20);Check(clouds.HasSubmittedCapacity(30));Check(cwb.expired()&&!cwa.expired());clouds.HoldSubmittedFrame(30);
 clouds.BeginFrame(40);clouds.AbortFrame();clouds.Reset();Check(!cwa.expired());
 Reject([&]{clouds.RetireCompletedFrame(20);});Reject([&]{clouds.RetireCompletedFrame(0);});
 clouds.RetireCompletedFrame(10);Check(cwa.expired());clouds.RetireCompletedFrame(30);
 clouds.BeginFrame(50);auto cd=clouds.EndFrame();clouds.RetireCompletedFrame(50);
 // Preserve original cloud API's zero-valued source frame acceptance.
 clouds.BeginFrame(0);auto zero=clouds.EndFrame();clouds.HoldSubmittedFrame(0);clouds.RetireCompletedFrame(0);
 Reject([&]{clouds.RetireCompletedFrame(0);});
 // Cloud callers historically may repeat a source ID; reject a second Hold
 // without consuming pending, and preserve held-first completion precedence.
 clouds.BeginFrame(55);auto old55=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> weak55=old55;
 clouds.HoldSubmittedFrame(55);old55.reset();
 clouds.BeginFrame(55);auto pending55=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> weakPending55=pending55;
 Reject([&]{clouds.HoldSubmittedFrame(55);});pending55.reset();
 clouds.RetireCompletedFrame(55);Check(weak55.expired()&&!weakPending55.expired());
 clouds.HoldSubmittedFrame(55);clouds.RetireCompletedFrame(55);Check(weakPending55.expired());
 // Validate every held owner before retirement: a missing later cloud owner
 // must leave an earlier valid postprocess owner and its publication alive.
 auto [guarded,guardedVersion]=capture(58);auto guardedID=guarded->serial;
 std::weak_ptr<const NativePostprocessFrame> guardedWeak=guarded;
 recorder.HoldSubmittedFrame(guardedID);guarded.reset();
 Reject([&]{recorder.ValidateHeldFrame(guardedID);clouds.ValidateHeldFrame(58);});
 Check(!guardedWeak.expired()&&recorder.OwnsPendingVersion(guardedVersion));
 recorder.ValidateHeldFrame(guardedID);recorder.RetireCompletedFrame(guardedID);
 Check(guardedWeak.expired());Reject([&]{recorder.ValidateHeldFrame(guardedID);});
 // Early transfer is reversible before Begin and after synchronous completion.
 for(unsigned failure=0;failure<4;++failure){
  NativePostprocessRecorder early;NativeVolumetricCloudRecorder cloud;
  auto cell=CreateNativePostprocessBinding(8,8,NativePostprocessFormat::RGBA8,{},false,16384);auto previous=cell->current;
  early.BeginFrame(8,8,80);auto next=early.Copy(cell,NativePostprocessOperationKind::CopyColor,rect,rect,0);auto frame=early.EndFrame();
  cloud.BeginFrame(80);auto cloudFrame=cloud.EndFrame();
  early.ValidateSubmittedCapacity(frame->serial);cloud.ValidateSubmittedCapacity(80);
  early.TransferPreparedFrame(frame->serial);cloud.TransferPreparedFrame(80);
  Check(early.OwnsPendingVersion(next));
  // No-Begin throw/fallback restores journals, not original values yet.
  // The original Abort then rolls publications back in its normal order.
  if(failure<2){
   early.ValidateRestoreHeldFrame(frame->serial);cloud.ValidateRestoreHeldFrame(80);
   early.RestoreHeldFrame(frame->serial);cloud.RestoreHeldFrame(80);
   Check(cell->current==next);early.AbortFrame();cloud.AbortFrame();Check(cell->current==previous);
  }else if(failure==2){
   // Synchronous completed fallback restores for historical pending retirement.
   early.ValidateRestoreHeldFrame(frame->serial);cloud.ValidateRestoreHeldFrame(80);
   early.RestoreHeldFrame(frame->serial);cloud.RestoreHeldFrame(80);
   early.RetireCompletedFrame(frame->serial);cloud.RetireCompletedFrame(80);Check(cell->current==next);
  }else{
   early.QuarantineHeldFrame(frame->serial);cloud.QuarantineHeldFrame(80);
   Reject([&]{early.BeginFrame(8,8,81);});Reject([&]{cloud.BeginFrame(81);});
   Reject([&]{early.ValidateRestoreHeldFrame(frame->serial);});Reject([&]{cloud.ValidateRestoreHeldFrame(80);});
   early.Reset();cloud.Reset();Check(cell->current==next);Check(early.OwnsPendingVersion(next));
   Reject([&]{early.BeginFrame(8,8,82);});Reject([&]{cloud.BeginFrame(82);});
  }
 }
 // A younger pending capture may never be overwritten by restoration.
 {NativeVolumetricCloudRecorder c;c.BeginFrame(1);c.EndFrame();c.TransferPreparedFrame(1);c.BeginFrame(2);c.EndFrame();
  Reject([&]{c.ValidateRestoreHeldFrame(1);});c.ValidateHeldFrame(1);c.RetireCompletedFrame(2);c.ValidateRestoreHeldFrame(1);c.RestoreHeldFrame(1);c.RetireCompletedFrame(1);}
 // Exercise the production collective coordinator with actual journal owners.
 // UI's transfer ABI is checked at compile time; full UI runtime is not linked.
 static_assert(noexcept(std::declval<ui::NativeUIRecorder&>().TransferPreparedFrame(1)));
 static_assert(noexcept(std::declval<ui::NativeUIRecorder&>().RestoreHeldFrame(1)));
 for(unsigned rejectAt=0;rejectAt<3;++rejectAt){
  NativePostprocessRecorder first,second;NativeVolumetricCloudRecorder third;
  if(rejectAt==0){first.BeginFrame(8,8,1);first.EndFrame();first.AbortFrame();}
  if(rejectAt==1){second.BeginFrame(8,8,1);second.EndFrame();second.AbortFrame();}
  first.BeginFrame(8,8,1);auto f=first.EndFrame();second.BeginFrame(8,8,1);auto s=second.EndFrame();
  third.BeginFrame(rejectAt==2?2:1);third.EndFrame();
  NativeFrameAdmissionTransaction transaction(first,second,third,1,1);
  Reject([&]{transaction.Validate();});
  first.ValidateSubmittedCapacity(f->serial);second.ValidateSubmittedCapacity(s->serial);third.ValidateSubmittedCapacity(rejectAt==2?2:1);
 }
 for(unsigned failure=0;failure<6;++failure){
  NativePostprocessRecorder first,second;NativeVolumetricCloudRecorder third;
  first.BeginFrame(8,8,1);first.EndFrame();second.BeginFrame(8,8,1);second.EndFrame();third.BeginFrame(1);third.EndFrame();
  NativeFrameAdmissionTransaction transaction(first,second,third,1,1);NativeFrameBeginReceipt begun(1,1);
  transaction.Validate();transaction.TransferValidated();
  if(failure>=3)begun.MarkBegun(71);
  try {throw std::runtime_error(failure==0?"renderer pre-Begin":failure==1?"post-return drain":failure==2?"fallback settlement":"after Begin");}
  catch(...){if(begun.Submission())transaction.Quarantine();else transaction.Restore();}
  if(failure<3){first.ValidateSubmittedCapacity(1);second.ValidateSubmittedCapacity(1);third.ValidateSubmittedCapacity(1);}
  else{first.ValidateHeldFrame(1);second.ValidateHeldFrame(1);third.ValidateHeldFrame(1);
   Reject([&]{first.BeginFrame(8,8,2);});Reject([&]{second.BeginFrame(8,8,2);});Reject([&]{third.BeginFrame(2);});}
 }
 // Restore validates every owner before moving any payload back.
 {NativePostprocessRecorder first,second;NativeVolumetricCloudRecorder third;
  first.BeginFrame(8,8,1);first.EndFrame();second.BeginFrame(8,8,1);second.EndFrame();third.BeginFrame(1);third.EndFrame();
  NativeFrameAdmissionTransaction transaction(first,second,third,1,1);transaction.Validate();transaction.TransferValidated();
  third.BeginFrame(2);third.EndFrame();Reject([&]{transaction.Restore();});first.ValidateHeldFrame(1);second.ValidateHeldFrame(1);
  third.AbortFrame();transaction.Restore();first.ValidateSubmittedCapacity(1);second.ValidateSubmittedCapacity(1);}
 // Many cycles continue to reuse bounded slots and release exact completed data.
 for(std::uint64_t i=60;i<1060;i+=2){
  clouds.BeginFrame(i);auto x=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> wx=x;clouds.HoldSubmittedFrame(i);x.reset();
  clouds.BeginFrame(i+1);auto y=clouds.EndFrame();clouds.HoldSubmittedFrame(i+1);
  clouds.RetireCompletedFrame(i+1);Check(!wx.expired());clouds.RetireCompletedFrame(i);Check(wx.expired());
 }
 std::cout<<checks<<" actual postprocess/cloud owner checks passed\n";
}
