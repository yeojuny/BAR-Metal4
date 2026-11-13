/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace rendering {
// Capture-owner-only retention. Not a queue or a synchronization primitive.
// Fixed storage makes the transfer following a successful GPU submit allocation
// free. Exact IDs permit completion notices to arrive in either order.
template<class Payload, std::size_t Capacity, bool AllowZeroIdentity=false> class NativeCompletedFrameLedger {
 static_assert(Capacity>0);
 struct Slot {std::uint64_t id=0;std::optional<Payload> payload;};
 std::array<Slot,Capacity> slots;
public:
 bool CanHold(std::uint64_t id) const noexcept {
  if(!id && !AllowZeroIdentity)return false;
  bool available=false;
  for(const auto& slot:slots){if(slot.payload && slot.id==id)return false;available|=!slot.payload;}
  return available;
 }
 template<class... Args> bool TryHold(std::uint64_t id,Args&&... args) noexcept {
  static_assert(std::is_nothrow_constructible_v<Payload,Args&&...>);
  if(!CanHold(id))return false; // Rejection does not consume caller resources.
  for(auto& slot:slots)if(!slot.payload){
   slot.payload.emplace(std::forward<Args>(args)...);slot.id=id;return true;
  }
  return false;
 }
 bool Contains(std::uint64_t id) const noexcept {
  for(const auto& slot:slots)if(slot.payload && slot.id==id)return true;
  return false;
 }
 bool Retire(std::uint64_t id) noexcept {
  if(!id && !AllowZeroIdentity)return false;
  for(auto& slot:slots)if(slot.payload && slot.id==id){slot.payload.reset();slot.id=0;return true;}
  return false;
 }
 template<class Predicate> bool AnyOf(Predicate matches) const noexcept {
  static_assert(std::is_nothrow_invocable_r_v<bool,Predicate,const Payload&>);
  for(const auto& slot:slots)if(slot.payload && matches(*slot.payload))return true;
  return false;
 }
 std::size_t Size() const noexcept {
  std::size_t count=0;for(const auto& slot:slots)count+=slot.payload.has_value();return count;
 }
};
}
