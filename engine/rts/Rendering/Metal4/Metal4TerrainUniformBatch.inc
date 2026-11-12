// Fresh chunks by default; optional bounded pool requires prepared-owner tokens.
#include "Metal4SharedUploadPool.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
namespace rendering::metal4 {
class TerrainUniformBatch {
public:
 static constexpr std::size_t Alignment=256, ChunkBytes=256u*1024u;
 struct Slice { __strong id<MTLBuffer> buffer=nil; std::size_t offset=0; };
 TerrainUniformBatch(RuntimeLease& value,std::size_t count,std::size_t size,unsigned firstCounter=9,ScreenUploadPool* pool=nullptr,std::vector<std::shared_ptr<ScreenUploadStorage>>* owners=nullptr)
  :lease(value),pool(pool),owners(owners),remaining(count),length(size),counterBase(firstCounter) {
  Require(length>0&&length<=lease.Device().maxBufferLength,"Terrain uniform exceeds actual device buffer bounds");
  if(length<=std::numeric_limits<std::size_t>::max()-(Alignment-1))stride=(length+Alignment-1)&~(Alignment-1);
 }
 Slice Upload(const void* data,std::size_t size) {
  Require(data&&size==length&&remaining,"Terrain uniform batch exceeded its original payload/count");
  CountPreparationTrace(counterBase,1);CountPreparationTrace(counterBase+2,length);
  const auto limit=std::min(ChunkBytes,std::size_t(lease.Device().maxBufferLength));
  if(!stride||stride>limit) {
   // Preserve valid oversized payloads; batching never imposes a draw limit.
   auto single=[lease.Device()newBufferWithBytes:data length:length options:MTLResourceStorageModeShared];
   Require(single!=nil,"Cannot retain actual original terrain uniforms");
   lease.AddAllocation(single);CountPreparationTrace(counterBase+1,1);--remaining;return {single,0};
  }
  if(!available) {
   available=std::min(remaining,limit/stride);offset=0;
   auto token=pool&&owners?pool->Acquire(lease.Device(),available*stride):nullptr;
   if(token){chunk=token->buffer;owners->push_back(std::move(token));}
   else chunk=[lease.Device()newBufferWithLength:available*stride options:MTLResourceStorageModeShared];
   Require(chunk!=nil&&chunk.contents!=nullptr,"Cannot retain shared terrain uniform batch");
   chunk.label=@"Original terrain uniforms, distinct aligned draw slices";
   lease.AddAllocation(chunk);CountPreparationTrace(counterBase+1,1);
  }
  Require(offset<=chunk.length&&length<=chunk.length-offset,"Terrain uniform slice exceeds its retained buffer");
  std::memcpy(static_cast<std::byte*>(chunk.contents)+offset,data,length);
  Slice result{chunk,offset};offset+=stride;--available;--remaining;return result;
 }
private:
 RuntimeLease& lease;
 ScreenUploadPool* pool=nullptr;
 std::vector<std::shared_ptr<ScreenUploadStorage>>* owners=nullptr;
 std::size_t remaining=0,length=0,stride=0,available=0,offset=0;
 unsigned counterBase=9;
 __strong id<MTLBuffer> chunk=nil;
};
} // namespace rendering::metal4
