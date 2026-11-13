/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeRendererAccessGate.h"
#include "NativeTargetSubmissionNotice.h"
#include <functional>
#include <atomic>
#include <optional>
namespace rendering {
// Owned dispatch boundary. Worker callbacks retain this object, never caller
// stack references. Begin-ready permits only target-notice delivery on main.
// Gate release and retirement settlement still require Join. Not a renderer owner.
class NativeRenderDispatchReceipt {
 NativeTargetSubmissionNotice target;
 std::function<void()> previousRetired;
 std::atomic<bool> receiptReady{false};
 std::atomic<unsigned> progress{0}; // 1: complete Begin notice, 2: worker finished
 std::optional<NativeRendererAccessGate::Handoff> handoff;
public:
 NativeRenderDispatchReceipt(const ScreenFrameView* screen,std::function<void()> prior):target(screen),previousRetired(std::move(prior)) {}
 void Reserve(NativeRendererAccessGate& gate){if(handoff)throw std::logic_error("Dispatch receipt already reserved");handoff.emplace(gate);}
 NativeRendererAccessGate::Handoff& Handoff(){return handoff.value();}
 bool NeedsRetirementNotice()const noexcept{return bool(previousRetired);}
 void MarkRetirementReady()noexcept{receiptReady.store(true,std::memory_order_release);}
 void MarkBegun(uint64_t submission)noexcept{target.MarkBegun(submission);}
 // Publish separately AFTER target notice and active-source setup. The original
 // earlier Begin receipt remains the failure classifier, not this release gate.
 void PublishBeginReady()noexcept{progress.fetch_or(1,std::memory_order_release);progress.notify_all();}
 void MarkWorkerFinished()noexcept{progress.fetch_or(2,std::memory_order_release);progress.notify_all();}
 // Finished/ready bits never authorize releasing the handoff without Join.
 unsigned Progress()const noexcept{return progress.load(std::memory_order_acquire);}
 unsigned WaitForBeginOrFinished()const noexcept{
  unsigned observed=Progress();while(!observed){progress.wait(0,std::memory_order_acquire);observed=Progress();}return observed;
 }
 // Only after worker join, outside a runtime/cache ownership scope.
 void ReleaseHandoff()noexcept{handoff.reset();}
 template<class Apply> void DeliverTarget(Apply&& apply)noexcept{target.Deliver(std::forward<Apply>(apply));}
 void DeliverRetirement(){if(receiptReady.exchange(false,std::memory_order_acq_rel)){previousRetired();}}
};
}
