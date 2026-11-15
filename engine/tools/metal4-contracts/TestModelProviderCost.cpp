/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Host regression: actual provider scope + positive-fact table + trace format.
// Only Metal lease, sampled trace sink and thread-CPU clock are modeled.
#include "Rendering/Backend/NativeCPUPhaseTrace.h"
#include "Rendering/Backend/NativePositiveBackingFacts.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <iostream>

namespace rendering {
struct NativeModelTextureBacking final:NativeTextureBacking {
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}
};
struct OtherBacking final:NativeTextureBacking {
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}
};
namespace metal4 {
struct RuntimeLease {
 const void* context=this;std::uint64_t submission=7;
 const void* ContextIdentity()const{return context;}
 std::uint64_t SourceSubmission()const{return submission;}
};
// Reproduce the live renderer's disabled clock. The fixed scope must not use it.
struct Clock:std::chrono::steady_clock {static time_point now()noexcept{return {};}};
static bool sampled=true;
static NativeCPUPhaseBuffer buffer;
static std::uint64_t nextID=1;
struct Slot {const void* owner;NativeCPUPhaseSample sample;};
static std::vector<Slot> stack;
static std::uint64_t cpuCalls=0;
static std::uint64_t invalidAt=0;
NativeThreadCPUStamp TestCPUTime()noexcept{
 ++cpuCalls;return {cpuCalls*100,cpuCalls!=invalidAt};
}
NativeCPUPhaseContext CaptureNativeCPUPhaseContext()noexcept {
 NativeCPUPhaseContext context;context.sampled=sampled;context.buffer=sampled?&buffer:nullptr;return context;
}
std::int64_t NativeCPUTraceFrame()noexcept{return 300;}
static std::uint64_t Wall(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// The modeled sink uses this pointer only as an identity, never dereferences it.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((access(none,1)))
#endif
bool BeginNativeCPUPhase(const void* owner,unsigned kind,std::int64_t frame)noexcept{
 if(!sampled||kind>=unsigned(NativeCPUPhaseKind::Count))return false;
 NativeCPUPhaseSample s;s.id=nextID++;s.parent=stack.empty()?0:stack.back().sample.id;
 s.start=Wall();s.frame=frame;s.kind=kind;s.incoming.request=1;s.incoming.draw=11;s.incoming.simulation=300;
 s.job=s.incoming;s.job.job=2;s.job.lease=3;s.job.submission=4;s.job.event=5;s.job.runtimeIndex=6;
 stack.push_back({owner,s});return true;
}
void CountNativeCPUPhase(const void* owner,std::uint64_t a,std::uint64_t b)noexcept{
 assert(!stack.empty()&&stack.back().owner==owner);stack.back().sample.bytes=a;stack.back().sample.allocations=b;
}
void SetNativeCPUPhaseStatus(const void* owner,unsigned status)noexcept{
 assert(!stack.empty()&&stack.back().owner==owner);stack.back().sample.status=status;
}
void FinishNativeCPUPhase(const void* owner)noexcept{
 assert(!stack.empty()&&stack.back().owner==owner);buffer.Append(stack.back().sample,Wall());stack.pop_back();
}
#define ReadNativeThreadCPUTime TestCPUTime
#include "Rendering/Metal4/Metal4ModelTypeFacts.inc"
#undef ReadNativeThreadCPUTime

static void Reset(){assert(stack.empty());buffer={};cpuCalls=0;invalidAt=0;}
static void ModelCall(ModelTypeFactsScope& scope,RuntimeLease& lease,const std::shared_ptr<const NativeTextureBacking>& backing,bool fail=false){
 ModelTypeFactsScope::ProviderCall call(&scope,lease,lease.submission);
 auto model=call.Find(backing);std::optional<ModelTypeFactsScope::Facts::Proof> proof;
 if(!model){
  if(call.ReuseEnabled()){proof.emplace(ModelTypeFactsScope::Facts::Classify(backing));model=proof->Typed();}
  else model=std::dynamic_pointer_cast<const NativeModelTextureBacking>(backing);
 }
 assert(model);call.Positive(proof);
 if(fail)throw std::runtime_error("modeled post-dispatch provider failure");
}
static void CheckPartition(const ModelTypeFactsScope& s){
 assert(s.requests==s.completedModel+s.completedOther+s.failed);
 assert(s.providerWallNs==s.dispatchTime.wall+s.preparationTime.wall+s.otherTime.wall+s.failureTime.wall);
 assert(s.providerCPUNs==s.dispatchTime.cpu+s.preparationTime.cpu+s.otherTime.cpu+s.failureTime.cpu);
}
static void CheckSummary(){
 assert(buffer.size==13&&buffer.dropped==0);
 const auto& summary=buffer.records[12].sample;
 assert(summary.kind==unsigned(NativeCPUPhaseKind::ModelProviderSummary));
 for(std::size_t i=0;i<12;++i)assert(buffer.records[i].sample.parent==summary.id);
 assert(unsigned(NativeCPUPhaseKind::ModelProviderFailureTime)<unsigned(NativeCPUPhaseKind::Count));
}
int Test(bool emit){
 assert(Clock::now().time_since_epoch().count()==0);
 assert(ModelTypeFactsScope::ProviderCall::Now()>0);
 RuntimeLease lease;auto backing=std::make_shared<NativeModelTextureBacking>();
 if(emit){
  Reset();{
   ModelTypeFactsScope scope(lease,7);ModelCall(scope,lease,backing);ModelCall(scope,lease,backing);
   {ModelTypeFactsScope::ProviderCall call(&scope,lease,7);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
   CheckPartition(scope);
  }CheckSummary();
  for(std::size_t i=0;i<buffer.size;++i){char text[4096];auto& r=buffer.records[i];auto n=FormatNativeCPUPhase(text,sizeof(text),r.sample,r.end);assert(n);std::cout<<text<<'\n';}
  return 0;
 }
 for(int i=0;i<1000;++i){
  Reset();{
   ModelTypeFactsScope scope(lease,7);ModelCall(scope,lease,backing);ModelCall(scope,lease,backing);
   assert(scope.requests==2&&scope.completedModel==2&&scope.completedOther==0&&scope.failed==0);
   assert(scope.hits==(scope.enabled?1u:0u));assert(scope.positiveCasts==(scope.enabled?1u:2u));
   assert(scope.facts.Size()==(scope.enabled?1u:0u));assert(scope.invalidCPU==0&&scope.invalidWall==0);
   assert(scope.dispatchTime.cpu==200&&scope.preparationTime.cpu==200);CheckPartition(scope);
  }CheckSummary();
 }
 Reset();{
  ModelTypeFactsScope scope(lease,7);
  {ModelTypeFactsScope::ProviderCall call(&scope,lease,7);auto other=std::make_shared<OtherBacking>();assert(!call.Find(other));}
  try{ModelTypeFactsScope::ProviderCall call(&scope,lease,7);throw std::runtime_error("before model");}catch(const std::runtime_error&){}
  try{ModelCall(scope,lease,backing,true);}catch(const std::runtime_error&){}
  assert(scope.completedOther==1&&scope.failed==2&&scope.completedModel==0);
  assert(scope.dispatchTime.cpu==0&&scope.preparationTime.cpu==0&&scope.failureTime.cpu==300);CheckPartition(scope);
 }
 // Invalid model midpoint must invalidate the whole call's CPU partition,
 // not silently combine a valid total with incomplete model subtotals.
 for(std::uint64_t failureAt:{1u,2u,3u}){
  Reset();invalidAt=failureAt;{
   ModelTypeFactsScope scope(lease,7);ModelCall(scope,lease,backing);
   assert(scope.invalidCPU==1&&scope.providerCPUNs==0&&scope.invalidWall==0);CheckPartition(scope);
  }
 }
 Reset();{
  ModelTypeFactsScope scope(lease,7);
  {ModelTypeFactsScope::ProviderCall call(&scope,lease,7);call.wall=std::numeric_limits<std::uint64_t>::max();call.cpu.ns=std::numeric_limits<std::uint64_t>::max();}
  assert(scope.invalidWall==1&&scope.invalidCPU==1&&scope.providerWallNs==0&&scope.providerCPUNs==0);CheckPartition(scope);
 }
 Reset();{
  ModelTypeFactsScope scope(lease,7);
  {ModelTypeFactsScope::ProviderCall call(&scope,lease,7);call.Positive({});call.Positive({});}
  assert(scope.invalidWall==1&&scope.invalidCPU==1);CheckPartition(scope);
 }
 Reset();{
  ModelTypeFactsScope scope(lease,7);RuntimeLease foreign;
  {ModelTypeFactsScope::ProviderCall call(&scope,foreign,7);assert(!call.ReuseEnabled());}
  {ModelTypeFactsScope::ProviderCall call(&scope,lease,8);assert(!call.ReuseEnabled());}
  assert(scope.scopeMismatch==2);CheckPartition(scope);
 }
 Reset();sampled=false;{
  ModelTypeFactsScope scope(lease,7);assert(!scope.diagnostic);
  for(int i=0;i<1000;++i)ModelCall(scope,lease,backing);
  assert(cpuCalls==0&&scope.requests==0&&scope.providerWallNs==0);
 }assert(buffer.size==0);sampled=true;
 Reset();{ModelTypeFactsScope::ProviderCall call(nullptr,lease,7);call.Find(backing);call.Positive({});}assert(cpuCalls==0&&buffer.size==0);
 // Backing lifetime is still the original weak-fact-cache contract.
 Reset();{
  ModelTypeFactsScope scope(lease,7);std::weak_ptr<const NativeTextureBacking> weak;
  {auto temporary=std::make_shared<NativeModelTextureBacking>();weak=temporary;ModelCall(scope,lease,temporary);}
  assert(weak.expired());
 }
 std::cout<<"PASS: actual provider scope, live zero-clock regression, OFF/ON facts, 1000 partitions, failures, invalid stamps, unsampled zero CPU reads, weak lifetime, 13-row summary\n";
 return 0;
}
}}
int main(int argc,char** argv){return rendering::metal4::Test(argc==2&&std::string(argv[1])=="--emit");}
