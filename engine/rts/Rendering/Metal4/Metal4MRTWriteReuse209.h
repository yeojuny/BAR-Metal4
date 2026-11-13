/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4MRTFrontier209.h"
#include <limits>
#include <cstdlib>
#include <new>
#include <optional>
#include <utility>

namespace rendering::metal4 {
inline bool MRTLuaWriteReuseInterventionEnabled() {
 static const bool enabled=[] {
  const char* value=std::getenv("BAR_NATIVE_INTERVENTION_MRT_LUA_REUSE");
  return value&&value[0]=='1'&&value[1]=='\0';
 }();
 return enabled;
}
// A narrowly scoped permission, NOT a general shared_ptr last-reader test.
// The only production issuer is the sealed normal-main opaque Prepare path,
// before either fresh MRT has a native Resource. All later samplers in that
// path use already captured strong object/backing bindings. An ancestor held
// only for encode/retirement does not manufacture another source sampler.
class MRTWriteReuse209 {
public:
 MRTWriteReuse209(const MRTWriteReuse209&)=delete;
 MRTWriteReuse209& operator=(const MRTWriteReuse209&)=delete;
 MRTWriteReuse209(MRTWriteReuse209&& other) noexcept:
  unit(std::exchange(other.unit,nullptr)),feature(std::exchange(other.feature,nullptr)),
  frame(other.frame),submission(other.submission),used(std::exchange(other.used,true)) {}

 static std::optional<MRTWriteReuse209> Try(const NativeOpaqueModelFrame& source,
  uint64_t actualFrame,uint64_t actualSubmission,const MRTFrontier209* sealedMainRegistry) {
  if(!sealedMainRegistry||!actualFrame||!actualSubmission||source.sourceDrawFrame!=actualFrame||
   source.alphaStage||source.drawReflection||source.drawRefraction||source.passes.size()!=4)return {};
  for(size_t n=0;n<4;++n){const auto& p=source.passes[n];
   if(!p.sequence||p.sequence>=source.endSequence||(n&&source.passes[n-1].sequence>=p.sequence)||
    p.feature!=(n>=2)||p.deferred!=(n%2==0)||!p.engine||!p.lua||
    p.engine->sourceDrawFrame!=actualFrame||p.lua->sourceDrawFrame!=actualFrame||
    bool(p.mrt)!=(n%2==0))return {};
  }
  const auto& up=source.passes[0];const auto& fp=source.passes[2];
  // Empty feature frames still take readonly164. The intervention also
  // admits Lua-only writing features, but NONE of the sealed registry,
  // exclusive-owner, exact publication/version or view proofs below change.
  // A source sampler of the U publication adds an owner and rejects reuse.
  const bool engineWrites=!fp.engine->beforeLua.empty()||!fp.engine->afterLua.empty();
  const bool luaWrites=MRTLuaWriteReuseInterventionEnabled()&&fp.lua&&!fp.lua->draws.empty();
  if(!engineWrites&&!luaWrites)return {};
  if(!up.FullInitialClear()||fp.initialClear)return {};
  const auto& u=*up.mrt;const auto& f=*fp.mrt;
  if(&u==&f||u.role!=NativeGeometryTextureRole::Models||f.role!=u.role||u.feature||!f.feature||
   !u.clearBefore||f.clearBefore||u.clearOnly||f.clearOnly||
   u.combinedColor.Changed()||f.combinedColor.Changed()||
   u.sourceDrawFrame!=actualFrame||f.sourceDrawFrame!=actualFrame||
   !u.depth.backing||!f.depth.backing||!u.lua||!f.lua)return {};
  const auto& view=u.depth.backing->Metadata();
  if(!view.width||!view.height||!SameView(view,f.depth.backing->Metadata())||
   u.camera.view!=f.camera.view||u.camera.projection!=f.camera.projection||
   u.camera.viewport!=f.camera.viewport||u.camera.clipDepth!=f.camera.clipDepth)return {};
  for(unsigned n=0;n<5;++n){
   const auto& a=u.colors[n];const auto& b=f.colors[n];
   if(!a.backing||!b.backing||a.backing->Metadata().role!=NativeGeometryTextureRole::Models||
    b.backing->Metadata().role!=NativeGeometryTextureRole::Models||
    a.backing->Metadata().attachment!=n||b.backing->Metadata().attachment!=n||
    !SameView(view,a.backing->Metadata().view)||!SameView(view,b.backing->Metadata().view)||
    !ExclusiveFreshPair(a.previous,a.current,a.backing,b.previous,b.current,b.backing,actualFrame,view,
     sealedMainRegistry->context,sealedMainRegistry->current[n],false))return {};
  }
  if(u.depth.backing->Kind()!=NativeTextureBackingKind::ModelDepth||
   f.depth.backing->Kind()!=NativeTextureBackingKind::ModelDepth||
   !ExclusiveFreshPair(u.depth.previous,u.depth.published,u.depth.backing,
    f.depth.previous,f.depth.published,f.depth.backing,actualFrame,view,sealedMainRegistry->context,sealedMainRegistry->current[5],true))return {};
  return MRTWriteReuse209(u,f,actualFrame,actualSubmission);
 }

