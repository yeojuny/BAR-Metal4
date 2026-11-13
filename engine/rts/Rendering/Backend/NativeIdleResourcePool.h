/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
namespace rendering {
// Serial-only bounded cache, not a bound on outstanding owners. External pins
// exclude a slot from reuse/eviction. Busy pools return an uncached owned result.
// Match/Create/Prepare must not re-enter or mutate this pool.
template<class Resource,std::size_t Capacity> class NativeIdleResourcePool {
 std::array<std::shared_ptr<Resource>,Capacity> slots{};
public:
 template<class Match,class Create,class Prepare>
 std::shared_ptr<Resource> Acquire(Match&& match,Create&& create,Prepare&& prepare) {
  std::size_t victim=Capacity;
  for(std::size_t i=0;i<Capacity;++i){
   // Test the stored owner before making any temporary shared_ptr copies.
   if(slots[i]&&slots[i].use_count()!=1)continue;
   if(slots[i]&&match(*slots[i])){
    auto selected=slots[i];prepare(selected);return selected;
   }
   if(victim==Capacity)victim=i;
  }
  auto selected=std::forward<Create>(create)();
  if(!selected)throw std::logic_error("Idle resource factory returned no owner");
  // Retention/frame descriptor construction can throw. Preserve all cache
  // entries until it finishes, including a mismatched idle victim.
  std::forward<Prepare>(prepare)(selected);
  if(victim!=Capacity)slots[victim]=selected;
  return selected;
 }
 void Clear() noexcept {for(auto& slot:slots)slot.reset();}
};
}
