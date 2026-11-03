/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SharedUploadPool.h"
#include <cstring>
#include <cstdint>

// Retaining a Metal buffer alone does not keep its reuse token busy. Both the
// prepared command and every runtime submission that can read it own this holder.
@interface M4TransientUploadLifetime177 : NSObject {
@public
 std::vector<std::shared_ptr<rendering::metal4::ScreenUploadStorage>> slots;
}
@end

namespace rendering::metal4 {
struct TransientUploadPool177 {
 explicit TransientUploadPool177(const char* label):family(label){}
 ~TransientUploadPool177();
 TransientUploadPool177(const TransientUploadPool177&)=delete;
 TransientUploadPool177& operator=(const TransientUploadPool177&)=delete;
 void BeginBatch(id<MTLDevice> device){storage.BeginBatch(device);}
 void Clear(){storage=ScreenUploadPool{};}
 ScreenUploadPool storage;
 const char* family;
 // Constant-cost accounting is included in normal performance measurements.
 // One summary at owner teardown; no timers or per-frame logging.
 uint64_t requests=0,reused=0,created=0,fallbacks=0,uploadedBytes=0;
 size_t peakCachedBytes=0,peakCachedCount=0;
};

template<class Lease,class Fallback>
id<MTLBuffer> UploadTransient177(Lease& lease,TransientUploadPool177& pool,
 M4TransientUploadLifetime177* __strong& lifetime,const void* data,size_t length,
 Fallback&& fallback)
{
 ++pool.requests;
 // Preserve each caller's original validation and exact allocation on failure.
 if(!data||!length||length>lease.Device().maxBufferLength){++pool.fallbacks;return fallback();}
 const auto priorCount=pool.storage.CachedCount();
 auto slot=pool.storage.Acquire(lease.Device(),length);
 if(!slot){++pool.fallbacks;return fallback();}
 if(!lifetime){
  lifetime=[M4TransientUploadLifetime177 new];
  Require(lifetime!=nil,"Cannot retain transient upload ownership");
  lease.Retain(lifetime);
 }
 auto buffer=slot->buffer;
 lifetime->slots.push_back(std::move(slot));
 // Acquire admits only pool-exclusive slots. Publish no address before the
 // prepared owner and runtime completion/poison graph own the same token.
 std::memcpy(buffer.contents,data,length);
 lease.AddAllocation(buffer);
 if(pool.storage.CachedCount()==priorCount)++pool.reused;
 else {
  ++pool.created;
  pool.peakCachedBytes=std::max(pool.peakCachedBytes,pool.storage.CachedBytes());
  pool.peakCachedCount=std::max(pool.peakCachedCount,pool.storage.CachedCount());
 }
 pool.uploadedBytes+=length;
 return buffer;
}
} // namespace rendering::metal4