 // Consumed only after normal source validation and physical owner checks.
 // No pointer to this stack permission is stored in a prepared GPU owner.
 bool Consume(const NativeCUSDeferredPhase& selected,uint64_t actualSubmission) noexcept {
  if(used||!unit||feature!=&selected||selected.sourceDrawFrame!=frame||actualSubmission!=submission)return false;
  used=true;return true;
 }
 const NativeCUSDeferredPhase* Unit()const noexcept{return unit;}
private:
 const NativeCUSDeferredPhase* unit=nullptr;
 const NativeCUSDeferredPhase* feature=nullptr;
 uint64_t frame=0,submission=0;
 bool used=false;
 MRTWriteReuse209(const NativeCUSDeferredPhase& u,const NativeCUSDeferredPhase& f,uint64_t draw,uint64_t submit):
  unit(&u),feature(&f),frame(draw),submission(submit) {}
 template<class A,class B> static bool SameOwner(const A& a,const B& b) noexcept {
  return a&&b&&a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);
 }
 static bool SameView(const NativeShieldDepthMetadata& a,const NativeShieldDepthMetadata& b) noexcept {
  return a.width==b.width&&a.height==b.height&&a.view==b.view&&a.projection==b.projection&&a.viewport==b.viewport;
 }
 static bool SameLogicalAllocation(const NativeCUSTextureObject& a,const NativeCUSTextureObject& b) noexcept {
  return a.identity&&a.name&&a.contextGeneration&&a.identity==b.identity&&a.name==b.name&&
   a.contextGeneration==b.contextGeneration&&a.target==b.target&&a.allocation==b.allocation;
 }
 template<class Backing> static bool ExclusiveFreshPair(
  const std::shared_ptr<const NativeCUSTextureObject>& preU,
  const std::shared_ptr<const NativeCUSTextureObject>& u,const std::shared_ptr<const Backing>& ub,
  const std::shared_ptr<const NativeCUSTextureObject>& fPrevious,
  const std::shared_ptr<const NativeCUSTextureObject>& f,const std::shared_ptr<const Backing>& fb,
  uint64_t draw,const NativeShieldDepthMetadata& view,uint64_t context,const std::shared_ptr<const NativeCUSTextureObject>& current,bool depth) {
  if(!preU||!u||!ub||!f||!fb||preU==u||u==f||ub==fb||
   !SameOwner(u,fPrevious)||!SameOwner(u->backing,ub)||!SameOwner(f->backing,fb)||
   !SameLogicalAllocation(*preU,*u)||!SameLogicalAllocation(*u,*f)||
   preU->backing.get()==ub.get()||context!=u->contextGeneration||
   u->image||f->image||u->target!=NativeCUSTextureTarget::Image2D||
   u->storage!=NativeTextureStorageKnowledge::Published||f->storage!=NativeTextureStorageKnowledge::Published||
   ub->content!=NativeTextureContent::ProducerVersion||fb->content!=NativeTextureContent::ProducerVersion||
   ub->sourceDrawFrame!=draw||fb->sourceDrawFrame!=draw||
   ub->storageGeneration!=u->identity||fb->storageGeneration!=f->identity||
   ub->contentVersion!=u->version||fb->contentVersion!=f->version||f->version<=u->version||
   ub->Resource()||fb->Resource())return false;
  const auto& allocation=u->allocation;
  if(allocation.width!=view.width||allocation.height!=view.height||allocation.depth!=1||
   allocation.samples!=1||allocation.levels!=1||
   (depth?allocation.originalInternalFormat!=0x8CAC:
    (allocation.originalInternalFormat!=0x1908&&allocation.originalInternalFormat!=0x8058)))return false;
  // Exactly two validated MEMBER instances own each U publication:
  // object: U.current/published and F.previous;
  // backing: U.backing and U.current/published->backing.
  // A sampler-state clone can pass the object test but fails the backing test.
  constexpr long structuralObjectMembers=2,structuralBackingMembers=2;
  if(u.use_count()!=structuralObjectMembers||ub.use_count()!=structuralBackingMembers)return false;
  {
   // Normal sealed Prepare has no later source writer. If a downstream
   // receipt transfer fails, reverse F->U->preU cancellation must have two
   // version increments available, with all original matching preconditions.
   return current&&SameLogicalAllocation(*current,*f)&&SameOwner(current->backing,fb)&&
    current->image==f->image&&current->storage==f->storage&&current->version>=f->version&&
    current->version<=std::numeric_limits<uint64_t>::max()-2;
  }
 }
};
} // namespace rendering::metal4
