/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <memory>
#include <thread>
#include <cstdint>
#include <stdexcept>
namespace rendering {
// Two SOURCE frames, not two CPU tasks. Capture leases expire naturally;
// transferred leases stay held through actual main-side GPU receipt retirement.
class NativeSourceFrameCredits {
 const std::thread::id source=std::this_thread::get_id();
 const std::shared_ptr<const int> identity=std::make_shared<const int>(0);
public:
 class Token {
  friend class NativeSourceFrameCredits;
  const std::shared_ptr<const int> owner;
  const uint64_t draw;
  Token(std::shared_ptr<const int> owner,uint64_t draw):owner(std::move(owner)),draw(draw){}
 public:
  Token(const Token&)=delete;Token& operator=(const Token&)=delete;
  uint64_t Draw()const noexcept{return draw;}
 };
 using Lease=std::shared_ptr<const Token>;
private:
 struct Slot{std::weak_ptr<const Token> lease;Lease held;};
 std::array<Slot,2> slots;
 void Source()const{if(source!=std::this_thread::get_id())throw std::logic_error("Source credit requires capture thread");}
 Slot& Find(const Lease& token){
  Source();if(!token||token->owner!=identity)throw std::logic_error("Foreign source credit");
  for(auto& slot:slots)if(slot.lease.lock()==token)return slot;
  throw std::logic_error("Source credit is not reserved");
 }
public:
 NativeSourceFrameCredits()=default;
 NativeSourceFrameCredits(const NativeSourceFrameCredits&)=delete;
 NativeSourceFrameCredits& operator=(const NativeSourceFrameCredits&)=delete;
 NativeSourceFrameCredits(NativeSourceFrameCredits&&)=delete;
 NativeSourceFrameCredits& operator=(NativeSourceFrameCredits&&)=delete;
 bool HasCapacity()const{Source();for(const auto& slot:slots)if(slot.lease.expired())return true;return false;}
 Lease Acquire(uint64_t draw){
  Source();for(const auto& slot:slots)if(auto live=slot.lease.lock();live&&live->draw==draw)throw std::logic_error("Repeated source capture credit");
  for(auto& slot:slots)if(slot.lease.expired()){auto token=Lease(new Token(identity,draw));slot.lease=token;return token;}
  throw std::logic_error("Two source frames already retained");
 }
 void Validate(const Lease& token,uint64_t draw){(void)Find(token);if(token->draw!=draw)throw std::logic_error("Source credit draw mismatch");}
 void Hold(const Lease& token){auto& slot=Find(token);if(slot.held)throw std::logic_error("Source credit transferred twice");slot.held=token;}
 void ValidateHeld(uint64_t draw)const{
  Source();for(const auto& slot:slots)if(slot.held&&slot.held->draw==draw)return;
  throw std::logic_error("GPU receipt has no held source credit");
 }
 void ReleaseHeld(uint64_t draw)noexcept{
  if(source!=std::this_thread::get_id())std::terminate();
  for(auto& slot:slots)if(slot.held&&slot.held->draw==draw){slot.held.reset();return;}
  std::terminate();
 }
};
}
