/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeRetiredResourcePool.h"
#include <stdexcept>
#include <utility>

namespace rendering {
// Retain an Owner in EVERY content-cache entry and prepared consumer. The
// consumer's existing completion/quarantine graph must keep that Owner, not
// just its native handle. No event/clock/frame-count shortcut grants a write.
// Caller serialization is the same as NativeRetiredResourcePool.
template<class Handle, std::size_t Capacity=256>
class NativeRetiredBufferStorage {
public:
 struct Storage {Handle buffer{};std::size_t allocatedBytes=0;};
 using Owner=std::shared_ptr<Storage>;
 struct Acquisition {Owner owner;bool created=false;};
 void BeginBatch() noexcept {pool.BeginBatch();}
 void Clear() noexcept {pool.Clear();allocatedBytes=0;}
 std::size_t CachedCount() const noexcept {return pool.CachedCount();}
 // Counts storage retained by THIS pool, including busy cached slots. This
 // overlaps cache/draw ownership; it excludes uncached outstanding spills.
 std::size_t CachedAllocatedBytes() const noexcept {return allocatedBytes;}
 template<class Factory>
 Acquisition Acquire(std::size_t length,Factory&& factory) {
  if(!length)throw std::invalid_argument("Retired buffer storage needs a nonzero layout extent");
  const auto before=pool.CachedCount();bool created=false;
  auto owner=pool.Acquire(length,[&]{
   auto storage=factory();created=true;return storage;
  },[](Storage&){});
  if(pool.CachedCount()!=before)allocatedBytes+=owner->allocatedBytes;
  return {std::move(owner),created};
 }
private:
 NativeRetiredResourcePool<std::size_t,Storage,Capacity> pool;
 std::size_t allocatedBytes=0;
};
}
