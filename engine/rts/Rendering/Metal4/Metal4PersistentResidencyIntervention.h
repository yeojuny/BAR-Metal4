/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdlib>
#include <string_view>
#include <unordered_set>
#include "Rendering/Backend/NativePersistentResidencyBudget.h"

// A committed set is immutable for its whole GPU-visible lifetime. New static
// allocations create a new small set, never mutate a set used by pending work.
// Every frame retains its snapshot through the existing completion/poison graph.
@interface M4PersistentResidencySnapshot : NSObject {
@public
 __strong NSArray* sets;
 __strong NSArray* allocations;
 __strong M4PersistentResidencySnapshot* previous;
}
@end

namespace rendering::metal4 {
inline bool PersistentResidencyInterventionEnabled() {
 static const bool enabled=[] {const char* p=std::getenv("BAR_NATIVE_INTERVENTION_PERSISTENT_RESIDENCY");return p&&std::string_view(p)=="1";}();
 return enabled;
}
class InterventionPersistentResidency {
public:
 explicit InterventionPersistentResidency(id<MTLDevice> value):device(value),pending([NSMutableArray new]){}
 bool Contains(id<MTLAllocation> allocation)const {
  return accepted.contains((__bridge const void*)allocation);
 }
 // Only explicitly long-lived renderer-owned buffers/textures use TryAdd.
 // Unknown footprints and limits fall back to normal per-frame residency.
 bool TryAdd(id<MTLAllocation> allocation) {
  if(Contains(allocation))return true;
  const uint64_t footprint=allocation.allocatedSize;
  if(!budget.CanAdd(footprint))return false;
  // Retain before publishing pointer identity; allocation can never be recycled
  // under this identity while the cache or any command snapshot still owns it.
  [pending addObject:allocation];
  try {accepted.insert((__bridge const void*)allocation);}
  catch(...){[pending removeLastObject];throw;}
  budget.Added(footprint);
  return true;
 }
 id SnapshotForRecording() {
  if(pending.count){
   auto descriptor=[MTLResidencySetDescriptor new];
   descriptor.initialCapacity=pending.count;
   descriptor.label=@"Intervention immutable persistent residency batch";
   NSError* error=nil;
   auto set=[device newResidencySetWithDescriptor:descriptor error:&error];
   Require(set!=nil,"Cannot create intervention persistent residency: "+Error(error));
   for(id<MTLAllocation> allocation in pending)[set addAllocation:allocation];
   [set commit];
   auto next=[M4PersistentResidencySnapshot new];
   auto sets=snapshot?[snapshot->sets mutableCopy]:[NSMutableArray new];
   [sets addObject:set];next->sets=[sets copy];
   next->allocations=[pending copy];next->previous=snapshot;
   snapshot=next;budget.Sealed();[pending removeAllObjects];
  }
  return snapshot;
 }
 void Bind(id<MTL4CommandBuffer> command)const {
  Require(pending.count==0,"Persistent residency must be sealed before command recording");
  if(!snapshot)return;
  for(id<MTLResidencySet> set in snapshot->sets)[command useResidencySet:set];
 }
 uint64_t Bytes()const noexcept{return budget.bytes;}
 size_t Count()const noexcept{return accepted.size();}
 // Destroy/reset this object ONLY after the runtime's all-pending retirement
 // gate. Immutable snapshots retained by failed frames remain independently
 // alive even if the runtime later releases its own pointer.
private:
 __strong id<MTLDevice> device=nil;
 __strong NSMutableArray* pending=nil;
 __strong M4PersistentResidencySnapshot* snapshot=nil;
 std::unordered_set<const void*> accepted;
 NativePersistentResidencyBudget budget;
};
}
