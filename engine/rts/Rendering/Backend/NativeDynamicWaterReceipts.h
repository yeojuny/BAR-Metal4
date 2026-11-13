/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDynamicWaterData.h"
#include <array>
#include <exception>
#include <stdexcept>
#include <utility>
namespace rendering {
// Main-owner receipt ledger only. Does not permit capture/encoding overlap or
// relax the dynamic recurrence's existing retirement and Update barriers.
class NativeDynamicWaterReceipts {
 using Frame=NativeDynamicWaterFrame;
 enum class State { Empty, Captured, Finalized, Held, Quarantined };
 struct Slot { std::shared_ptr<const Frame> source,finalized;State state=State::Empty; };
 std::array<Slot,3> slots{};
 const Slot* Find(uint64_t draw)const noexcept {
  for(const auto& s:slots)if(s.source&&s.source->sourceDrawFrame==draw)return &s;
  return nullptr;
 }
 Slot* Find(uint64_t draw)noexcept{return const_cast<Slot*>(std::as_const(*this).Find(draw));}
 static void Require(bool valid,const char* why){if(!valid)throw std::logic_error(why);}
 static bool Tuple(const Frame& a,const Frame& b)noexcept {
  return a.material&&b.material&&a.material==b.material&&a.sourceDrawFrame==b.sourceDrawFrame&&a.throughUpdate==b.throughUpdate;
 }
public:
 static constexpr size_t Capacity=3;
 void ValidateReset()const {for(const auto& s:slots)Require(s.state!=State::Held&&s.state!=State::Quarantined,"Dynamic reset would discard held/quarantined receipt");}
 bool Any()const noexcept {for(const auto& s:slots)if(s.source)return true;return false;}
 std::shared_ptr<const Frame> Current(uint64_t draw)const noexcept {const auto* s=Find(draw);return s?s->source:nullptr;}
 std::shared_ptr<const Frame> Finalized(uint64_t draw)const noexcept {const auto* s=Find(draw);return s?s->finalized:nullptr;}
 void ValidateReserve(const std::shared_ptr<const Frame>& source)const {
  Require(source&&source->material&&source->material->generation,"Dynamic receipt lacks exact material/draw owner");
  bool capacity=false;
  for(const auto& s:slots){
   if(!s.source){capacity=true;continue;}
   Require(s.state!=State::Quarantined,"Dynamic receipt ledger is quarantined");
   Require(s.source->material==source->material&&s.source->sourceDrawFrame<source->sourceDrawFrame&&s.source->throughUpdate<=source->throughUpdate,"Dynamic reservation crosses generation/order or repeats a draw");
  }
  Require(capacity,"Dynamic receipt capacity exhausted");
 }
 void Reserve(std::shared_ptr<const Frame> source)noexcept {
  // Caller performs ValidateReserve in the collective throwing preflight.
  if(!source||Find(source->sourceDrawFrame))std::terminate();
  for(auto& s:slots)if(!s.source){s.source=std::move(source);s.state=State::Captured;return;}
  std::terminate();
 }
 void SetFinalized(const std::shared_ptr<const Frame>& source,std::shared_ptr<const Frame> out){
  const auto* found=source?Find(source->sourceDrawFrame):nullptr;
  Require(found&&found->state==State::Captured&&found->source==source&&out&&Tuple(*source,*out),"Dynamic finalized receipt differs from exact captured owner");
  auto* s=Find(source->sourceDrawFrame);s->finalized=std::move(out);s->state=State::Finalized;
 }
 void ValidateHold(uint64_t draw)const {const auto* s=Find(draw);Require(s&&s->state==State::Finalized&&s->finalized,"Dynamic admission lacks finalized owner");}
 void Hold(uint64_t draw)noexcept {auto* s=Find(draw);if(!s||s->state!=State::Finalized||!s->finalized)std::terminate();s->state=State::Held;}
 void ValidateHeld(uint64_t draw)const {const auto* s=Find(draw);Require(s&&s->state==State::Held&&s->finalized,"Dynamic completion lacks held owner");}
 void ValidateRestore(uint64_t draw)const {ValidateHeld(draw);}
 void Restore(uint64_t draw)noexcept {auto* s=Find(draw);if(!s||s->state!=State::Held)std::terminate();s->state=State::Finalized;}
 void Quarantine(uint64_t draw)noexcept {auto* s=Find(draw);if(!s||(s->state!=State::Held&&s->state!=State::Quarantined))std::terminate();s->state=State::Quarantined;}
 void ValidateCompletion(uint64_t generation,uint64_t through,uint64_t draw,uint64_t retired,uint64_t sequence)const {
  const auto* s=Find(draw);
  Require(s&&s->finalized&&(s->state==State::Finalized||s->state==State::Held),"Dynamic retirement lacks a completable exact receipt");
  Require(s->source->material->generation==generation&&s->source->throughUpdate==through&&through>=retired&&through<=sequence,"Dynamic retirement generation/update tuple differs");
  for(const auto& other:slots)Require(!other.source||other.source->sourceDrawFrame>=draw,"Dynamic retirement skipped an older receipt");
 }
 void Complete(uint64_t draw)noexcept {
  auto* s=Find(draw);if(!s||(s->state!=State::Finalized&&s->state!=State::Held))std::terminate();
  *s=Slot{};
 }
};
}
