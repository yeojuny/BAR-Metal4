/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeRendererAccessGate.h"
#include "NativeTargetSubmissionNotice.h"
#include <functional>
#include <optional>
namespace rendering {
// Owned dispatch boundary. Worker callbacks retain this object, never caller
// stack references. Settlement still requires joined worker completion on main;
// this is not a concurrently polled mailbox or a renderer lifetime owner.
class NativeRenderDispatchReceipt {
 NativeTargetSubmissionNotice target;
 std::function<void()> previousRetired;
 bool receiptReady=false;
 std::optional<NativeRendererAccessGate::Handoff> handoff;
public:
 NativeRenderDispatchReceipt(const ScreenFrameView* screen,std::function<void()> prior):target(screen),previousRetired(std::move(prior)) {}
 void Reserve(NativeRendererAccessGate& gate){if(handoff)throw std::logic_error("Dispatch receipt already reserved");handoff.emplace(gate);}
 NativeRendererAccessGate::Handoff& Handoff(){return handoff.value();}
 bool NeedsRetirementNotice()const noexcept{return bool(previousRetired);}
 void MarkRetirementReady()noexcept{receiptReady=true;}
 void MarkBegun(uint64_t submission)noexcept{target.MarkBegun(submission);}
 // Only after worker join, outside a runtime/cache ownership scope.
 void ReleaseHandoff()noexcept{handoff.reset();}
 template<class Apply> void DeliverTarget(Apply&& apply)noexcept{target.Deliver(std::forward<Apply>(apply));}
 void DeliverRetirement(){if(receiptReady){receiptReady=false;previousRetired();}}
};
}
