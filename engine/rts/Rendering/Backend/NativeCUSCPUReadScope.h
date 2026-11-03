/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSRenderInputs.h"
#include <map>
#include <stdexcept>

namespace rendering {
// CPU-only work for one serialized Draw invocation. Captured model inputs,
// instance bytes, program and engine uniforms are immutable shared owners.
// No device, allocator, upload, residency or submission is accessed here.
class NativeCUSCPUReadScope {
public:
 static constexpr size_t EntryLimit=4096, ByteLimit=32*1024*1024;
 NativeCUSCPUReadScope():previous(active){geometry.Prune();active=this;}
 ~NativeCUSCPUReadScope(){active=previous;}
 NativeCUSCPUReadScope(const NativeCUSCPUReadScope&)=delete;
 NativeCUSCPUReadScope& operator=(const NativeCUSCPUReadScope&)=delete;
 void Prime(const std::shared_ptr<const NativeCUSSourceFrame>& source){
  if(!source)return;
  for(const auto& draw:source->draws){
   if(entries.size()>=EntryLimit)return;
   // Match the consumer's early no-raster predicates. The consumer still
   // performs its ordinary full source/state validation at its original site.
   if((draw.cullEnabled&&draw.cullFace==0x0408)||HasNoNativeCUSAttachmentEffects(draw))continue;
   const auto key=KeyFor(draw);if(entries.contains(key))continue;
   NativeCUSRenderReadSet reads;
   try {reads=ValidateNativeCUSRenderReads(draw,geometry);}
   catch(const std::runtime_error&){continue;} // Keep original consumer error/order.
   size_t cost=0;bool fits=true;
   for(const auto* rows:{&reads.transforms,&reads.modelUniforms,&reads.teamColors}){
    if(rows->capacity()>(ByteLimit-bytes-cost)/sizeof(uint32_t)){fits=false;break;}
    cost+=rows->capacity()*sizeof(uint32_t);
   }
   if(!fits)continue;
   Entry entry{draw.program,draw.modelInputs,draw.engineUniforms,draw.instances,std::move(reads)};
   entries.emplace(key,std::move(entry));bytes+=cost;
  }
 }
 static const NativeCUSRenderReadSet* Find(const NativeCUSSourceDraw& draw){
  if(!active)return nullptr;
  const auto it=active->entries.find(KeyFor(draw));
  return it==active->entries.end()||!it->second.Matches(draw)?nullptr:&it->second.reads;
 }
 void SetPreparationNanoseconds(uint64_t value){preparationNanoseconds=value;}
 static uint64_t ActivePreparationNanoseconds(){return active?active->preparationNanoseconds:0;}
 static size_t ActiveCount(){return active?active->entries.size():0;}
 size_t Count()const{return entries.size();}
 size_t Bytes()const{return bytes;}
private:
 using Key=std::array<uintptr_t,4>;
 static Key KeyFor(const NativeCUSSourceDraw& d){return {reinterpret_cast<uintptr_t>(d.program.get()),reinterpret_cast<uintptr_t>(d.modelInputs.get()),reinterpret_cast<uintptr_t>(d.engineUniforms.get()),reinterpret_cast<uintptr_t>(d.instances.get())};}
 struct Entry {
  std::shared_ptr<const NativeCUSProgramConsumer> program;
  std::shared_ptr<const NativeCUSModelInputs> modelInputs;
  std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
  NativeBufferSnapshotRef instances;
  NativeCUSRenderReadSet reads;
  template<class A,class B>static bool SameOwner(const A& a,const B& b){return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
  bool Matches(const NativeCUSSourceDraw& d)const{return SameOwner(program,d.program)&&SameOwner(modelInputs,d.modelInputs)&&SameOwner(engineUniforms,d.engineUniforms)&&SameOwner(instances,d.instances);}
 };
 std::map<Key,Entry> entries;size_t bytes=0;uint64_t preparationNanoseconds=0;
 NativeCUSCPUReadScope* previous;
 inline static thread_local NativeCUSCPUReadScope* active=nullptr;
 inline static thread_local NativeCUSGeometryReadCache geometry;
};
} // namespace rendering
