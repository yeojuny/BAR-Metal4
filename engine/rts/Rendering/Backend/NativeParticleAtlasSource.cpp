/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeParticleAtlasSource.h"
#include "NativeTextureContext.h"
#include "NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include <exception>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
NativeTextureContext& Context(uint64_t generation=0){
 auto& c=OriginalNativeTextureContext();
 Check(c.Initialized()&&(!generation||c.Registry().ContextGeneration()==generation),"Original atlas texture crossed its actual context lifetime");return c;
}
void Mirror(NativeTextureContext& c,NativeCUSTextureTarget target,unsigned units){
 if(target!=NativeCUSTextureTarget::Image2D||!c.UIAttached())return;
 auto& r=ui::GetRecorder();Check(r.TextureObserver()==&c.UI(),"Original atlas and recorder have different texture histories");
 for(unsigned u=0;u<2;++u)if(units&(1u<<u))r.SyncObservedTextureBinding(u);
}
unsigned BoundUnits(NativeTextureContext& c,uint64_t name,NativeCUSTextureTarget target){
 unsigned units=0;for(unsigned u=0;u<2;++u){const auto b=c.History().Binding(u,target);if(b.object&&b.object->name==name)units|=1u<<u;}return units;
}
void Delete(NativeTextureContext& c,uint64_t name,NativeCUSTextureTarget target){
 const auto units=BoundUnits(c,name,target);
 // glDeleteTextures is immediate even in GL_COMPILE. Its mirror must not
 // append a later binding command to that list.
 if(c.Registry().Delete(name)&&!ui::GetRecorder().IsCompiling())Mirror(c,target,units);
}
struct SavedBinding {unsigned unit;NativeCUSTextureBinding prior;};
SavedBinding QueryBinding(NativeTextureContext& c,NativeCUSTextureTarget target){
 const auto active=c.History().ActiveUnit();Check(bool(active),"Original atlas upload has no observed active unit");
 auto prior=c.History().Binding(*active,target);
 Check(prior.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original atlas scoped binding has unobserved prior contents");
 return {*active,std::move(prior)};
}
// GL::TexBind(target,name) saves and restores this target on the current unit,
// including a real rebind of a saved name deleted during the scoped operation.
// Its glGet queries execute at list construction; its first Bind executes on
// the replay-time active unit, then its destructor selects the queried unit.
class ScopedBinding {
 NativeTextureContext& c;NativeCUSTextureTarget target;unsigned unit;
 NativeCUSTextureBinding prior;bool restored=false;
public:
 ScopedBinding(NativeTextureContext& c_,NativeCUSTextureTarget t,std::shared_ptr<const NativeCUSTextureObject> name,const SavedBinding& saved):c(c_),target(t),unit(saved.unit),prior(saved.prior){
  const auto active=c.History().ActiveUnit();Check(bool(active),"Original atlas upload has no observed active unit");
  const auto current=c.Registry().ResolveRecordedName(std::move(name));
  c.Registry().Bind(*active,current->name,target);Mirror(c,target,1u<<*active);
 }
 void Restore(){
  if(restored)return;
  c.History().ActiveTexture(unit);
  const auto object=prior.object?c.Registry().ResolveRecordedName(prior.object):nullptr;
  c.Registry().Bind(unit,object?object->name:0,target);restored=true;Mirror(c,target,1u<<unit);
 }
 ~ScopedBinding(){if(!restored)try{Restore();}catch(...){std::terminate();}}
};
}
NativeParticleAtlasTexture::~NativeParticleAtlasTexture(){
 if(!owned||!name)return;
 auto& c=OriginalNativeTextureContext();
 // A destroyed context has already deleted its whole native namespace.
 if(c.Initialized()&&c.Registry().ContextGeneration()==context)Delete(c,name,target);
}
void NativeParticleAtlasTexture::Bind()const{
 const auto generation=context,id=name;const auto kind=target;
 ui::GetRecorder().RecordOperation([generation,id,kind](ui::NativeUIRecorder& replay){
  if(replay.IsCompiling())return;
  auto& c=Context(generation);const auto unit=c.History().ActiveUnit();Check(bool(unit),"Original atlas Bind has no active texture unit");
  if(!c.Registry().Find(id))c.Registry().Create(kind,"CTextureAtlas retained name",id);
  c.Registry().Bind(*unit,id,kind);Mirror(c,kind,1u<<*unit);
 });
}
void NativeParticleAtlasTexture::Unbind()const{
 const auto generation=context;const auto kind=target;
 ui::GetRecorder().RecordOperation([generation,kind](ui::NativeUIRecorder& replay){
  if(replay.IsCompiling())return;
  auto& c=Context(generation);const auto unit=c.History().ActiveUnit();Check(bool(unit),"Original atlas Unbind has no active texture unit");
  c.Registry().Bind(*unit,0,kind);Mirror(c,kind,1u<<*unit);
 });
}
std::shared_ptr<const NativeWorldParticleAtlas> NativeParticleAtlasTexture::Source()const{
 auto& c=Context(context);const auto current=c.Registry().Find(name);
 if(!current||current->target!=target||current->storage!=NativeTextureStorageKnowledge::Published)return {};
 const auto backing=std::dynamic_pointer_cast<const NativeParticleAtlasBacking>(current->backing);
 return backing?backing->atlas:nullptr;
}
bool PublishNativeParticleAtlas(std::shared_ptr<NativeParticleAtlasTexture>& texture,
 std::shared_ptr<const NativeWorldParticleAtlas> atlas,uint64_t frame){
 auto& c=Context();auto& registry=c.Registry();
 Check(atlas&&!atlas->pages.empty()&&atlas->pages.size()<=16&&atlas->pages.front().size()==1&&atlas->requestedMipLevels,"Original atlas upload lost its packed pages or requested levels");
 const auto target=atlas->pages.size()>1?NativeCUSTextureTarget::Image2DArray:NativeCUSTextureTarget::Image2D;
 Check(!texture||(texture->context==registry.ContextGeneration()&&texture->target==target),"Original atlas target-changing reuse requires its invalid-target source semantics");
 const auto name=texture?texture->name:registry.Create(target,"CTextureAtlas: "+atlas->sourceName)->name;
 Check(name<=std::numeric_limits<uint32_t>::max(),"Original atlas logical texture namespace overflows");
 auto recordedName=registry.Find(name);
 // A retained wrapper can name a deleted object. Keep its numeric name for
 // the deferred Bind without recreating live storage during compilation.
 if(!recordedName){auto token=std::make_shared<NativeCUSTextureObject>();token->name=name;token->contextGeneration=registry.ContextGeneration();token->target=target;token->source="CTextureAtlas: "+atlas->sourceName;recordedName=std::move(token);}
 const auto& base=atlas->pages.front().front();
 Check(base.width&&base.height,"Original atlas upload has no actual base extent");
 for(const auto& page:atlas->pages)Check(page.size()==1&&page.front().width==base.width&&page.front().height==base.height&&page.front().rgba.size()==size_t(base.width)*base.height*4,"Original atlas packed page dimensions differ");
 const NativeTextureAllocation allocation{base.width,base.height,uint32_t(atlas->pages.size()),atlas->requestedMipLevels,1,0x8058};
 const auto generation=registry.ContextGeneration();
 const auto constructorRestore=QueryBinding(c,target);
 ui::GetRecorder().RecordOperation([generation,target,recordedName,allocation,constructorRestore](ui::NativeUIRecorder& replay){
  if(replay.IsCompiling())return;
  auto& actual=Context(generation);auto& names=actual.Registry();
  ScopedBinding binding(actual,target,recordedName,constructorRestore);
  auto sampling=names.Find(recordedName->name)->sampling;
  // Exact TextureCreationParams used by CTextureAtlas and InitTexture.
  sampling.magFilter=0x2601;sampling.minFilter=allocation.levels==1?0x2601:0x2703;
  sampling.wrapS=sampling.wrapT=sampling.wrapR=0x812F;
  names.SetSampling(recordedName->name,sampling);Mirror(actual,target,BoundUnits(actual,recordedName->name,target));
  names.Allocate(recordedName->name,allocation);
  sampling.baseLevel=0;sampling.maxLevel=int(allocation.levels)-1;
  names.SetSampling(recordedName->name,sampling);Mirror(actual,target,BoundUnits(actual,recordedName->name,target));binding.Restore();
 });
 // Assignment destroys the previous GL::TextureBase after the new wrapper
 // constructor reused its name. Preserve that deletion, including bound aliases.
 if(texture&&texture->owned)Delete(c,name,target);
 if(texture)texture->owned=false;
 auto next=std::make_shared<NativeParticleAtlasTexture>();next->context=registry.ContextGeneration();next->name=name;next->target=target;texture=std::move(next);
 const auto uploadRestore=QueryBinding(c,target);
 const bool compiled=ui::GetRecorder().IsCompiling();
 ui::GetRecorder().RecordOperation([generation,target,recordedName,allocation,uploadRestore,atlas,frame,compiled](ui::NativeUIRecorder& replay){
  if(replay.IsCompiling())return;
  auto& actual=Context(generation);auto& names=actual.Registry();
  ScopedBinding binding(actual,target,recordedName,uploadRestore);
  const auto current=names.Find(recordedName->name);
  // Immediate owned-name reload deletes the just-constructed storage. In a
  // compiled list deletion happened BEFORE the deferred constructor, so the
  // replayed allocation is valid. Decide from that actual object, not from
  // whether a wrapper happened to be deleted while the list was constructed.
  if(current->storage!=NativeTextureStorageKnowledge::Empty){
   Check(current->allocation==allocation,"Original atlas upload differs from the actual bound allocation");
   Check(!compiled||globalRendering,"Original atlas replay lost its actual draw-frame owner");
   auto backing=std::make_shared<NativeParticleAtlasBacking>();
   backing->storageGeneration=current->identity;backing->contentVersion=current->version+1;
   backing->sourceDrawFrame=compiled?uint64_t(globalRendering->drawFrame):frame;
   backing->content=NativeTextureContent::ProducerVersion;backing->atlas=atlas;
   names.PublishContent(recordedName->name,{},std::move(backing));Mirror(actual,target,BoundUnits(actual,recordedName->name,target));
  }
  binding.Restore();
 });
 return !compiled&&bool(texture->Source());
}
}
