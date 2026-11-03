#pragma once
#include "Metal4RuntimeNative.h"
#include "Metal4PublicationProbe.h"
#include "Rendering/Backend/NativePublishedRequestCache.h"
#include "Rendering/Backend/NativePublishedBufferPool.h"
#include <cstring>
namespace rendering::metal4 {
struct PublishedBufferValue { __strong id<MTLBuffer> value=nil; };
using PublishedBufferPool=NativePublishedBufferPool<PublishedBufferValue>;
struct PublishedPoolRegistry {
 std::mutex mutex;
 std::unordered_map<const Runtime*,std::shared_ptr<PublishedBufferPool>> pools;
};
inline auto& PublishedPools(){static PublishedPoolRegistry registry;return registry;}
inline void RegisterPublishedRuntime(const Runtime* runtime) {
 auto& registry=PublishedPools();std::lock_guard<std::mutex> lock(registry.mutex);
 Require(!registry.pools.contains(runtime),"Published runtime registered twice");
 registry.pools.emplace(runtime,std::make_shared<PublishedBufferPool>());
}
inline void UnregisterPublishedRuntime(const Runtime* runtime) {
 auto& registry=PublishedPools();std::lock_guard<std::mutex> lock(registry.mutex);registry.pools.erase(runtime);
}
inline std::shared_ptr<PublishedBufferPool> FindPublishedPool(const Runtime* runtime) {
 auto& registry=PublishedPools();std::lock_guard<std::mutex> lock(registry.mutex);
 auto it=registry.pools.find(runtime);Require(it!=registry.pools.end(),"Published runtime not registered");return it->second;
}
inline void ClearPublishedPool(const Runtime* runtime) {
 // Called only under RuntimeCleanupLease's request mutex, after its pending
 // work check. Release bounded history on scene cleanup as well as shutdown.
 *FindPublishedPool(runtime)=PublishedBufferPool{};
}
struct PublishedRequestState {
 const Runtime* context=nullptr;
 __strong id<MTLDevice> device=nil;
 std::shared_ptr<PublishedBufferPool> pool;
 NativePublishedRequestCache<PublishedBufferPool::Pin> buffers;
 // Independent pins are essential: a cache-key replacement must never release
 // an earlier binding while this request still refers to its GPU address.
 std::vector<PublishedBufferPool::Pin> used;
};
struct PublishedUploadStats {size_t allocations=0,allocatedBytes=0,copiedBytes=0,uploads=0,hits=0;};
inline id<MTLBuffer> SharePublishedRequest(RuntimeLease& lease,const std::shared_ptr<const NativeCUSPublishedBuffer>& source,PublicationProbe& probe,PublishedUploadStats* stats=nullptr) {
 Require(source&&source->capacityElements&&source->stride&&source->capacityElements<=SIZE_MAX/source->stride,"Shared published buffer extent differs");
 Require((source->kind==NativeCUSPublishedKind::Transforms&&source->stride==32)||(source->kind==NativeCUSPublishedKind::ModelUniforms&&source->stride==128),"Shared published buffer kind/stride differs");
 const size_t bytes=source->capacityElements*source->stride;
 Require(bytes<=lease.Device().maxBufferLength,"Shared published buffer exceeds device extent");
 auto& state=lease.Publications();
 Require(state.context==lease.ContextIdentity()&&state.device==lease.Device(),"Shared published buffer request/context differs");
 if(const auto* hit=state.buffers.Find(source)) {
  Require((*hit)->value.value!=nil&&(*hit)->value.value.length==bytes,"Shared published buffer extent changed");
  if(stats)++stats->hits;lease.AddAllocation((*hit)->value.value);return (*hit)->value.value;
 }
 auto result=state.pool->Acquire(source,[&](size_t size){
  id<MTLBuffer> value=[lease.Device() newBufferWithLength:size options:MTLResourceStorageModeShared];
  Require(value!=nil,"Cannot allocate published object storage");probe.Allocate(size);if(stats){++stats->allocations;stats->allocatedBytes+=size;}return PublishedBufferValue{value};
 },[&](PublishedBufferValue& slot,const auto& plan){
  Require(slot.value!=nil&&slot.value.length==bytes&&slot.value.contents!=nullptr,"Published writable buffer extent/storage differs");
  auto* dst=static_cast<uint8_t*>(slot.value.contents);
  for(const auto& range:plan){
   const size_t page=range.offset/NativeCUSPublishedPage::Bytes,local=range.offset%NativeCUSPublishedPage::Bytes;
   std::memcpy(dst+range.offset,source->pages[page]->bytes.data()+local,range.size);probe.Copy(range.offset,range.size);if(stats)stats->copiedBytes+=range.size;
  }
 });
 probe.Reuse(result.dirtyReuse,result.exactReuse);
 if(stats){stats->uploads+=!result.exactReuse;stats->hits+=result.exactReuse;}
 state.used.push_back(result.pin);
 state.buffers.Insert(source,result.pin);
 // Every hit still enters requested residency and retained/failedRetention.
 // Frame/feedback owners retain write-exclusion pins through completion;
 // a poisoned frame is quarantined along with its allocation graph.
 lease.AddAllocation(result.pin->value.value);return result.pin->value.value;
}
}
