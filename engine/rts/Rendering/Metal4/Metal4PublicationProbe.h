#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeCUSPublishedData.h"
#include "Rendering/Backend/NativePreparationTrace.h"
#include "System/Log/ILog.h"
#include <array>
#include <chrono>
#include <cstdlib>
namespace rendering::metal4 {
struct PublicationProbeState {
 std::array<std::shared_ptr<const NativeCUSPublishedBuffer>,10> last;
 struct Seen {const void* source=nullptr;uint64_t frame=0,gpu=0;};
 std::array<Seen,10> seen{};
 unsigned reports=0;
};
inline PublicationProbeState& PublicationProbes(){static thread_local PublicationProbeState s;return s;}
inline bool PublicationProbeEnabled(){static const bool on=[] {const char* p=std::getenv("BAR_NATIVE_PUBLICATION_TRACE");return p&&p[0]=='1'&&p[1]=='\0';}();return on;}
class PublicationProbe {
public:
 PublicationProbe(RuntimeLease& l,const std::shared_ptr<const NativeCUSPublishedBuffer>& s,unsigned f):lease(l),source(s.get()),family(f) {
  if(!PublicationProbeEnabled()||!s||f>=5)return;
  slot=f*2+(s->kind==NativeCUSPublishedKind::ModelUniforms);auto& state=PublicationProbes();
  start=StartPreparationScope();
  if(start)previous=state.last[slot];
  state.last[slot]=s;
 }
 void Reuse(bool dirty,bool exact){if(start){dirtyReuse=dirty;exactReuse=exact;}}
 void Allocate(size_t n){if(start)allocated=n;}
 void Copy(size_t offset,size_t n){if(!start)return;copied+=n;
  if(previous&&previous->kind==source->kind&&previous->stride==source->stride&&previous->capacityElements==source->capacityElements&&previous->contextGeneration==source->contextGeneration&&previous->allocationSerial==source->allocationSerial){
   const size_t p=offset/NativeCUSPublishedPage::Bytes;
   if(p<previous->pages.size()&&p<source->pages.size()&&previous->pages[p]==source->pages[p])unchanged+=n;
  }
 }
 void Finish(id<MTLBuffer> b){if(!start)return;
  const auto end=std::chrono::steady_clock::now();
  const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch()).count();
  auto& state=PublicationProbes();auto& seen=state.seen[slot];
  const uint64_t gpu=b.gpuAddress;
  if(!allocated&&seen.source==source&&seen.frame==source->publishedDrawFrame&&seen.gpu==gpu)return;
  seen={source,source->publishedDrawFrame,gpu};if(state.reports++>=4096)return;
  LOG("[NativePublication] family=%u kind=%u context=%p source=%p pub=%llu draw=%llu gpu=%llu capacity=%llu alloc=%llu copied=%llu unchanged=%llu dirty_reuse=%u exact_reuse=%u cpu_us=%.3f",family,unsigned(source->kind),lease.ContextIdentity(),source,(unsigned long long)source->publicationSerial,(unsigned long long)source->publishedDrawFrame,(unsigned long long)gpu,(unsigned long long)(source->capacityElements*source->stride),(unsigned long long)allocated,(unsigned long long)copied,(unsigned long long)unchanged,unsigned(dirtyReuse),unsigned(exactReuse),double(ns-start)/1000.0);
 }
private:
 RuntimeLease& lease;const NativeCUSPublishedBuffer* source;unsigned family,slot=0;uint64_t start=0,allocated=0,copied=0,unchanged=0;
 bool dirtyReuse=false,exactReuse=false;
 std::shared_ptr<const NativeCUSPublishedBuffer> previous;
};
}
