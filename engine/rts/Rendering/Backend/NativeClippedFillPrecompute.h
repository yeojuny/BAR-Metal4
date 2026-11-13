/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawGeometry.h"
#include <bit>
#include <cfenv>
#include <cstring>
#include <mutex>
#include <thread>
#include <limits>
namespace rendering {
// Optional one-invocation speculative cache. Main owns control calls and final
// destruction; only the pure kernel runs on the worker. Demand must still perform
// original validation, target checks and cache/packet/borrow selection.
class NativeClippedFillPrecompute final {
public:
 struct Limits {
  size_t candidates=8, indices=65536, logicalOutputBytes=8*1024*1024, scanCommands=4096;
 };
 enum class State { Pending, Running, Cancelled, Ready };
 template<class Select>
 NativeClippedFillPrecompute(std::shared_ptr<const NativeWorldDrawFrame> source,Select&& select,Limits limits) noexcept
  :owner(std::move(source)) {
  try {
   if(!owner)return;
   size_t usedIndices=0,usedBytes=0;
   for(size_t index=0;index<owner->draws.size()&&index<limits.scanCommands&&entries.size()<limits.candidates;++index){
    const auto& d=owner->draws[index];
    if(!select(d)||!IsIndependentNativeWorldClippedFill(d))continue;
    if(d.indices.empty()||d.indices.size()%3||d.indices.size()>limits.indices-usedIndices)continue;
    constexpr size_t triangleBytes=4*sizeof(NativeWorldDrawVertex)+6*sizeof(uint32_t);
    const size_t triangles=d.indices.size()/3;
    if(triangles>std::numeric_limits<uint32_t>::max()/4||triangles>(limits.logicalOutputBytes-usedBytes)/triangleBytes)continue;
    // Charge even rejected malformed candidates: validation work is bounded too.
    usedIndices+=d.indices.size();
    // Pure safety preflight only. The original full validator is deliberately
    // not advanced here. No provider, typed owner or diagnostic is visited.
    static_assert(sizeof(float)==sizeof(uint32_t)&&std::numeric_limits<float>::is_iec559);
    const auto finite=[](float value)noexcept{return (std::bit_cast<uint32_t>(value)&0x7f800000u)!=0x7f800000u;};
    bool valid=true;
    for(float value:*d.world.inheritedClipPlane)valid&=finite(value);
    for(auto i:d.indices){
     if(i>=d.vertices.size()){valid=false;break;}
     const auto& v=d.vertices[i];
     for(float value:v.position)valid&=finite(value);
     for(float value:v.color)valid&=finite(value);
     for(float value:v.uv)valid&=finite(value);
    }
    if(!valid)continue;
    entries.push_back(Entry{index,*d.world.inheritedClipPlane});
    usedBytes+=triangles*triangleBytes;
   }
  } catch(...) { entries.clear(); }
 }
 NativeClippedFillPrecompute(const NativeClippedFillPrecompute&)=delete;
 NativeClippedFillPrecompute& operator=(const NativeClippedFillPrecompute&)=delete;
 ~NativeClippedFillPrecompute(){CancelAndJoin();}
 // No throwing operation follows successful thread publication. Failure leaves
 // an unavailable optional receipt, and never advances a rendering exception.
 bool Start() noexcept {
  try {
   std::lock_guard lock(mutex);
   if(started||state!=State::Pending||entries.empty())return false;
   rounding=std::fegetround();
   if(rounding<0||std::fegetenv(&environment)!=0){state=State::Cancelled;return false;}
   thread=std::thread([this]{Run();});started=true;return true;
  } catch(...) {state=State::Cancelled;return false;}
 }
 // Cancellation wins only before the worker claims Pending. Running work is
 // joined, then discarded; it is never interrupted or detached.
 void CancelAndJoin() noexcept {
  {std::lock_guard lock(mutex);if(state==State::Pending)state=State::Cancelled;discard=true;}
  Join();
 }
 bool TryTake(const std::shared_ptr<const NativeWorldDrawFrame>& exactOwner,
              const NativeWorldDrawCommand* source,NativeWorldRasterMesh& out) noexcept {
  if(!exactOwner||exactOwner.get()!=owner.get()||exactOwner.owner_before(owner)||owner.owner_before(exactOwner))return false;
  Entry* selected=nullptr;
  for(auto& entry:entries)if(&owner->draws[entry.index]==source){selected=&entry;break;}
  if(!selected)return false;
  Join(); // Terminal synchronization before any worker-owned result is read.
  if(!started||state!=State::Ready||discard||!selected->ready||selected->consumed||std::fegetround()!=rounding)return false;
  // Conservative exact environment match: representations may false-miss, but
  // mismatches never reuse numeric output. No worker exception flags disappear.
  std::fenv_t current{};
  if(std::fegetenv(&current)!=0||std::memcmp(&current,&environment,sizeof(current))!=0)return false;
  if(std::feraiseexcept(selected->exceptions)!=0)return false;
  selected->consumed=true;out=std::move(selected->mesh);return true;
 }
 State Status() const {std::lock_guard lock(mutex);return state;}
 size_t CandidateCount()const noexcept{return entries.size();}
private:
 struct Entry {
  size_t index;std::array<float,4> plane;
  NativeWorldRasterMesh mesh;
  bool ready=false,consumed=false;
  int exceptions=0;
 };
 void Join()noexcept {if(thread.joinable())thread.join();}
 void Run() noexcept {
  {std::lock_guard lock(mutex);if(state==State::Cancelled)return;state=State::Running;}
  std::fenv_t held{};
  if(std::fesetenv(&environment)!=0||std::feholdexcept(&held)!=0){std::lock_guard lock(mutex);state=State::Cancelled;return;}
  for(auto& entry:entries){
   try {
    if(std::feclearexcept(FE_ALL_EXCEPT)!=0)continue;
    const auto& d=owner->draws[entry.index];entry.mesh=BuildValidatedNativeWorldClippedFill(d.vertices,d.indices,entry.plane);
    entry.exceptions=std::fetestexcept(FE_ALL_EXCEPT);entry.ready=true;
   }
   catch(...) {/* Optional failure: demand rebuilds through the original path. */}
  }
  {std::lock_guard lock(mutex);state=State::Ready;}
 }
 // Limits bound candidate work and logical mesh bytes, not allocator capacity,
 // temporary polygons or total process heap usage. No performance claim implied.
 std::shared_ptr<const NativeWorldDrawFrame> owner;
 std::vector<Entry> entries;
 mutable std::mutex mutex;
 std::thread thread;
 std::fenv_t environment{};
 int rounding=FE_TONEAREST;
 State state=State::Pending;
 bool started=false,discard=false;
};
}
