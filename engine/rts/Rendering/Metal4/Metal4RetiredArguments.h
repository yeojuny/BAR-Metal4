/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeRetiredResourcePool.h"
#include "Rendering/Backend/NativeArgumentResetMask.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <string_view>
#include "System/Log/ILog.h"
#include "Metal4RuntimeNative.h"
namespace rendering::metal4 {
struct RetiredArgumentSlot {__strong id<MTL4ArgumentTable> table=nil;};
using RetiredArgumentOwners=std::vector<std::shared_ptr<RetiredArgumentSlot>>;
class RetiredArguments {
public:
 using Key=std::array<NSUInteger,3>;
 void Clear(){pool.Clear();device=nil;context=nullptr;}
 template<class Lease> void BeginBatch(Lease& lease){
  if(device!=lease.Device()||context!=lease.ContextIdentity()){Clear();device=lease.Device();context=lease.ContextIdentity();}
  pool.BeginBatch();
 }
 template<class Lease> id<MTL4ArgumentTable> Acquire(Lease& lease,MTL4ArgumentTableDescriptor* descriptor,
  RetiredArgumentOwners& owners,NSError** error,uint64_t overwrittenBuffers=0){
  Require(device==lease.Device()&&context==lease.ContextIdentity(),"Argument pool requires its actual context batch");
  Require(descriptor&&descriptor.initializeBindings&&!descriptor.supportAttributeStrides,
   "Retired argument pool requires original zero-initialized non-stride bindings");
  const Key key{descriptor.maxBufferBindCount,descriptor.maxTextureBindCount,descriptor.maxSamplerStateBindCount};
  auto slot=pool.Acquire(key,[&]{
   auto table=[device newArgumentTableWithDescriptor:descriptor error:error];
   Require(table!=nil,"Cannot allocate retained original argument table: "+Error(error?*error:nil));
   return RetiredArgumentSlot{table};
  },[&](RetiredArgumentSlot& slot){
   // The pool alone owns this token. Restore initializeBindings=YES before
   // rebinding; every prepared/completion owner keeps its own token alive.
   // Only explicitly guaranteed writes may skip the reset. Callers must
   // perform these writes before publishing any draw; optional bindings stay zeroed.
   for(NSUInteger i=0;i<key[0];++i)if(NeedsArgumentBufferReset(i,overwrittenBuffers))[slot.table setAddress:0 atIndex:i];
   for(NSUInteger i=0;i<key[1];++i)[slot.table setTexture:MTLResourceID{0} atIndex:i];
   for(NSUInteger i=0;i<key[2];++i)[slot.table setSamplerState:MTLResourceID{0} atIndex:i];
  });
  auto table=slot->table;owners.push_back(std::move(slot));lease.Retain(table);return table;
 }
 size_t Count()const{return pool.CachedCount();}
private:
 const void* context=nullptr;__strong id<MTLDevice> device=nil;
 rendering::NativeRetiredResourcePool<Key,RetiredArgumentSlot,256> pool;
};

// New type; the existing RetiredArguments and its ABI stay unchanged in
// other translation units. No in-flight token is ever recycled.

namespace argPoolWitness {
struct Counts {std::atomic<unsigned long long> requested{0},allocated{0},reused{0},peak{0};};
inline Counts& Family(bool terrain){static Counts* a=new Counts;static Counts* b=new Counts;return terrain?*a:*b;}
inline bool Enabled(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_ARGPOOL_WITNESS");return p&&std::string_view(p)=="1";}();return v;}
inline void Emit(){for(bool t:{false,true}){auto& c=Family(t);std::fprintf(stderr,"[ArgPoolWitness] family=%s requested=%llu allocated=%llu reused=%llu peak_cached=%llu\n",t?"terrain":"shadow",c.requested.load(),c.allocated.load(),c.reused.load(),c.peak.load());}}
inline Counts* Get(const char* family){if(!Enabled())return nullptr;static const bool v=[](){std::atexit(Emit);return true;}();(void)v;return &Family(std::string_view(family)=="terrain");}
inline void Request(const char* family){if(auto* c=Get(family))++c->requested;}
inline void Allocate(const char* family){if(auto* c=Get(family))++c->allocated;}
inline void Reuse(const char* family){if(auto* c=Get(family))++c->reused;}
inline void Peak(const char* family,size_t slots){if(auto* c=Get(family)){auto old=c->peak.load(std::memory_order_relaxed);while(old<slots&&!c->peak.compare_exchange_weak(old,slots,std::memory_order_relaxed)){};}}
inline id<MTL4ArgumentTable> DirectNew(id<MTLDevice> device,MTL4ArgumentTableDescriptor* descriptor,const char* family){Request(family);Allocate(family);return [device newArgumentTableWithDescriptor:descriptor error:nil];}
}

class WitnessLegacyRetiredArguments {
public:
 using Key=std::array<NSUInteger,3>;
 void Clear(){pool.Clear();device=nil;context=nullptr;}
 template<class Lease> void BeginBatch(Lease& lease,const char* family){witnessFamily=family;
  if(device!=lease.Device()||context!=lease.ContextIdentity()){Clear();device=lease.Device();context=lease.ContextIdentity();}
  pool.BeginBatch();
 }
 template<class Lease> id<MTL4ArgumentTable> Acquire(Lease& lease,MTL4ArgumentTableDescriptor* descriptor,
  RetiredArgumentOwners& owners,NSError** error,uint64_t overwrittenBuffers=0){
  Require(device==lease.Device()&&context==lease.ContextIdentity(),"Argument pool requires its actual context batch");
  Require(descriptor&&descriptor.initializeBindings&&!descriptor.supportAttributeStrides,
   "Retired argument pool requires original zero-initialized non-stride bindings");
  const Key key{descriptor.maxBufferBindCount,descriptor.maxTextureBindCount,descriptor.maxSamplerStateBindCount};
  argPoolWitness::Request(witnessFamily);
  auto slot=pool.Acquire(key,[&]{
   argPoolWitness::Allocate(witnessFamily);
   auto table=[device newArgumentTableWithDescriptor:descriptor error:error];
   Require(table!=nil,"Cannot allocate retained original argument table: "+Error(error?*error:nil));
   return RetiredArgumentSlot{table};
  },[&](RetiredArgumentSlot& slot){
   argPoolWitness::Reuse(witnessFamily);
   // The pool alone owns this token. Restore initializeBindings=YES before
   // rebinding; every prepared/completion owner keeps its own token alive.
   // Only explicitly guaranteed writes may skip the reset. Callers must
   // perform these writes before publishing any draw; optional bindings stay zeroed.
   for(NSUInteger i=0;i<key[0];++i)if(NeedsArgumentBufferReset(i,overwrittenBuffers))[slot.table setAddress:0 atIndex:i];
   for(NSUInteger i=0;i<key[1];++i)[slot.table setTexture:MTLResourceID{0} atIndex:i];
   for(NSUInteger i=0;i<key[2];++i)[slot.table setSamplerState:MTLResourceID{0} atIndex:i];
  });
  if(argPoolWitness::Enabled())argPoolWitness::Peak(witnessFamily,pool.CachedCount());
  auto table=slot->table;owners.push_back(std::move(slot));lease.Retain(table);return table;
 }
 size_t Count()const{return pool.CachedCount();}
private:
 const char* witnessFamily="shadow";
 const void* context=nullptr;__strong id<MTLDevice> device=nil;
 rendering::NativeRetiredResourcePool<Key,RetiredArgumentSlot,256> pool;
};
class AdaptiveRetiredArguments {
 using Key=RetiredArguments::Key;
 struct Slot { std::shared_ptr<RetiredArgumentSlot> token;uint64_t used=0; };
 struct Bucket {std::vector<Slot> slots;size_t next=0;};
 WitnessLegacyRetiredArguments legacy;const char* witnessFamily="shadow";
 const void* context=nullptr;__strong id<MTLDevice> device=nil;
 std::map<Key,Bucket> buckets;
 size_t cached=0,limit=4096;uint64_t frame=0;
 struct Counts {uint64_t requested=0,reused=0,allocated=0,uncached=0,busy=0;};
 Counts counts{};
 static bool Diagnostics(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_TERRAIN_ARGUMENT_POOL_COUNTERS");return p&&std::string_view(p)=="1";}();return v;}
 void Trim(bool pressure) {
  for(auto it=buckets.begin();it!=buckets.end();){
   auto& slots=it->second.slots;
   slots.erase(std::remove_if(slots.begin(),slots.end(),[&](const Slot& s){
    if(s.token.use_count()!=1)return false;
    if((frame>s.used&&frame-s.used>120)||(pressure&&cached>limit)){--cached;return true;}
    return false;
   }),slots.end());
   if(slots.empty())it=buckets.erase(it);else ++it;
  }
 }
public:
 static bool Enabled(){static const bool v=[](){const char* p=std::getenv("BAR_NATIVE_TERRAIN_ARGUMENT_POOL_ADAPTIVE");return p&&std::string_view(p)=="1";}();return v;}
 void Clear(){legacy.Clear();buckets.clear();cached=0;frame=0;counts={};device=nil;context=nullptr;}
 template<class Lease> void BeginBatch(Lease& lease,uint64_t sourceFrame,const char* family){
  witnessFamily=family;
  if(!Enabled()){legacy.BeginBatch(lease,family);return;}
  Require(sourceFrame!=0,"Adaptive retired argument batch lost its source frame");
  if(device!=lease.Device()||context!=lease.ContextIdentity()||sourceFrame<frame){Clear();device=lease.Device();context=lease.ContextIdentity();}
  if(sourceFrame!=frame){
   if(Diagnostics()&&frame&&frame%64==0)LOG("[AdaptiveTerrainArguments] family=%s source_frame=%llu requested=%llu reused=%llu allocated=%llu uncached=%llu busy=%llu cached=%zu limit=%zu",family,(unsigned long long)frame,(unsigned long long)counts.requested,(unsigned long long)counts.reused,(unsigned long long)counts.allocated,(unsigned long long)counts.uncached,(unsigned long long)counts.busy,cached,limit);
   const bool samplePressure=!frame||sourceFrame/64!=frame/64;frame=sourceFrame;counts={};
   if(samplePressure){
    const auto recommended=uint64_t(device.recommendedMaxWorkingSetSize);
    const bool pressure=recommended&&uint64_t(device.currentAllocatedSize)>recommended-recommended/10;
    limit=pressure?256:4096;Trim(pressure);
   }
  }
  for(auto& [key,bucket]:buckets)bucket.next=0;
 }
 template<class Lease> id<MTL4ArgumentTable> Acquire(Lease& lease,MTL4ArgumentTableDescriptor* descriptor,RetiredArgumentOwners& owners,NSError** error,uint64_t overwrittenBuffers=0){
  if(!Enabled())return legacy.Acquire(lease,descriptor,owners,error,overwrittenBuffers);
  Require(device==lease.Device()&&context==lease.ContextIdentity()&&frame,"Adaptive arguments crossed its actual context batch");
  Require(descriptor&&descriptor.initializeBindings&&!descriptor.supportAttributeStrides,"Adaptive arguments require original zero-initialized non-stride bindings");
  const Key key{descriptor.maxBufferBindCount,descriptor.maxTextureBindCount,descriptor.maxSamplerStateBindCount};
  auto& bucket=buckets[key];std::shared_ptr<RetiredArgumentSlot> slot;
  ++counts.requested;argPoolWitness::Request(witnessFamily);
  while(bucket.next<bucket.slots.size()){
   auto& s=bucket.slots[bucket.next++];if(s.token.use_count()!=1){++counts.busy;continue;}
   s.used=frame;slot=s.token;++counts.reused;argPoolWitness::Reuse(witnessFamily);break;
  }
  if(slot){
   for(NSUInteger i=0;i<key[0];++i)if(NeedsArgumentBufferReset(i,overwrittenBuffers))[slot->table setAddress:0 atIndex:i];
   for(NSUInteger i=0;i<key[1];++i)[slot->table setTexture:MTLResourceID{0} atIndex:i];
   for(NSUInteger i=0;i<key[2];++i)[slot->table setSamplerState:MTLResourceID{0} atIndex:i];
  }else{
   argPoolWitness::Allocate(witnessFamily);
   auto table=[device newArgumentTableWithDescriptor:descriptor error:error];Require(table!=nil,"Cannot allocate retained adaptive argument table: "+Error(error?*error:nil));
   slot=std::make_shared<RetiredArgumentSlot>(RetiredArgumentSlot{table});++counts.allocated;
   if(cached<limit){bucket.slots.push_back({slot,frame});bucket.next=bucket.slots.size();++cached;}else ++counts.uncached;
  }
  if(argPoolWitness::Enabled())argPoolWitness::Peak(witnessFamily,cached);
  auto table=slot->table;owners.push_back(std::move(slot));lease.Retain(table);return table;
 }
 size_t Count()const{return Enabled()?cached:legacy.Count();}
};
}
