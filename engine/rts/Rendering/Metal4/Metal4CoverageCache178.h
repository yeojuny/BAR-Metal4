/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeMiniMapData.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

namespace rendering::metal4 {
// These are immutable raw R16Uint inputs, never writable compute outputs.
// A strong snapshot key prevents address reuse and preserves older publications.
// Eviction releases cache references only: no texture is recycled or overwritten.
class CoverageCache178 {
public:
 static constexpr size_t EntryLimit=16;
 static constexpr size_t ByteLimit=16u*1024u*1024u;
 ~CoverageCache178();
 void Clear(){for(auto& e:entries)e={};bytes=0;count=0;next=0;device=nil;}

 template<class Lease,class Factory>
 id<MTLTexture> Get(Lease& lease,const std::shared_ptr<const NativeMiniMapCoverage>& source,Factory&& create)
 {
  Require(bool(source),"Raw coverage cache requires an immutable source");
  ++requests;
  if(device!=lease.Device()){Clear();device=lease.Device();}
  for(const auto& e:entries)if(e.source==source){
   ++hits;avoidedUploadBytes+=uint64_t(source->counts.size())*sizeof(uint16_t);
   // Every submission must register even a cache hit. A prepared WorldInfo
   // update also retains it in infoAllocations for subsequent registrations.
   lease.AddAllocation(e.image);return e.image;
  }
  // The original factory validates extent, allocates, uploads and registers.
  // Exceptions publish no cache entry and preserve the prior entries.
  auto image=create();Require(image!=nil,"Raw coverage factory returned no texture");++created;
  const auto gpuBytes=std::max(size_t(image.allocatedSize),source->counts.size()*sizeof(uint16_t));
  const auto capacity=source->counts.capacity();
  if(capacity>(ByteLimit-sizeof(NativeMiniMapCoverage))/sizeof(uint16_t)){
   ++uncached;return image;
  }
  const auto cpuBytes=sizeof(NativeMiniMapCoverage)+capacity*sizeof(uint16_t);
  if(gpuBytes>ByteLimit-cpuBytes){++uncached;return image;}
  const auto charge=cpuBytes+gpuBytes;
  while(count==EntryLimit||charge>ByteLimit-bytes){
   auto& victim=entries[next];
   if(victim.source){bytes-=victim.charge;--count;++evictions;victim={};}
   next=(next+1)%EntryLimit;
  }
  while(entries[next].source)next=(next+1)%EntryLimit;
  entries[next]={source,image,charge};next=(next+1)%EntryLimit;
  bytes+=charge;++count;
  peakBytes=std::max(peakBytes,bytes);peakCount=std::max(peakCount,count);
  return image;
 }
 size_t CachedBytes()const{return bytes;}
 size_t CachedCount()const{return count;}
 uint64_t requests=0,hits=0,created=0,uncached=0,evictions=0,avoidedUploadBytes=0;
 size_t peakBytes=0,peakCount=0;
private:
 struct Entry {
  std::shared_ptr<const NativeMiniMapCoverage> source;
  __strong id<MTLTexture> image=nil;
  size_t charge=0;
 };
 std::array<Entry,EntryLimit> entries;
 __strong id<MTLDevice> device=nil;
 size_t bytes=0,count=0,next=0;
};
} // namespace rendering::metal4
