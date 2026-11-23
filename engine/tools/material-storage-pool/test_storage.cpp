/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeRetiredBufferStorage.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

struct Buffer {std::uint64_t id;std::array<std::uint64_t,24> words{};};
using Handle=std::shared_ptr<Buffer>;
using Pool=rendering::NativeRetiredBufferStorage<Handle,256>;
static std::uint64_t nextID=0;
static Pool::Storage Make(){return {std::make_shared<Buffer>(Buffer{++nextID,{}}),4096};}

void Boundaries(){
 using Small=rendering::NativeRetiredBufferStorage<Handle,2>;
 Small pool;int allocations=0;
 auto factory=[&]{++allocations;auto v=Make();return Small::Storage{v.buffer,v.allocatedBytes};};
 auto first=pool.Acquire(192,factory);assert(first.created);
 first.owner->buffer->words.fill(77);
 // Cache eviction alone, worker completion, or only one completion signal
 // leaves the prepared/feedback/quarantine owner alive. No slot is writable.
 auto prepared=first.owner,feedback=first.owner,quarantine=first.owner;
 first.owner.reset();prepared.reset();
 pool.BeginBatch();auto second=pool.Acquire(192,factory);assert(second.created);
 assert(feedback->buffer->words[0]==77);
 feedback.reset();pool.BeginBatch();auto spill=pool.Acquire(192,factory);
 assert(spill.created&&pool.CachedCount()==2&&quarantine->buffer->words[23]==77);
 std::weak_ptr<Small::Storage> spilled=spill.owner;spill.owner.reset();assert(spilled.expired());
 auto oldID=quarantine->buffer->id;quarantine.reset();pool.BeginBatch();
 auto reuse=pool.Acquire(192,factory);assert(!reuse.created&&reuse.owner->buffer->id==oldID);
 assert(allocations==3&&pool.CachedAllocatedBytes()==8192);
 // Different extents cannot share a slot; full capacity still safely spills.
 auto other=pool.Acquire(256,factory);assert(other.created&&other.owner!=reuse.owner);
 auto retained=reuse.owner;const auto native=retained->buffer;pool.Clear();
 assert(pool.CachedCount()==0&&pool.CachedAllocatedBytes()==0);
 pool.BeginBatch();auto replacement=pool.Acquire(192,factory);
 assert(replacement.created&&replacement.owner->buffer!=native&&retained->buffer->words[0]==77);
 bool bad=false;try{pool.Acquire(0,factory);}catch(const std::invalid_argument&){bad=true;}assert(bad);
 Small failed;try{failed.Acquire(192,[]()->Small::Storage{throw std::runtime_error("allocation");});assert(false);}catch(const std::runtime_error&){}
 assert(failed.CachedCount()==0&&failed.CachedAllocatedBytes()==0);
 auto partiallyWritten=failed.Acquire(192,factory);partiallyWritten.owner->buffer->words.fill(123);
 partiallyWritten.owner.reset();failed.BeginBatch();auto recovered=failed.Acquire(192,factory);
 assert(!recovered.created);recovered.owner->buffer->words.fill(0);
 assert((recovered.owner->buffer->words==std::array<std::uint64_t,24>{}));
}

struct Outcome {std::uint64_t allocations=0,reuses=0,encodes=0,pins=0,hash=0;std::size_t cached=0;};
Outcome Churn(bool reuse){
 Pool pool;struct Entry{Pool::Owner owner;std::uint64_t seen;};
 std::map<std::uint64_t,Entry> content;
 std::vector<Pool::Owner> prepared,submitted,feedback,quarantine;
 std::array<std::uint64_t,24> held{};
 Outcome out;
 for(std::uint64_t batch=1;batch<=1000;++batch){
  pool.BeginBatch();prepared.clear();
  // MODELED workload with no consecutive-key hits: eight fresh material
  // contents per batch, each fetched twice. This is NOT a game trace replay.
  for(unsigned n=0;n<16;++n){
   const auto key=batch*8+(n%8);auto it=content.find(key);Pool::Owner owner;
   if(it!=content.end()){it->second.seen=batch;owner=it->second.owner;}
   else{
    if(reuse){auto a=pool.Acquire(192,Make);owner=std::move(a.owner);out.allocations+=a.created;out.reuses+=!a.created;}
    else{owner=std::make_shared<Pool::Storage>(Make());++out.allocations;}
    for(unsigned slot=0;slot<24;++slot)owner->buffer->words[slot]=(slot%3==0)?0:key*37+slot;
    if(content.size()==128){auto oldest=content.begin();for(auto e=content.begin();e!=content.end();++e)if(e->second.seen<oldest->second.seen)oldest=e;content.erase(oldest);}
    content.emplace(key,Entry{owner,batch});++out.encodes;
   }
   prepared.push_back(owner);++out.pins;
   for(auto word:owner->buffer->words)out.hash=out.hash*131+word;
  }
  if(batch==1){quarantine.push_back(prepared.front());held=quarantine.front()->buffer->words;}
  assert(quarantine.front()->buffer->words==held);
  // Submitted and late-feedback graphs remain after the next BeginBatch.
  feedback=std::move(submitted);submitted=prepared;
 }
 out.cached=pool.CachedCount();return out;
}
int main(){
 Boundaries();const auto a=Churn(false),b=Churn(true);
 assert(a.encodes==b.encodes&&a.pins==b.pins&&a.hash==b.hash);
 assert(a.allocations==8000&&b.reuses>0&&b.allocations<b.encodes&&b.cached<=256);
 std::cout<<"modeled_material_churn batches=1000 encodes="<<a.encodes<<" pins="<<a.pins
          <<" allocations_off="<<a.allocations<<" allocations_on="<<b.allocations<<" reuses="<<b.reuses
          <<" retained_pool_slots="<<b.cached<<" byte_results_equal=1 quarantine_unchanged=1 PASS\n";
 std::cout<<"pool boundaries: cache/prepared/feedback pins, capacity spill, layout, clear, failure, nil-slot rewrite PASS\n";
}
