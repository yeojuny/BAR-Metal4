#include "Rendering/Backend/NativePostprocessRecorder.h"
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
 recorder.HoldSubmittedFrame(a->serial);Check(recorder.OwnsPendingVersion(va));
 auto [b,vb]=capture(20);std::weak_ptr<const NativePostprocessFrame> wb=b;
 const auto aid=a->serial,bid=b->serial;
 recorder.HoldSubmittedFrame(bid);a.reset();b.reset();
 Check(!wa.expired()&&!wb.expired());Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vb));
 // Rejection must not consume pending publications; after exact retirement it
 // can be held. Abort of a new capture must not retire either old frame.
 auto [c,vc]=capture(30);const auto cid=c->serial;Reject([&]{recorder.HoldSubmittedFrame(cid);});
 Check(recorder.OwnsPendingVersion(va)&&recorder.OwnsPendingVersion(vb)&&recorder.OwnsPendingVersion(vc));
 recorder.RetireCompletedFrame(bid);Check(wb.expired()&&!wa.expired());Check(!recorder.OwnsPendingVersion(vb)&&recorder.OwnsPendingVersion(va));
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
 clouds.HoldSubmittedFrame(10);ca.reset();
 clouds.BeginFrame(20);auto cb=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> cwb=cb;
 clouds.HoldSubmittedFrame(20);cb.reset();Check(!cwa.expired()&&!cwb.expired());
 clouds.BeginFrame(30);auto cc=clouds.EndFrame();Reject([&]{clouds.HoldSubmittedFrame(30);});
 clouds.RetireCompletedFrame(20);Check(cwb.expired()&&!cwa.expired());clouds.HoldSubmittedFrame(30);
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
 // Many cycles continue to reuse bounded slots and release exact completed data.
 for(std::uint64_t i=60;i<1060;i+=2){
  clouds.BeginFrame(i);auto x=clouds.EndFrame();std::weak_ptr<const NativeVolumetricCloudFrame> wx=x;clouds.HoldSubmittedFrame(i);x.reset();
  clouds.BeginFrame(i+1);auto y=clouds.EndFrame();clouds.HoldSubmittedFrame(i+1);
  clouds.RetireCompletedFrame(i+1);Check(!wx.expired());clouds.RetireCompletedFrame(i);Check(wx.expired());
 }
 std::cout<<checks<<" actual postprocess/cloud owner checks passed\n";
}
