/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <vector>
#include <memory>
#include <algorithm>
namespace rendering::metal4 {
struct ScreenUploadStorage {
 __strong id<MTLBuffer> buffer=nil;
};
class ScreenUploadPool {
public:
 static constexpr size_t ByteLimit=16u*1024u*1024u;
 static constexpr size_t EntryLimit=64;
 static constexpr size_t LargestBuffer=2u*1024u*1024u;
 void BeginBatch(id<MTLDevice> device) {
  if(owner!=device) {
   for(auto& bucket:buckets)bucket.slots.clear();
   bytes=0;entries=0;owner=device;
  }
  for(auto& bucket:buckets)bucket.next=0;
 }
 std::shared_ptr<ScreenUploadStorage> Acquire(id<MTLDevice> device,size_t length) {
  Require(owner==device,"Screen upload pool has no matching device batch");
  if(!length || length>LargestBuffer)return {};
  size_t capacity=256,index=0;
  while(capacity<length){capacity*=2;++index;}
  if(capacity>device.maxBufferLength)return {};
  auto& bucket=buckets[index];
  while(bucket.next<bucket.slots.size()) {
   auto& slot=bucket.slots[bucket.next++];
   if(slot.use_count()==1)return slot;
  }
  // Cache limits never restrict a draw: the caller falls back to its original
  // exact-size fresh allocation. Count actual resource allocation bytes, not
  // just the requested length; driver metadata is separately bounded by count.
  if(entries==EntryLimit || capacity>ByteLimit-bytes)return {};
  auto buffer=[device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
  if(!buffer)return {};
  Require(buffer.contents!=nullptr,"Native UI shared upload has no CPU address");
  const auto footprint=std::max(size_t(buffer.length),size_t(buffer.allocatedSize));
  if(footprint>ByteLimit-bytes)return {};
  auto slot=std::make_shared<ScreenUploadStorage>(ScreenUploadStorage{buffer});
  bucket.slots.push_back(slot);bucket.next=bucket.slots.size();
  bytes+=footprint;++entries;
  return slot;
 }
 size_t CachedBytes() const {return bytes;}
 size_t CachedCount() const {return entries;}
private:
 struct Bucket {std::vector<std::shared_ptr<ScreenUploadStorage>> slots;size_t next=0;};
 // Exactly fourteen size classes (256 B .. 2 MiB); arbitrary input lengths
 // cannot grow an unbounded lookup map.
 std::array<Bucket,14> buckets;
 __strong id<MTLDevice> owner=nil;
 size_t bytes=0,entries=0;
};
}
