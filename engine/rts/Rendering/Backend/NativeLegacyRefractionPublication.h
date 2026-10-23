/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include <stdexcept>
#include <limits>
namespace rendering {
// Real CRefractWater::subSurfaceTex contents. Allocation is RGB and one level;
// each CopyTexSubImage creates a retained source version, not a global scratch image.
struct NativeLegacyRefractionResource {
 virtual ~NativeLegacyRefractionResource()=default;
 virtual bool Completed()const noexcept=0;
};
struct NativeLegacyRefractionBacking final:NativeTextureBacking {
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
 void Fulfill(std::shared_ptr<const NativeLegacyRefractionResource> r)const{
  if(!r||resource)throw std::logic_error("Refractive source image fulfilled twice or without owner");
  resource=std::move(r);
 }
 const auto& Resource()const{return resource;}
private:mutable std::shared_ptr<const NativeLegacyRefractionResource> resource;
};
class NativeLegacyRefractionPublication {
 NativeTextureRegistry* registry=nullptr;NativeCUSTextureHistory* history=nullptr;
 uint64_t context=0,name=0,revision=0;
 std::shared_ptr<const NativeCUSTextureObject> prior,pending;
 void Check()const{if(!registry||!name||registry->ContextGeneration()!=context)throw std::logic_error("Refractive source texture context changed");}
public:
 void Create(NativeTextureRegistry& r,NativeCUSTextureHistory& h,NativeCUSTextureTarget target,uint32_t width,uint32_t height){
  if(name||!width||!height||(target!=NativeCUSTextureTarget::Rectangle&&target!=NativeCUSTextureTarget::Image2D))throw std::logic_error("Invalid original refractive allocation");
  auto active=h.ActiveUnit();if(!active)throw std::logic_error("Refractive LoadGfx active unit unobserved");
  registry=&r;history=&h;context=r.ContextGeneration();auto image=r.Create(target,"CRefractWater::subSurfaceTex");name=image->name;
  try{r.Bind(*active,name,target);auto sampling=r.Find(name)->sampling;sampling.minFilter=sampling.magFilter=0x2600;r.SetSampling(name,sampling);
   r.Allocate(name,{width,height,1,1,1,3});
  }catch(...){r.Delete(name);name=0;throw;}
 }
 std::shared_ptr<const NativeCUSTextureObject> Copy(uint64_t frame){
  Check();AbortUncompleted();if(revision==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Refractive source copy revision overflows");prior=registry->Find(name);
  auto b=std::make_shared<NativeLegacyRefractionBacking>();b->storageGeneration=prior->identity;b->contentVersion=++revision;b->sourceDrawFrame=frame;b->content=NativeTextureContent::ProducerVersion;
  history->ActiveTexture(2);registry->Bind(2,name,prior->target);history->Enable(2,prior->target,true);
  registry->PublishContent(name,{},b);pending=registry->Find(name);return pending;
 }
 void DrawTail(){Check();history->ActiveTexture(2);history->Enable(2,registry->Find(name)->target,false);history->ActiveTexture(0);}
 // Called at every source submission exit, including skipped/failed submissions.
 // Successful immutable GPU contents remain published; aborted writes never leak
 // into a later frame's inherited binding. Saved draw snapshots are not rewritten.
 void AbortUncompleted()noexcept{
  if(!pending)return;
  auto b=std::dynamic_pointer_cast<const NativeLegacyRefractionBacking>(pending->backing);
  if(registry&&registry->ContextGeneration()==context&&(!b||!b->Resource()||!b->Resource()->Completed()))
   registry->CancelContentPublication(pending,prior);
  pending.reset();prior.reset();
 }
 void Destroy(){
  if(!name)return;
  Check();AbortUncompleted();registry->Delete(name);name=0;registry=nullptr;history=nullptr;context=revision=0;
 }
};
}
