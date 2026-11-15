/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeSharedProgramPilot.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <tuple>

namespace rendering::metal4 {
inline bool SharedCUSPilotEnabled() {
 static const bool enabled=[] {const auto* value=std::getenv("BAR_METAL4_SHARED_CUS_PILOT");return value&&std::strcmp(value,"1")==0;}();
 return enabled;
}
// Implemented only next to WorldTexturePreparationJournal's real completion state.
class CUSCompletedModelImageProof {
public:
 virtual ~CUSCompletedModelImageProof()=default;
 virtual bool ValidFor(RuntimeLease&,const NativeCUSTextureBinding&,const CUSSourceImage&) const=0;
};
// One lease's completed immutable sub-binding cache. Uniforms and every other
// texture remain per-use. No argument tables or cached readability verdicts.
class CUSSharedPreparationScope {
 using Key=std::tuple<uint64_t,std::vector<uint64_t>,uintptr_t,uintptr_t>;
 struct Entry {
  sharedpilot::PreparedSlice slice;
  std::array<CUSSourceImage,2> images;
 };
 RuntimeLease* lease;
 const void* context;
 __strong id<MTLDevice> device;
 uint64_t submission;
 uint64_t sourceContext;
 std::map<Key,Entry> entries;
 static constexpr size_t Capacity=128;
 static bool SameOwner(const std::shared_ptr<const NativeCUSTextureObject>& a,
                       const std::shared_ptr<const NativeCUSTextureObject>& b) {
  return a&&b&&a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);
 }
 static Key MakeKey(const sharedpilot::MaterialContent& material,const std::vector<uint64_t>& target) {
  return {material.program->generation,target,reinterpret_cast<uintptr_t>(material.bindings[0].captured.object.get()),
   reinterpret_cast<uintptr_t>(material.bindings[1].captured.object.get())};
 }
 bool Identity(RuntimeLease& actual) const {
  return &actual==lease&&actual.ContextIdentity()==context&&actual.Device()==device&&actual.SourceSubmission()==submission;
 }
 static bool Pair(const sharedpilot::MaterialContent& material) {
  return material.program&&material.program->generation&&material.bindings.size()>=2&&
   material.bindings[0].logicalSlot==0&&material.bindings[1].logicalSlot==1&&
   material.bindings[0].captured.knowledge==NativeCUSBindingKnowledge::BoundObject&&
   material.bindings[1].captured.knowledge==NativeCUSBindingKnowledge::BoundObject&&
   material.bindings[0].captured.object&&material.bindings[1].captured.object;
 }
public:
 struct Counts {uint64_t prepares=0,eligibleDraws=0,hits=0,misses=0,admissions=0,uncertified=0,omittedProviders=0,omittedRegistrations=0;} counts;
 explicit CUSSharedPreparationScope(RuntimeLease& actual,uint64_t originalContext):lease(&actual),context(actual.ContextIdentity()),device(actual.Device()),submission(actual.SourceSubmission()),sourceContext(originalContext) {
  actual.ValidatePreparationMutation();Require(submission!=0&&sourceContext!=0,"Shared CUS scope lacks actual source submission/context");
 }
 sharedpilot::ExecutionIdentity Execution(RuntimeLease& actual,const std::vector<uint64_t>& target) const {
  Require(Identity(actual),"Shared CUS scope crossed lease/context/submission");
  return {context,(__bridge const void*)device,lease,actual.WorldTexturePreparation().get(),submission,target};
 }
 bool Find(RuntimeLease& actual,const std::shared_ptr<const sharedpilot::MaterialContent>& material,
           const std::vector<uint64_t>& target,std::array<CUSSourceImage,2>& images) {
  actual.ValidatePreparationMutation();Require(Identity(actual),"Shared CUS lookup crossed actual lease");
  if(!material||!Pair(*material)||material->bindings[0].captured.object->contextGeneration!=sourceContext||material->bindings[1].captured.object->contextGeneration!=sourceContext)return false;
  ++counts.eligibleDraws;
  auto found=entries.find(MakeKey(*material,target));
  if(found==entries.end()){++counts.misses;return false;}
  const auto& e=found->second;
  if(e.slice.scope!=Execution(actual,target)||e.slice.material->program!=material->program){++counts.misses;return false;}
  for(unsigned slot=0;slot<2;++slot){
   const auto& current=material->bindings[slot].captured;
   if(!SameOwner(current.object,e.slice.material->bindings[slot].captured.object)||
      !e.images[slot].completedModelProof||!e.images[slot].completedModelProof->ValidFor(actual,current,e.images[slot])){
    ++counts.misses;return false;
   }
  }
  images=e.images;++counts.hits;counts.omittedProviders+=2;counts.omittedRegistrations+=4;return true;
 }
 // Called only after both original provider calls AND caller texture/sampler
 // registrations completed. A failure never publishes a partially pinned pair.
 void Admit(RuntimeLease& actual,const std::shared_ptr<const sharedpilot::MaterialContent>& material,
            const std::vector<uint64_t>& target,const std::array<CUSSourceImage,2>& images) {
  actual.ValidatePreparationMutation();Require(Identity(actual),"Shared CUS admission crossed actual lease");
  if(!material||!Pair(*material)||material->bindings[0].captured.object->contextGeneration!=sourceContext||material->bindings[1].captured.object->contextGeneration!=sourceContext)return;
  for(unsigned slot=0;slot<2;++slot)
   if(!images[slot].completedModelProof||!images[slot].completedModelProof->ValidFor(actual,material->bindings[slot].captured,images[slot])){++counts.uncertified;return;}
  if(entries.size()>=Capacity)return;
  Entry candidate;candidate.slice.scope=Execution(actual,target);candidate.slice.material=material;candidate.images=images;
  for(const auto& image:images)candidate.slice.owners.push_back(image.owner);
  const auto inserted=entries.emplace(MakeKey(*material,target),std::move(candidate));
  if(inserted.second)++counts.admissions;
 }
};
}
