/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSRenderInputs.h"
#include <map>
#include <stdexcept>
#include <utility>

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
 public:
  // A sealed CPU receipt. Source/GPU owners are released by the receiving main
  // thread; the worker's active scope and geometry cache are never transferred.
  class Packet {
   friend class NativeCUSCPUReadScope;
   std::map<Key,Entry> entries;
   size_t bytes=0;
  public:
   Packet()=default;
   Packet(Packet&&)=default;
   Packet& operator=(Packet&&)=default;
   Packet(const Packet&)=delete;
   Packet& operator=(const Packet&)=delete;
   size_t Count()const{return entries.size();}
   size_t Bytes()const{return bytes;}
   uint64_t RowDigest()const{
    uint64_t sum=0;
    for(const auto& [key,entry]:entries){
     uint64_t hash=14695981039346656037ull;
     for(const auto* rows:{&entry.reads.transforms,&entry.reads.modelUniforms,&entry.reads.teamColors}){
      hash=(hash^rows->size())*1099511628211ull;
      hash=(hash^rows->capacity())*1099511628211ull;
      for(uint32_t row:*rows)hash=(hash^row)*1099511628211ull;
     }
     sum+=hash; // Order-independent diagnostic only, never an identity check.
    }
    return sum;
   }
  };
  Packet Seal(){Packet out;out.entries=std::move(entries);out.bytes=std::exchange(bytes,0);return out;}
  void Adopt(Packet&& packet){
   if(!entries.empty()||bytes)throw std::logic_error("CUS CPU receipt must precede alpha/shadow priming");
   entries=std::move(packet.entries);bytes=std::exchange(packet.bytes,0);
  }
  bool Matches(const Packet& packet)const{
   if(bytes!=packet.bytes||entries.size()!=packet.entries.size())return false;
   auto right=packet.entries.begin();
   for(const auto& [key,left]:entries){
    const auto& r=right->second;
    if(key!=right->first||!Entry::SameOwner(left.program,r.program)||!Entry::SameOwner(left.modelInputs,r.modelInputs)||
       !Entry::SameOwner(left.engineUniforms,r.engineUniforms)||!Entry::SameOwner(left.instances,r.instances))return false;
    const auto same=[](const auto& a,const auto& b){return a==b&&a.capacity()==b.capacity();};
    if(!same(left.reads.transforms,r.reads.transforms)||!same(left.reads.modelUniforms,r.reads.modelUniforms)||!same(left.reads.teamColors,r.reads.teamColors))return false;
    ++right;
   }
   return true;
  }
  static size_t GeometryCacheSize(){return geometry.Size();}
 private:
  std::map<Key,Entry> entries;size_t bytes=0;uint64_t preparationNanoseconds=0;
 NativeCUSCPUReadScope* previous;
 inline static thread_local NativeCUSCPUReadScope* active=nullptr;
 inline static thread_local NativeCUSGeometryReadCache geometry;
};
} // namespace rendering
