/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureBacking.h"
#include <array>
#include <memory>
#include <type_traits>

namespace rendering {
// CPU type evidence only. No image, sampler, generation, completion or GPU
// readiness is memoized. Entries never prolong the backing's lifetime.
template<class Derived, std::size_t Capacity=128> class NativePositiveBackingFacts {
 static_assert(std::is_final_v<Derived> && std::is_convertible_v<const Derived*,const NativeTextureBacking*>);
 static_assert(Capacity && (Capacity & (Capacity-1))==0);
public:
 struct Scope {
  const void* lease=nullptr; const void* context=nullptr; std::uint64_t submission=0;
  bool operator==(const Scope&) const = default;
 };
 struct Stats {std::uint64_t probes=0,full=0;};
 class Proof {
  friend class NativePositiveBackingFacts;
  std::shared_ptr<const NativeTextureBacking> backing;
  std::shared_ptr<const Derived> typed;
  explicit Proof(std::shared_ptr<const NativeTextureBacking> source)
   :backing(std::move(source)),typed(std::dynamic_pointer_cast<const Derived>(backing)) {}
 public:
  const std::shared_ptr<const Derived>& Typed()const noexcept{return typed;}
 };
 explicit NativePositiveBackingFacts(Scope scope,bool count=false):scope(scope),count(count){}
 NativePositiveBackingFacts(const NativePositiveBackingFacts&)=delete;
 NativePositiveBackingFacts& operator=(const NativePositiveBackingFacts&)=delete;
 static Proof Classify(const std::shared_ptr<const NativeTextureBacking>& backing){return Proof(backing);}
 bool Matches(Scope actual)const noexcept{return scope.lease && scope.context && scope.submission && scope==actual;}
 std::shared_ptr<const Derived> Find(Scope actual,const std::shared_ptr<const NativeTextureBacking>& backing)noexcept {
  if(!Matches(actual)||!backing)return {};
  const auto first=Bucket(backing.get());
  for(std::size_t n=0;n<Capacity;++n){
   const auto& entry=entries[(first+n)&(Capacity-1)];if(count)++stats.probes;
   if(!entry.address)return {};
   if(entry.address!=backing.get() || entry.typed.owner_before(backing) || backing.owner_before(entry.typed))continue;
   auto typed=entry.typed.lock();
   if(typed && static_cast<const NativeTextureBacking*>(typed.get())==backing.get())return typed;
  }
  return {};
 }
 // The caller validates its own payload before committing this unforgeable
 // positive proof. Full tables and invalid scopes simply retain the old path.
 bool Remember(Scope actual,const Proof& proof)noexcept {
  if(!Matches(actual)||!proof.typed)return false;
  const auto first=Bucket(proof.backing.get());
  for(std::size_t n=0;n<Capacity;++n){
   auto& entry=entries[(first+n)&(Capacity-1)];
   if(entry.address==proof.backing.get() && !entry.typed.owner_before(proof.backing) && !proof.backing.owner_before(entry.typed))return true;
   if(!entry.address){entry.address=proof.backing.get();entry.typed=proof.typed;++used;return true;}
  }
  if(count)++stats.full;return false;
 }
 std::size_t Size()const noexcept{return used;}
 const Stats& Counts()const noexcept{return stats;}
private:
 struct Entry {const NativeTextureBacking* address=nullptr;std::weak_ptr<const Derived> typed;};
 static std::size_t Bucket(const void* pointer)noexcept {
  auto value=reinterpret_cast<std::uintptr_t>(pointer);value^=value>>17;value^=value>>9;
  return (value>>3)&(Capacity-1);
 }
 Scope scope;bool count=false;Stats stats{};std::size_t used=0;
 std::array<Entry,Capacity> entries{};
};
}
