/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include "Rendering/Backend/NativeFrameBeginReceipt.h"
#include "Rendering/Backend/NativeFrameCompletionMailbox.h"
#include <functional>
namespace rendering {
class Metal4SceneBeginRetirement;
namespace scene {
// The scene alone constructs this capability; callers cannot supply a callback.
std::shared_ptr<const Metal4SceneBeginRetirement> MakeBeginPreparationRetirement(
 const NativeSourceFrameCredits::Lease&,const std::shared_ptr<NativeFrameBeginReceipt>&,
 const std::shared_ptr<NativeFrameCompletionMailbox>&);
}
// Exact current admission and scene epoch. Its main action is delivered by the
// existing dispatch receipt only after the worker has Joined and released access.
class Metal4SceneBeginRetirement {
 const std::thread::id source=std::this_thread::get_id();
 const NativeSourceFrameCredits::Lease credit;
 const std::shared_ptr<NativeFrameBeginReceipt> begin;
 const std::shared_ptr<NativeFrameCompletionMailbox> mailbox;
 const std::function<void()> afterJoin;
 Metal4SceneBeginRetirement(NativeSourceFrameCredits::Lease credit,
  std::shared_ptr<NativeFrameBeginReceipt> begin,std::shared_ptr<NativeFrameCompletionMailbox> mailbox,
  std::function<void()> afterJoin):credit(std::move(credit)),begin(std::move(begin)),mailbox(std::move(mailbox)),afterJoin(std::move(afterJoin)){}
 friend std::shared_ptr<const Metal4SceneBeginRetirement> scene::MakeBeginPreparationRetirement(
  const NativeSourceFrameCredits::Lease&,const std::shared_ptr<NativeFrameBeginReceipt>&,
  const std::shared_ptr<NativeFrameCompletionMailbox>&);
public:
 void Validate(const NativeSourceFrameCredits::Lease& expectedCredit,
  const std::shared_ptr<NativeFrameBeginReceipt>& expectedBegin)const {
  if(source!=std::this_thread::get_id()||!credit||!begin||!mailbox||!afterJoin||
   credit!=expectedCredit||begin!=expectedBegin)
   throw std::logic_error("BEGIN preparation retirement differs from exact scene admission");
 }
 void DeliverAfterJoin()const {
  Validate(credit,begin);afterJoin();
 }
};
}
