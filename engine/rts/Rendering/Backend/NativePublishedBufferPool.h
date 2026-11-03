#pragma once
#include "NativeCUSPublishedReuse.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
struct NativePublishedCopyRange { size_t offset, size; };
inline bool CompatiblePublishedLayout(const NativeCUSPublishedBuffer& a, const NativeCUSPublishedBuffer& b) {
 return a.kind==b.kind && a.stride==b.stride && a.capacityElements==b.capacityElements &&
  a.contextGeneration==b.contextGeneration && a.allocationSerial==b.allocationSerial && a.pages.size()==b.pages.size();
}
// Plan every write before changing GPU-visible storage. Unobserved records are
// not invented; existing draw validation remains responsible for every read.
inline std::vector<NativePublishedCopyRange> PublishedCopyPlan(const NativeCUSPublishedBuffer& source,const NativeCUSPublishedBuffer* previous) {
 if(!source.capacityElements || (source.kind==NativeCUSPublishedKind::Transforms ? source.stride!=32 :
    source.kind!=NativeCUSPublishedKind::ModelUniforms || source.stride!=128) || source.capacityElements>SIZE_MAX/source.stride)
  throw std::invalid_argument("Published copy extent/kind differs");
 const size_t perPage=NativeCUSPublishedPage::Bytes/source.stride;
 const size_t pages=source.capacityElements/perPage+(source.capacityElements%perPage!=0);
 if(source.pages.size()!=pages)throw std::invalid_argument("Published page extent differs");
 if(previous && !CompatiblePublishedLayout(source,*previous))previous=nullptr;
 std::vector<NativePublishedCopyRange> plan;
 for(size_t p=0;p<pages;++p) {
  const auto& page=source.pages[p];
  if(previous) {const auto& old=previous->pages[p];
   if(page.get()==old.get() && !page.owner_before(old) && !old.owner_before(page))continue;
  }
  if(!page)continue;
  const size_t first=p*perPage, end=std::min(source.capacityElements,first+perPage);
  for(size_t row=first;row<end;) {
   if(!NativeCUSPublishedElementObserved(source,row)){++row;continue;}
   const size_t begin=row++;
   while(row<end && NativeCUSPublishedElementObserved(source,row))++row;
   plan.push_back({begin*source.stride,(row-begin)*source.stride});
  }
 }
 return plan;
}

// Caller serializes access. Each request MUST keep its returned slots pinned
// until GPU completion. Only the pool's sole-owned slots can be rewritten.
// Two retained slots per kind bound history; extra simultaneous generations
// use transient slots, never overwrite a pinned one or truncate a request.
template<class Value> class NativePublishedBufferPool {
public:
 struct Slot {std::shared_ptr<const NativeCUSPublishedBuffer> source; Value value;};
 using Pin=std::shared_ptr<Slot>;
 struct Result {Pin pin; bool allocated=false, dirtyReuse=false, exactReuse=false;};
 template<class Allocate,class Copy>
 Result Acquire(const std::shared_ptr<const NativeCUSPublishedBuffer>& source,Allocate&& allocate,Copy&& copy) {
  if(!source)throw std::invalid_argument("Published pool has no source");
  if(source->kind!=NativeCUSPublishedKind::Transforms && source->kind!=NativeCUSPublishedKind::ModelUniforms)
   throw std::invalid_argument("Published pool kind differs");
  auto& slots=retained[source->kind==NativeCUSPublishedKind::ModelUniforms];
  for(const auto& slot:slots)if(slot && slot->source && SameNativeCUSPublishedStorage(*slot->source,*source))return {slot,false,false,true};
  size_t compatible=slots.size(),free=slots.size();
  for(size_t i=0;i<slots.size();++i)if(slots[i] && slots[i].use_count()==1) {
   if(free==slots.size())free=i;
   if(slots[i]->source && CompatiblePublishedLayout(*slots[i]->source,*source)){compatible=i;break;}
  }
  const bool reuse=compatible!=slots.size();
  const auto plan=PublishedCopyPlan(*source,reuse?slots[compatible]->source.get():nullptr);
  Pin slot;
  if(reuse)slot=slots[compatible];
  else slot=std::make_shared<Slot>(Slot{{},allocate(source->capacityElements*source->stride)});
  // If a callback fails, no old generation may alias partially changed bytes.
  slot->source.reset();
  copy(slot->value,plan);
  slot->source=source;
  if(!reuse) {
   if(free!=slots.size())slots[free]=slot;
   else for(auto& entry:slots)if(!entry){entry=slot;break;}
  }
  return {std::move(slot),!reuse,reuse,false};
 }
 size_t RetainedCount() const {size_t n=0;for(const auto& kind:retained)for(const auto& s:kind)n+=bool(s);return n;}
private:
 std::array<std::array<Pin,2>,2> retained{};
};
}
