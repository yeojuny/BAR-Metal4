/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "System/Log/ILog.h"
#endif
#include <initializer_list>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>

namespace rendering {
// BEGIN WORLD_PLAN167_EXTRACT
// Invocation-local indexing, never a validation receipt or address-keyed cache.
// The production caller owns an EndFrame/SnapshotPrefix publication throughout
// Prepare. Typed inputs may have their own versions: every old payload,
// target, image/readability and completion validator remains at its consumer.
enum class NativeWorldFamily167 : unsigned { Grass, ResourceSpots, ObjectSpotlight, ReclaimField, SensorLOS };
struct NativeWorldPlanMetrics167 {
 std::uint64_t inputDraws=0,indexBuildVisits=0,selectionIndexVisits=0,entryChecks=0;
 std::uint64_t legacyFamilyScanVisits=0,legacySelectionScanVisits=0;
 std::uint64_t copiedCommandsAvoided=0,copiedCommandBytesAvoided=0,copiedVertexBytesAvoided=0,copiedIndexBytesAvoided=0;
 std::uint64_t planOwnerAllocations=0,planVectorGrowths=0,planRequestedBytes=0,planCapacityBytes=0;
 std::uint64_t selectionOwnerAllocations=0,selectionVectorGrowths=0,selectionRequestedBytes=0;
 double planMicroseconds=0,selectionMicroseconds=0;
 bool timingCollected=false;
};
namespace worldPlan167 {
using Clock=std::chrono::steady_clock;
inline bool Observe174(std::uint64_t frame) {
 if(frame<2048||frame>=2112)return false;
 static const bool enabled=[] {const char* p=std::getenv("BAR_NATIVE_GEOMETRY_MRT174");return p&&p[0]=='1'&&p[1]=='\0';}();
 return enabled;
}
constexpr unsigned StageCount=unsigned(NativeWorldDrawStage::ModelDeferred)+1;
constexpr unsigned FamilyCount=5,MainBucket=FamilyCount,StageBase=MainBucket+1,BucketCount=StageBase+StageCount;
inline void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
inline unsigned FamilyBits(const NativeWorldDrawCommand& c) {
 return unsigned(bool(c.grass))|(unsigned(bool(c.resourceSpots))<<1)|(unsigned(bool(c.objectSpotlight))<<2)|
  (unsigned(bool(c.reclaimField))<<3)|(unsigned(bool(c.sensorLOS))<<4);
}
inline bool MainStage(NativeWorldDrawStage s) {
 return s==NativeWorldDrawStage::PreDecals||s==NativeWorldDrawStage::PreUnit||s==NativeWorldDrawStage::World||
  s==NativeWorldDrawStage::AlphaBelowWater||s==NativeWorldDrawStage::AlphaAboveWater||
  s==NativeWorldDrawStage::WaterPost||s==NativeWorldDrawStage::AlphaAll;
}
struct Entry {
 std::uint64_t sequence=0;
 NativeWorldDrawStage stage{};
 unsigned families=0;
 std::size_t vertexBytes=0,indexBytes=0;
};
// A command may belong to a stage, the main pass and multiple typed families.
// Use the same membership rules for counting and placement.
template<class F> inline void ForBuckets(const Entry& entry,F visit) {
 const unsigned stage=unsigned(entry.stage);
 if(stage<StageCount)visit(StageBase+stage);
 if(MainStage(entry.stage))visit(MainBucket);
 for(unsigned bits=entry.families;bits;bits&=bits-1)visit(unsigned(std::countr_zero(bits)));
}
struct State {
 std::shared_ptr<const NativeWorldDrawFrame> owner;
 std::uint64_t serial=0,sourceDrawFrame=0;
 std::vector<Entry> entries;
 std::array<std::size_t,BucketCount+1> offsets{};
 std::vector<std::size_t> ordered;
 std::span<const std::size_t> Bucket(unsigned bucket)const {
  const auto begin=offsets.at(bucket),end=offsets.at(bucket+1);
  return std::span<const std::size_t>(ordered).subspan(begin,end-begin);
 }
 std::array<std::array<std::uint64_t,2>,BucketCount> geometryBytes{};
 mutable NativeWorldPlanMetrics167 metrics;
 void CheckOwner() const {
  Require(!owner ? entries.empty() : owner->serial==serial&&owner->draws.size()==entries.size(),
   "World167 selection source generation/extent changed during preparation");
 }
 const NativeWorldDrawCommand& At(std::size_t index) const {
  CheckOwner();Require(owner&&index<entries.size(),"World167 selection index outside exact owner");
  const auto& c=owner->draws.at(index);const auto& e=entries.at(index);
  Require(c.sequence==e.sequence&&c.stage==e.stage&&FamilyBits(c)==e.families,
   "World167 selection command ordinal/stage/family changed during preparation");
  ++metrics.entryChecks;return c;
 }
};
template<class T> inline void Push(std::vector<T>& out,T value,NativeWorldPlanMetrics167& m,bool selection) {
 const auto capacity=out.capacity();out.push_back(std::move(value));
 if(out.capacity()!=capacity){
  if(selection){++m.selectionVectorGrowths;m.selectionRequestedBytes+=out.capacity()*sizeof(T);}
  else {++m.planVectorGrowths;m.planRequestedBytes+=out.capacity()*sizeof(T);}
 }
}
} // namespace worldPlan167
class NativeWorldDrawPlan167;
class NativeWorldDrawSelection167 {
 std::shared_ptr<const worldPlan167::State> state;
 // Non-null only for StageWhere. Ordinary buckets live in the retained state.
 std::shared_ptr<const std::vector<std::size_t>> indices;
 unsigned kind=worldPlan167::MainBucket;
 NativeWorldDrawSelection167(std::shared_ptr<const worldPlan167::State> s,
  std::shared_ptr<const std::vector<std::size_t>> i,unsigned k):state(std::move(s)),indices(std::move(i)),kind(k){}
 friend class NativeWorldDrawPlan167;
public:
 bool empty()const{return size()==0;}
 std::size_t size()const{return indices?indices->size():state->offsets[kind+1]-state->offsets[kind];}
 std::uint64_t SourceDrawFrame()const{return state->sourceDrawFrame;}
 std::uint64_t Serial()const{return state->serial;}
 const std::shared_ptr<const NativeWorldDrawFrame>& Owner()const{return state->owner;}
 void RequireFamily(NativeWorldFamily167 family,std::uint64_t sourceFrame)const {
  state->CheckOwner();worldPlan167::Require(kind==unsigned(family)&&sourceFrame==state->sourceDrawFrame,
   "World167 family selection belongs to another consumer/source frame");
 }
 void RequireOverlay()const {
  state->CheckOwner();worldPlan167::Require(kind>=worldPlan167::MainBucket,
   "World167 typed-family view cannot replace an original overlay invocation");
 }
 const NativeWorldDrawCommand& At(std::size_t position)const {
  if(indices)return state->At(indices->at(position));
  if(position>=size())throw std::out_of_range("World selection position outside bucket");
  return state->At(state->ordered.at(state->offsets[kind]+position));
 }
 struct Iterator {
  using iterator_category=std::forward_iterator_tag;
  using value_type=NativeWorldDrawCommand;
  using difference_type=std::ptrdiff_t;
  using pointer=const NativeWorldDrawCommand*;
  using reference=const NativeWorldDrawCommand&;
  const NativeWorldDrawSelection167* selection=nullptr;std::size_t position=0;
  reference operator*()const{return selection->At(position);}
  pointer operator->()const{return &selection->At(position);}
  Iterator& operator++(){++position;return *this;}
  Iterator operator++(int){auto old=*this;++*this;return old;}
  bool operator==(const Iterator&)const=default;
 };
 Iterator begin()const{return {this,0};}
 Iterator end()const{return {this,size()};}
};
class NativeWorldDrawPlan167 {
 std::shared_ptr<const worldPlan167::State> state;
 NativeWorldDrawSelection167 Bucket(unsigned bucket,bool copies,bool familyProbe)const {
  state->CheckOwner();const auto indices=state->Bucket(bucket);auto& m=state->metrics;
  const auto started=m.timingCollected?worldPlan167::Clock::now():worldPlan167::Clock::time_point{};
  if(copies){m.legacySelectionScanVisits+=m.inputDraws;m.copiedCommandsAvoided+=indices.size();m.copiedCommandBytesAvoided+=indices.size()*sizeof(NativeWorldDrawCommand);m.copiedVertexBytesAvoided+=state->geometryBytes[bucket][0];m.copiedIndexBytesAvoided+=state->geometryBytes[bucket][1];}
  if(bucket<worldPlan167::FamilyCount){
   // The four old typed consumers first did none_of, then a complete frame
   // loop if present. SensorLOS had only the complete-frame loop.
   m.legacyFamilyScanVisits+=familyProbe?(indices.empty()?m.inputDraws:indices.front()+1+m.inputDraws):m.inputDraws;
  }
  if(m.timingCollected)m.selectionMicroseconds+=std::chrono::duration<double,std::micro>(worldPlan167::Clock::now()-started).count();
  return NativeWorldDrawSelection167(state,{},bucket);
 }
public:
 NativeWorldDrawPlan167(std::shared_ptr<const NativeWorldDrawFrame> owner,std::uint64_t sourceFrame,bool timing=false) {
  const bool observe174=worldPlan167::Observe174(sourceFrame);
  timing=timing||observe174;
  const auto started=timing?worldPlan167::Clock::now():worldPlan167::Clock::time_point{};
  auto s=std::make_shared<worldPlan167::State>();s->owner=std::move(owner);s->sourceDrawFrame=sourceFrame;
  auto& m=s->metrics;m.timingCollected=timing;m.planOwnerAllocations=1;m.planRequestedBytes=sizeof(worldPlan167::State);
  if(s->owner){
   s->serial=s->owner->serial;m.inputDraws=s->owner->draws.size();
   if(m.inputDraws){s->entries.reserve(m.inputDraws);++m.planVectorGrowths;m.planRequestedBytes+=s->entries.capacity()*sizeof(worldPlan167::Entry);}
   for(std::size_t i=0;i<s->owner->draws.size();++i){
    const auto& c=s->owner->draws[i];const unsigned bits=worldPlan167::FamilyBits(c);
    worldPlan167::Push(s->entries,worldPlan167::Entry{c.sequence,c.stage,bits,c.vertices.size()*sizeof(NativeWorldDrawVertex),c.indices.size()*sizeof(std::uint32_t)},m,false);
    const auto& entry=s->entries.back();
    worldPlan167::ForBuckets(entry,[&](unsigned bucket){
     ++s->offsets[bucket+1];
     s->geometryBytes[bucket][0]+=entry.vertexBytes;s->geometryBytes[bucket][1]+=entry.indexBytes;
    });
    ++m.indexBuildVisits;
   }
   for(unsigned b=0;b<worldPlan167::BucketCount;++b){
    if(s->offsets[b+1]>s->ordered.max_size()-s->offsets[b])throw std::length_error("World selection index extent overflow");
    s->offsets[b+1]+=s->offsets[b];
   }
   if(s->offsets.back()){
    s->ordered.resize(s->offsets.back());++m.planVectorGrowths;
    m.planRequestedBytes+=s->ordered.capacity()*sizeof(std::size_t);
    auto cursor=s->offsets;
    for(std::size_t i=0;i<s->entries.size();++i)
     worldPlan167::ForBuckets(s->entries[i],[&](unsigned bucket){s->ordered[cursor[bucket]++]=i;});
   }
  }
  m.planCapacityBytes=sizeof(worldPlan167::State)+s->entries.capacity()*sizeof(worldPlan167::Entry);
  m.planCapacityBytes+=s->ordered.capacity()*sizeof(std::size_t);
  if(timing)m.planMicroseconds=std::chrono::duration<double,std::micro>(worldPlan167::Clock::now()-started).count();
#ifdef RECOIL_METAL4_LIVE_SCENE
  if(observe174){
   // One bounded plan input row per invocation; enough to replay the same
   // ordered memberships and geometry extents against the archived parent.
   char input[4096]{};size_t used=0,emitted=0;
   for(const auto& e:s->entries){
    const int n=std::snprintf(input+used,sizeof(input)-used,"%s%x:%x:%zx:%zx",emitted?",":"",unsigned(e.stage),e.families,e.vertexBytes/sizeof(NativeWorldDrawVertex),e.indexBytes/sizeof(std::uint32_t));
    if(n<0||size_t(n)>=sizeof(input)-used){input[used]='\0';break;}
    used+=size_t(n);++emitted;
   }
   LOG("[NativeWorldPlan174] frame=%llu serial=%llu sources=%zu emitted=%zu input_visits=%llu placement_visits=%zu memberships=%zu growths=%llu requested=%llu capacity=%llu plan_us=%.3f input=%s",
    (unsigned long long)sourceFrame,(unsigned long long)s->serial,s->entries.size(),emitted,(unsigned long long)m.indexBuildVisits,s->ordered.empty()?size_t(0):s->entries.size(),s->ordered.size(),(unsigned long long)m.planVectorGrowths,(unsigned long long)m.planRequestedBytes,(unsigned long long)m.planCapacityBytes,m.planMicroseconds,input);
  }
#endif
  state=std::move(s);
 }
 NativeWorldDrawSelection167 Main()const{return Bucket(worldPlan167::MainBucket,true,false);}
 NativeWorldDrawSelection167 Stage(NativeWorldDrawStage stage)const {
  worldPlan167::Require(unsigned(stage)<worldPlan167::StageCount,"World167 selected stage outside source enum");
  return Bucket(worldPlan167::StageBase+unsigned(stage),true,false);
 }
 NativeWorldDrawSelection167 Family(NativeWorldFamily167 family)const {
  worldPlan167::Require(unsigned(family)<worldPlan167::FamilyCount,"World167 unknown selected family");
  return Bucket(unsigned(family),false,family!=NativeWorldFamily167::SensorLOS);
 }
 template<class Predicate> NativeWorldDrawSelection167 StageWhere(NativeWorldDrawStage stage,Predicate predicate)const {
  state->CheckOwner();worldPlan167::Require(unsigned(stage)<worldPlan167::StageCount,"World167 selected stage outside source enum");
  const unsigned bucket=worldPlan167::StageBase+unsigned(stage);
  // No predicate can run for an empty stage. Retain the existing bucket and
  // its exact owner without allocating a separate selection vector.
  const auto source=state->Bucket(bucket);
  if(source.empty())return Bucket(bucket,true,false);
  auto& m=state->metrics;const auto started=m.timingCollected?worldPlan167::Clock::now():worldPlan167::Clock::time_point{};
  auto selected=std::make_shared<std::vector<std::size_t>>();++m.selectionOwnerAllocations;m.selectionRequestedBytes+=sizeof(*selected);
  for(auto index:source){
   ++m.selectionIndexVisits;
   if(predicate(state->At(index))){worldPlan167::Push(*selected,index,m,true);++m.copiedCommandsAvoided;m.copiedCommandBytesAvoided+=sizeof(NativeWorldDrawCommand);m.copiedVertexBytesAvoided+=state->entries[index].vertexBytes;m.copiedIndexBytesAvoided+=state->entries[index].indexBytes;}
  }
  // Predicate executes for every same-stage command, including out-of-span
  // rejection; nested/deferred invocations never get silently filtered away.
  m.legacySelectionScanVisits+=m.inputDraws;
  if(m.timingCollected)m.selectionMicroseconds+=std::chrono::duration<double,std::micro>(worldPlan167::Clock::now()-started).count();
  return NativeWorldDrawSelection167(state,std::move(selected),bucket);
 }
 NativeWorldPlanMetrics167 Metrics()const{return state->metrics;}
 std::uint64_t Serial()const{return state->serial;}
};
// END WORLD_PLAN167_EXTRACT
} // namespace rendering

