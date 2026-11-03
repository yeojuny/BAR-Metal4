/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeRetiredResourcePool.h"
#include <array>
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
  RetiredArgumentOwners& owners,NSError** error){
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
   for(NSUInteger i=0;i<key[0];++i)[slot.table setAddress:0 atIndex:i];
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
}
