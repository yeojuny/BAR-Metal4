/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// Private to glFontRenderer.cpp and NativeUIRecorder.cpp. CPU capacity only.
#include "ScreenDrawList.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>

namespace rendering::fontRecycle169 {
struct Streams {
 std::vector<ScreenVertex> vertices;
 std::vector<ScreenTexCoord> uv;
 std::vector<uint32_t> indices;
 void Clear() noexcept {vertices.clear();uv.clear();indices.clear();}
 void Swap(Streams& other) noexcept {vertices.swap(other.vertices);uv.swap(other.uv);indices.swap(other.indices);}
 bool Vacant() const noexcept {return vertices.capacity()==0&&uv.capacity()==0&&indices.capacity()==0;}
};
// Capacity payload, not allocator footprint or resident memory.
inline bool AddCapacity(size_t count,size_t stride,size_t& total) noexcept {
 if(count>(std::numeric_limits<size_t>::max()-total)/stride)return false;
 total+=count*stride;return true;
}
inline bool CapacityBytes(const Streams& s,size_t& total) noexcept {
 total=0;return AddCapacity(s.vertices.capacity(),sizeof(ScreenVertex),total)&&
  AddCapacity(s.uv.capacity(),sizeof(ScreenTexCoord),total)&&AddCapacity(s.indices.capacity(),sizeof(uint32_t),total);
}
enum Counter : size_t {Acquires,Hits,ReusedCapacity,Returns,Stored,EmptyDrops,OversizeDrops,FullDrops,ByteDrops,
 LockAttempts,BusyDrops,LockErrors,OwnerLookups,SourceOwners,DeepCopies,ProducerBatches,GrowthV,GrowthUV,GrowthI,CounterCount};
struct Snapshot {
 std::array<uint64_t,CounterCount> counts{};
 size_t heldBytes=0,heldSlots=0,highWater=0;
 bool locked=false;
};
class Pool {
public:
 static constexpr size_t SlotLimit=8,ByteLimit=1024u*1024u,BlockLimit=256u*1024u;
 void Count(Counter key,uint64_t amount=1) noexcept {counts[key].fetch_add(amount,std::memory_order_relaxed);}
 bool Acquire(Streams& destination) noexcept {
  // Never overwrite pending data or a producer's already-owned capacity.
  if(!destination.Vacant())return false;
  Count(Acquires);TryLock lock(*this);if(!lock.owns)return false;
  for(auto& slot:slots)if(!slot.Vacant()) {
   size_t bytes=0;CapacityBytes(slot,bytes);slot.Swap(destination);
   heldBytes-=bytes;--heldSlots;Count(Hits);Count(ReusedCapacity,bytes);return true;
  }
  return false;
 }
 void Return(Streams& source) noexcept {
  Count(Returns);source.Clear();size_t bytes=0;
  if(!CapacityBytes(source,bytes)||bytes>BlockLimit){Count(OversizeDrops);return;}
  if(!bytes){Count(EmptyDrops);return;}
  TryLock lock(*this);if(!lock.owns)return;
  if(heldSlots==SlotLimit){Count(FullDrops);return;}
  if(bytes>ByteLimit-heldBytes){Count(ByteDrops);return;}
  for(auto& slot:slots)if(slot.Vacant()) {
   slot.Swap(source);heldBytes+=bytes;++heldSlots;
   highWater=std::max(highWater,heldBytes);Count(Stored);return;
  }
 }
 Snapshot Read() noexcept {
  Snapshot result;
  {TryLock lock(*this);result.locked=lock.owns;
   if(lock.owns){result.heldBytes=heldBytes;result.heldSlots=heldSlots;result.highWater=highWater;}}
  for(size_t n=0;n<CounterCount;++n)result.counts[n]=counts[n].load(std::memory_order_relaxed);
  return result;
 }
private:
 struct TryLock {
  Pool& pool;bool owns=false;
  explicit TryLock(Pool& p) noexcept:pool(p) {
   pool.Count(LockAttempts);
   try {owns=pool.mutex.try_lock();if(!owns)pool.Count(BusyDrops);}
   catch(...){pool.Count(LockErrors);}
  }
  ~TryLock(){if(owns)pool.mutex.unlock();}
 };
 std::mutex mutex;
 std::array<Streams,SlotLimit> slots;
 std::array<std::atomic<uint64_t>,CounterCount> counts{};
 size_t heldBytes=0,heldSlots=0,highWater=0;
};
template<class Factory> std::shared_ptr<Pool> TryCreate(Factory&& factory) noexcept {
 try{return factory();}catch(...){return {};}
}
// This trivial sentinel survives the owning anchor's destruction. Late source
// destruction uses its own shared_ptr, and late new requests get a cache miss.
inline std::atomic<bool> serviceClosed{false};
struct ServiceAnchor {
 std::shared_ptr<Pool> pool=TryCreate([]{return std::make_shared<Pool>();});
 ~ServiceAnchor(){serviceClosed.store(true,std::memory_order_release);}
};
inline std::shared_ptr<Pool> Owner() noexcept {
 if(serviceClosed.load(std::memory_order_acquire))return {};
 // Engine recording stops before service teardown, as with GetRecorder().
 // No destructor consults this anchor; there is no TLS-owned storage.
 static const ServiceAnchor anchor;
 if(anchor.pool)anchor.pool->Count(OwnerLookups);
 return anchor.pool;
}
inline void Acquire(Streams& streams) noexcept {
 if(const auto& owner=Owner())owner->Acquire(streams);
}
inline void ProducerGrowth(const std::array<uint64_t,3>& growth) noexcept {
 if(const auto& owner=Owner()) {
  owner->Count(ProducerBatches);owner->Count(GrowthV,growth[0]);
  owner->Count(GrowthUV,growth[1]);owner->Count(GrowthI,growth[2]);
 }
}
struct Source {
 std::shared_ptr<Pool> owner;
 Streams streams;
 Source(std::shared_ptr<Pool> pool,std::vector<ScreenVertex>&& v,std::vector<ScreenTexCoord>&& uv,std::vector<uint32_t>&& i) noexcept:
  owner(std::move(pool)),streams{std::move(v),std::move(uv),std::move(i)} {if(owner)owner->Count(SourceOwners);}
 Source(std::vector<ScreenVertex>&& v,std::vector<ScreenTexCoord>&& uv,std::vector<uint32_t>&& i) noexcept:
  Source(Owner(),std::move(v),std::move(uv),std::move(i)) {}
 // std::function's retained command still owns an independent deep copy.
 Source(const Source& other):owner(other.owner),streams(other.streams){if(owner)owner->Count(DeepCopies);}
 Source(Source&& other) noexcept=default;
 Source& operator=(const Source&)=delete;
 Source& operator=(Source&&)=delete;
 ~Source() noexcept {if(owner)owner->Return(streams);}
};
} // namespace rendering::fontRecycle169
