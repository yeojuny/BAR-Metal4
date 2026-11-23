/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <span>
#include <memory>
#include <thread>
#include <cstdint>
#include <stdexcept>
#include <limits>
namespace rendering {
// Two source credits by default. Overlapped capture may reserve one more than the GPU limit.
// This is not a CPU task or GPU submission limit. Capture leases expire naturally;
// transferred leases stay held through actual main-side GPU receipt retirement.
class NativeSourceFrameCredits {
 const std::thread::id source=std::this_thread::get_id();
 const std::shared_ptr<const int> identity=std::make_shared<const int>(0);
public:
 class Token {
  friend class NativeSourceFrameCredits;
  const std::shared_ptr<const int> owner;
  const uint64_t draw,ordinal;
  Token(std::shared_ptr<const int> owner,uint64_t draw,uint64_t ordinal):owner(std::move(owner)),draw(draw),ordinal(ordinal){}
 public:
  Token(const Token&)=delete;Token& operator=(const Token&)=delete;
  uint64_t Draw()const noexcept{return draw;}
 };
 using Lease=std::shared_ptr<const Token>;
private:
 struct Slot{std::weak_ptr<const Token> lease;Lease held;};
 const unsigned capacity;
 std::array<Slot,4> slots;
 auto ActiveSlots(){return std::span<Slot>(slots).first(capacity);}
 auto ActiveSlots()const{return std::span<const Slot>(slots).first(capacity);}
 // Capture order, not simulation/draw numbers (which may repeat or decrease).
 // A begun failure quarantines the dependency suffix. Never restore old
 // journals over a younger capture or recycle its credit as successful work.
 uint64_t nextOrdinal=1,failedOrdinal=0;
 void ValidateLineage(const Token& token)const {
  if(failedOrdinal && token.ordinal>=failedOrdinal)
   throw std::logic_error("Source capture depends on a quarantined predecessor; restart required");
 }
 void RequireOpenLineage()const {
  if(failedOrdinal)throw std::logic_error("Source capture lineage is quarantined; restart required");
 }
 void Source()const{if(source!=std::this_thread::get_id())throw std::logic_error("Source credit requires capture thread");}
 Slot& Find(const Lease& token){
  Source();if(!token||token->owner!=identity)throw std::logic_error("Foreign source credit");
  for(auto& slot:ActiveSlots())if(slot.lease.lock()==token)return slot;
  throw std::logic_error("Source credit is not reserved");
 }
public:
 explicit NativeSourceFrameCredits(unsigned requestedCapacity=2):capacity(requestedCapacity){
  if(capacity<2 || capacity>slots.size())throw std::invalid_argument("Source frame capacity must be two through four");
 }
 unsigned Capacity()const noexcept {return capacity;}
 NativeSourceFrameCredits(const NativeSourceFrameCredits&)=delete;
 NativeSourceFrameCredits& operator=(const NativeSourceFrameCredits&)=delete;
 NativeSourceFrameCredits(NativeSourceFrameCredits&&)=delete;
 NativeSourceFrameCredits& operator=(NativeSourceFrameCredits&&)=delete;
 bool HasCapacity()const{Source();RequireOpenLineage();for(const auto& slot:ActiveSlots())if(slot.lease.expired())return true;return false;}
 // Only CPU settlement may reclaim first. Recheck weak lease ownership:
 // an unexpected alias or missing retirement still takes the original drain.
 template<class SettleCPU,class DrainGPU>
 Lease AcquireWithCPUReclaim(uint64_t draw,bool enabled,SettleCPU settle,DrainGPU drain){
  if(enabled&&!HasCapacity())settle();
  if(!HasCapacity())drain();
  return Acquire(draw);
 }
 Lease Acquire(uint64_t draw){
  Source();RequireOpenLineage();
  if(nextOrdinal==std::numeric_limits<uint64_t>::max())throw std::logic_error("Source capture ordinal exhausted");
  for(const auto& slot:ActiveSlots())if(auto live=slot.lease.lock();live&&live->draw==draw)throw std::logic_error("Repeated source capture credit");
  for(auto& slot:ActiveSlots())if(slot.lease.expired()){auto token=Lease(new Token(identity,draw,nextOrdinal++));slot.lease=token;return token;}
  throw std::logic_error("Source frame retention capacity exhausted");
 }
 void Validate(const Lease& token,uint64_t draw){(void)Find(token);ValidateLineage(*token);if(token->draw!=draw)throw std::logic_error("Source credit draw mismatch");}
 void Hold(const Lease& token){auto& slot=Find(token);ValidateLineage(*token);if(slot.held)throw std::logic_error("Source credit transferred twice");slot.held=token;}
 void ValidateHeld(uint64_t draw)const{
  Source();for(const auto& slot:ActiveSlots())if(slot.held&&slot.held->draw==draw){ValidateLineage(*slot.held);return;}
  throw std::logic_error("GPU receipt has no held source credit");
 }
 // Called only for exact transferred/Begun failure on the capture thread.
 // Keep held owners alive, invalidate already-captured younger work and stop
 // future admission. This is quarantine, not general dependent rollback.
 void QuarantineHeld(uint64_t draw)noexcept {
  if(source!=std::this_thread::get_id())std::terminate();
  for(const auto& slot:ActiveSlots())if(slot.held&&slot.held->draw==draw){
   if(!failedOrdinal || slot.held->ordinal<failedOrdinal)failedOrdinal=slot.held->ordinal;
   return;
  }
  std::terminate();
 }
 void ReleaseHeld(uint64_t draw)noexcept{
  if(source!=std::this_thread::get_id())std::terminate();
  for(auto& slot:ActiveSlots())if(slot.held&&slot.held->draw==draw){if(failedOrdinal&&slot.held->ordinal>=failedOrdinal)std::terminate();slot.held.reset();return;}
  std::terminate();
 }
};
}
