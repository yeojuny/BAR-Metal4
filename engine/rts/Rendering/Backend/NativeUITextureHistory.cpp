/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeUITextureHistory.h"
#include "NativeUIRecorder.h"
#include "NativePostprocessRecorder.h"
#include "NativeUITextureStorage.h"
#include "NativeMapTexturePublication.h"
#include "NativeSMFTextureData.h"
#include "../EngineBridge/NativeTerrainImagePublication.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
namespace rendering::ui {
namespace{
void Check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
constexpr auto target=NativeCUSTextureTarget::Image2D;
const NativeUITextureBinding* Root(const std::shared_ptr<NativeUITextureBinding>& binding){
 Check(bool(binding),"Original UI texture binding is absent");auto*p=binding.get();
 std::array<const NativeUITextureBinding*,64> seen{};unsigned count=0;
 while(p->source){Check(count<seen.size()&&std::find(seen.begin(),seen.begin()+count,p)==seen.begin()+count,"Original UI texture delegation is cyclic");seen[count++]=p;p=p->source.get();}return p;
}
}
NativeTextureBackingKind NativeScreenTextureBacking::Kind()const noexcept{
 return image&&(image->format==ScreenTextureFormat::Depth32Float||image->format==ScreenTextureFormat::Depth24Unorm||image->format==ScreenTextureFormat::Depth16Unorm)?NativeTextureBackingKind::Other:NativeTextureBackingKind::ColorImage;
}
NativeUITextureHistory::NativeUITextureHistory(NativeCUSTextureHistory&h,NativeTextureRegistry&r,std::function<uint64_t()>frame):history(h),registry(r),context(r.ContextGeneration()),sourceFrame(std::move(frame)){Check(r.MatchesHistory(h)&&context&&bool(sourceFrame),"UI texture observer lacks actual shared context");}
NativeUITextureHistory::~NativeUITextureHistory()=default;
void NativeUITextureHistory::CheckContext()const{Check(context==registry.ContextGeneration(),"UI texture observer was not reset with its actual context");}
void NativeUITextureHistory::Reset(){publicationLifetime=std::make_shared<const int>(0);bindings.clear();named.clear();pendingNames.clear();context=registry.ContextGeneration();}
NativeTextureSampling NativeUITextureHistory::Sampling(ScreenSampler s){
 NativeTextureSampling p;
 p.minFilter=s.mipFilter==ScreenMipFilter::None?(s.filter==ScreenFilter::Nearest?0x2600:0x2601):s.mipFilter==ScreenMipFilter::Nearest?(s.filter==ScreenFilter::Nearest?0x2700:0x2701):(s.filter==ScreenFilter::Nearest?0x2702:0x2703);
 p.magFilter=s.magnificationFilter.value_or(s.filter)==ScreenFilter::Nearest?0x2600:0x2601;
 auto wrap=[](ScreenWrap v){switch(v){case ScreenWrap::Repeat:return 0x2901u;case ScreenWrap::ClampEdge:return 0x812Fu;case ScreenWrap::TransparentBorder:return 0x812Du;case ScreenWrap::LegacyClamp:return 0x2900u;}throw std::runtime_error("Original image wrapping state unavailable");};
 p.wrapS=wrap(s.wrapS);p.wrapT=wrap(s.wrapT);p.anisotropy=s.anisotropy;if(s.whiteBorder)p.borderColor={1,1,1,1};return p;
}
NativeTextureAllocation NativeUITextureHistory::Allocation(const ScreenTexture&i,unsigned format){
 Check(i.format!=ScreenTextureFormat::ExternalColor,"External color retains an actual source object, not a new UI allocation");
 if(!format)switch(i.format){case ScreenTextureFormat::R8Unorm:format=0x8229;break;case ScreenTextureFormat::RG8Unorm:format=0x822B;break;case ScreenTextureFormat::RGBA8Unorm:case ScreenTextureFormat::BGRA8Unorm:format=0x8058;break;case ScreenTextureFormat::BC1RGBA:format=0x83F1;break;case ScreenTextureFormat::BC2RGBA:format=0x83F2;break;case ScreenTextureFormat::BC3RGBA:format=0x83F3;break;case ScreenTextureFormat::BC4Unorm:format=0x8DBB;break;case ScreenTextureFormat::BC5Unorm:format=0x8DBD;break;case ScreenTextureFormat::BC7RGBA:format=0x8E8C;break;case ScreenTextureFormat::BC7RGBASRGB:format=0x8E8D;break;case ScreenTextureFormat::RGBA16Float:format=0x881A;break;case ScreenTextureFormat::RG16Float:format=0x822F;break;case ScreenTextureFormat::RG11B10Float:format=0x8C3A;break;case ScreenTextureFormat::R16Float:format=0x822D;break;case ScreenTextureFormat::R32Float:format=0x822E;break;case ScreenTextureFormat::Depth32Float:format=0x81A7;break;case ScreenTextureFormat::Depth24Unorm:format=0x81A6;break;case ScreenTextureFormat::Depth16Unorm:format=0x81A5;break;case ScreenTextureFormat::ExternalColor:throw std::runtime_error("External color cannot allocate UI storage");}
 unsigned levels=i.mips.empty()?1:i.mips.size();if(i.generateMipmaps)levels=1+unsigned(std::floor(std::log2(std::max(i.width,i.height))));
 return {i.width,i.height,1,levels,1,format};
}
NativeUITextureHistory::Entry& NativeUITextureHistory::Find(const std::shared_ptr<NativeUITextureBinding>&b){CheckContext();auto it=bindings.find(Root(b));Check(it!=bindings.end(),"UI image producer did not register its actual logical texture");return it->second;}
void NativeUITextureHistory::PublishImage(Entry&e,std::shared_ptr<const ScreenTexture> image,ScreenSampler sampler){
 Check(bool(image),"UI image publication lacks actual source storage");auto object=registry.Find(e.name);Check(bool(object),"UI publication refers to a deleted source texture");
 if(e.image==image||(e.image&&e.image->postprocessVersion&&e.image->postprocessVersion==image->postprocessVersion)){e.image=std::move(image);e.sampler=sampler;return;}
 if(!image->incomplete&&(!image->bytes.empty()||!image->mips.empty()||(image->renderTarget&&image->generation)||image->postprocessVersion)){
  auto backing=std::make_shared<NativeScreenTextureBacking>();backing->image=image;backing->storageGeneration=object->identity;backing->contentVersion=object->version+1;backing->content=NativeTextureContent::ProducerVersion;
  backing->sourceDrawFrame=sourceFrame(); // Captured at the actual producer, never at a consumer.
  registry.PublishContent(e.name,{},std::move(backing));
 }
 e.image=std::move(image);e.sampler=sampler;
}
void NativeUITextureHistory::Create(const std::shared_ptr<NativeUITextureBinding>&b,const std::string&source,unsigned format){
 CheckContext();Check(b&&b->current&&!bindings.contains(b.get()),"Original Lua image creation owner differs");auto o=registry.Create(target,source);Entry e{o->name,b,{},{},false};
 // Original LuaTextures::Create temporarily binds and restores the inherited
 // active unit; it does not leave the new image bound.
 auto unit=history.ActiveUnit();Check(bool(unit),"Lua image creation lacks actual active unit");auto prior=history.Binding(*unit,target);registry.Bind(*unit,o->name,target);registry.Allocate(o->name,Allocation(*b->current,format));registry.SetSampling(o->name,Sampling(b->sampler));
 if(prior.object)registry.Bind(*unit,prior.object->name,target);else{Check(prior.knowledge==NativeCUSBindingKnowledge::DefaultObject,"Original Lua image prior binding unobserved");registry.Bind(*unit,0,target);}
 e.declared=registry.Find(e.name);e.ownsStorage=true;bindings.emplace(b.get(),std::move(e));
 // Allocation with nullptr data is not an image upload or implicit clear.
}
void NativeUITextureHistory::RegisterExternal(const std::shared_ptr<NativeUITextureBinding>&b,uint64_t name){CheckContext();Check(b&&!bindings.contains(b.get())&&(!name||registry.Find(name)),"External UI source texture identity differs");bindings.emplace(b.get(),Entry{name,b,{},{},false,false,registry.Find(name)});}
std::shared_ptr<const NativeCUSTextureObject> NativeUITextureHistory::ResolveObservedName(uint64_t name)
{
 CheckContext();Check(name!=0,"Original borrowed UI texture has no observed logical name");
 const auto resolve=[&](const Entry& entry){
  auto current=registry.Find(entry.name);
  Check(bool(current),"Original borrowed UI texture was deleted; old pixels cannot be republished");
  return registry.Resolve(std::move(current));
 };
 for(const auto&[binding,entry]:bindings)if(entry.name==name)return resolve(entry);
 for(const auto&[source,entry]:named)if(entry.name==name)return resolve(entry);
 throw std::runtime_error("Original borrowed texture name was not observed by the actual UI/named owner");
}
void NativeUITextureHistory::PublishBorrowedSquare(const NativeTerrainBorrowedSquareWrite& write)
{
 CheckContext();
 const auto before=registry.Resolve(write.originalTarget);
 Check(before==write.originalTarget&&write.contextGeneration==context&&write.originalObjectGeneration==before->identity&&
  before->target==target&&write.originalDestLevel==0&&write.originalInternalFormat==0x83F1&&write.sourceMip<4&&
  write.width==(NATIVE_SMF_SQUARE_TEXELS>>write.sourceMip)&&write.height==write.width&&write.tiles,
  "Borrowed square TexImage differs from its actual destination/source event");
 const auto unit=history.ActiveUnit();Check(bool(unit),"Borrowed square writer has no original active unit");
 const auto bound=history.Binding(*unit,target);
 Check(bound.object&&bound.object->identity==before->identity,"Borrowed square destination is no longer bound");
 auto found=std::find_if(bindings.begin(),bindings.end(),[&](const auto& item){
  return item.second.name==before->name&&item.second.ownsStorage;
 });
 Check(found!=bindings.end()||write.recordedCommand,"Borrowed square destination is not an existing Lua/UI storage owner");
 Entry* entry=found!=bindings.end()?&found->second:nullptr;
 const auto cell=entry?entry->binding:nullptr;
 Check(!entry||(cell&&!cell->source&&cell->current),"Borrowed square destination has no original live cell");
 Check(before->version!=std::numeric_limits<uint64_t>::max(),"Borrowed square registry version exhausted");
 const bool sameLifetime=entry&&entry->declared&&entry->declared->contextGeneration==context&&entry->declared->identity==before->identity;

 // Resolve the actual old storage, independently of its current sampler's
 // completeness. Retained commands keep their old immutable owners.
 std::shared_ptr<const ScreenTexture> previous=sameLifetime?cell->current:nullptr;
 if(auto old=std::dynamic_pointer_cast<const NativeScreenTextureBacking>(before->backing))previous=old->image;
 if(cell&&cell->postprocess){
  Check(!cell->postprocess->deleted&&cell->postprocess->image&&cell->postprocess->nextGeneration&&
   cell->postprocess->nextGeneration!=std::numeric_limits<uint64_t>::max(),"Borrowed shared destination is deleted/exhausted");
  if(!previous&&sameLifetime&&cell->postprocess->current&&cell->postprocess->current->sourceImage)previous=cell->postprocess->current->sourceImage;
 }
 Check(previous||before->storage==NativeTextureStorageKnowledge::Empty||before->storage==NativeTextureStorageKnowledge::AllocatedUndefined,
  "Borrowed square previous level definitions are absent");
 auto definitions=std::make_shared<ScreenTextureLevelDefinitions>();
 definitions->contextGeneration=context;definitions->objectGeneration=before->identity;
 if(previous&&previous->levelDefinitions){
  Check(previous->levelDefinitions->contextGeneration==context&&previous->levelDefinitions->objectGeneration==before->identity,
   "Borrowed square retained levels crossed a texture lifetime");
  definitions->levels=previous->levelDefinitions->levels;
 }else{
  unsigned width=before->allocation.width,height=before->allocation.height;
  for(unsigned level=0;level<before->allocation.levels;++level){
   ScreenTextureLevelDefinition definition;
   definition.width=width;definition.height=height;definition.originalInternalFormat=before->allocation.originalInternalFormat;
   definition.defined=true;definition.owner=previous;definition.ownerLevel=level;
   definition.allocatedUndefined=before->storage==NativeTextureStorageKnowledge::AllocatedUndefined||
    (previous&&level<previous->mips.size()&&previous->mips[level].allocatedUndefined);
   definitions->levels.push_back(std::move(definition));
   width=width>1?width/2:width;height=height>1?height/2:height;
  }
 }
 Check(definitions->levels.size()==before->allocation.levels,"Borrowed destination lost an earlier mip definition");
 definitions->levels.resize(std::max(before->allocation.levels,1u));

 // Consume the exact input retained when TexImage was issued/compiled. List
 // replay must not re-read source tiles or resurrect a prior destination.
 Check(write.sourcePayload&&write.sourcePayload->width==write.width&&write.sourcePayload->height==write.height&&
  write.sourcePayload->bytesPerRow==size_t(write.width/4)*8&&
  write.sourcePayload->bytes.size()==write.sourcePayload->bytesPerRow*(write.height/4),
  "Borrowed square command lacks its captured original compressed input");
 const auto& selected=*write.sourcePayload;
 auto payload=std::make_shared<ScreenTexture>();
 payload->sourceName=before->source;
 payload->generation=cell&&cell->postprocess?cell->postprocess->nextGeneration:before->version+1;
 payload->format=ScreenTextureFormat::BC1RGBA;payload->width=write.width;payload->height=write.height;
 ScreenTextureMip base;base.width=selected.width;base.height=selected.height;base.bytesPerRow=selected.bytesPerRow;base.bytes=selected.bytes;
 payload->mips.push_back(std::move(base)); // Original compressed upload: no framebuffer UV flip.
 ScreenTextureLevelDefinition replacement;
 replacement.width=write.width;replacement.height=write.height;replacement.originalInternalFormat=write.originalInternalFormat;
 replacement.defined=true;replacement.owner=payload;replacement.terrainTiles=write.tiles;
 replacement.squareX=write.squareX;replacement.squareZ=write.squareZ;replacement.sourceMip=write.sourceMip;
 definitions->levels[write.originalDestLevel]=std::move(replacement);

 auto image=std::make_shared<ScreenTexture>(*payload);image->levelDefinitions=definitions;
 unsigned width=write.width,height=write.height;
 for(unsigned level=1;level<definitions->levels.size()&&(width>1||height>1);++level){
  width=std::max(1u,width/2);height=std::max(1u,height/2);const auto& definition=definitions->levels[level];
  if(!definition.defined||definition.width!=width||definition.height!=height||definition.originalInternalFormat!=write.originalInternalFormat)break;
  if(definition.allocatedUndefined){
   ScreenTextureMip mip;mip.width=width;mip.height=height;mip.bytesPerRow=size_t((width+3)/4)*8;mip.allocatedUndefined=true;
   image->mips.push_back(std::move(mip));continue;
  }
  Check(definition.owner&&definition.owner->format==ScreenTextureFormat::BC1RGBA&&definition.ownerLevel<definition.owner->mips.size(),
   "Retained compressed level has no original immutable CPU upload owner");
  image->mips.push_back(definition.owner->mips[definition.ownerLevel]);
 }

 std::shared_ptr<NativePostprocessImage> sharedImage;
 std::shared_ptr<NativePostprocessVersion> sharedVersion;
 if(cell&&cell->postprocess){
  sharedImage=std::make_shared<NativePostprocessImage>(*cell->postprocess->image);
  if(!sameLifetime){
   auto fresh=CreateNativePostprocessBinding(write.width,write.height,NativePostprocessFormat::BC1RGBA,
    sharedImage->sampler,sharedImage->renderToTexture,16384);
   sharedImage->object=fresh->image->object;
  }
  sharedImage->width=write.width;sharedImage->height=write.height;sharedImage->format=NativePostprocessFormat::BC1RGBA;
  sharedVersion=std::make_shared<NativePostprocessVersion>();
  sharedVersion->image=sharedImage;sharedVersion->generation=payload->generation;sharedVersion->sourceImage=image;
  Check(HasNativePostprocessCPUContents(*sharedVersion),"Borrowed compressed shared image has no CPU source");
 }
 auto backing=std::make_shared<NativeScreenTextureBacking>();backing->image=image;
 backing->storageGeneration=before->identity;backing->contentVersion=before->version+1;
 backing->content=NativeTextureContent::ProducerVersion;backing->sourceDrawFrame=write.sourceDrawFrame;
 auto allocation=before->allocation;allocation.width=write.width;allocation.height=write.height;allocation.depth=1;
 allocation.samples=1;allocation.levels=definitions->levels.size();allocation.originalInternalFormat=write.originalInternalFormat;

 // Strong publication: all allocation/validation is complete before this
 // single original TexImage event. Every assignment below is nonthrowing.
 auto published=registry.PublishRedefinedContent(before,allocation,std::move(backing));
 if(cell)cell->current=image;
 if(cell&&cell->postprocess){
  cell->postprocess->image=std::move(sharedImage);cell->postprocess->current=std::move(sharedVersion);
  ++cell->postprocess->nextGeneration;
 }
 if(entry){entry->image=std::move(image);entry->declared=std::move(published);}
 // A deleted Lua owner stays deleted. Replaying its recorded bind/TexImage
 // can create real registry storage without restoring any deleted live cell.
 // No sampler/active-unit/bind writer, no list command or GPU retirement.
}
NativeTextureLookup NativeUITextureHistory::LookupObservedNamed(const std::string& name) const
{
 CheckContext();const auto it=named.find(name);
 if(it==named.end())return {};
 const auto& entry=it->second;
 if(!entry.name)return {true,{},false}; // Only a genuinely recorded ID0 is observed0.
 auto current=registry.Find(entry.name);
 Check(bool(current),"Original named map texture was deleted; old pixels cannot be republished");
 return {true,registry.Resolve(std::move(current)),false};
}
uint64_t NativeUITextureHistory::UploadBuildPicture(const std::shared_ptr<NativeUITextureBinding>&b,const std::string&source,bool flatDDS)
{
 CheckContext();Check(b&&b->current&&!b->source&&!bindings.contains(b.get()),"Original build picture load owner differs");
 if(b->current->incomplete){RegisterExternal(b,0);return 0;} // Original failed DDS returns texture ID0 without a bind.
 auto object=registry.Create(target,source);Entry e{object->name,b,{},{},false,flatDDS};
 const auto unit=history.ActiveUnit();Check(bool(unit),"Original build picture upload lacks current texture unit");const auto prior=history.Binding(*unit,target);
 if(flatDDS){history.PushAttributes(true,true);history.Enable(*unit,target,true);}
 registry.Bind(*unit,e.name,target);registry.Allocate(e.name,Allocation(*b->current));registry.SetSampling(e.name,Sampling(b->sampler));PublishImage(e,b->current,b->sampler);
 if(flatDDS)history.PopAttributes();else{Check(prior.object||prior.knowledge==NativeCUSBindingKnowledge::DefaultObject,"Original bitmap temporary binding unobserved");registry.Bind(*unit,prior.object?prior.object->name:0,target);}
 e.declared=registry.Find(e.name);bindings.emplace(b.get(),std::move(e));return object->name;
}
void NativeUITextureHistory::ReleaseBuildPicture(const std::shared_ptr<NativeUITextureBinding>&b)
{
 CheckContext();const auto root=Root(b);const auto it=bindings.find(root);if(it==bindings.end())return;
 // UnitDefImage::Free deletes the actual texture name even when it was
 // borrowed from Lua. The Lua owner's recorded name survives and can be
 // rebound as a new empty texture; it must not resurrect the old pixels.
 registry.Delete(it->second.name);if(root==b.get())bindings.erase(it);
}
NativeUITextureHistory::Entry* NativeUITextureHistory::FindPostprocess(const std::shared_ptr<NativePostprocessBinding>&b)
{
 CheckContext();for(auto&[key,e]:bindings)if(e.binding&&e.binding->postprocess==b)return &e;return nullptr;
}
void NativeUITextureHistory::CreatePostprocess(const std::shared_ptr<NativePostprocessBinding>& shared,const std::string& source)
{
 Check(shared&&shared->image&&!shared->deleted&&!FindPostprocess(shared),"Private postprocess creation owner differs");
 const auto&i=*shared->image;ValidateNativePostprocessImage(i);
 auto binding=std::make_shared<NativeUITextureBinding>();binding->postprocess=shared;
 auto metadata=std::make_shared<ScreenTexture>();metadata->width=i.width;metadata->height=i.height;metadata->sourceName=source;metadata->flipV=true;
 switch(i.format){case NativePostprocessFormat::RGBA8:metadata->format=ScreenTextureFormat::RGBA8Unorm;break;case NativePostprocessFormat::RG8:metadata->format=ScreenTextureFormat::RG8Unorm;break;case NativePostprocessFormat::R8Unorm:metadata->format=ScreenTextureFormat::R8Unorm;break;case NativePostprocessFormat::RGBA16Float:metadata->format=ScreenTextureFormat::RGBA16Float;break;case NativePostprocessFormat::Depth32:metadata->format=ScreenTextureFormat::Depth32Float;break;case NativePostprocessFormat::Depth24Unorm:metadata->format=ScreenTextureFormat::Depth24Unorm;break;case NativePostprocessFormat::BC1RGBA:metadata->format=ScreenTextureFormat::BC1RGBA;break;case NativePostprocessFormat::R16Float:metadata->format=ScreenTextureFormat::R16Float;break;case NativePostprocessFormat::R32Float:metadata->format=ScreenTextureFormat::R32Float;break;}
 binding->current=metadata;binding->sampler.filter=i.sampler.minFilter==NativePostprocessFilter::Nearest?ScreenFilter::Nearest:ScreenFilter::Linear;
 binding->sampler.magnificationFilter=i.sampler.magFilter==NativePostprocessFilter::Nearest?ScreenFilter::Nearest:ScreenFilter::Linear;
 const auto wrap=[](NativePostprocessWrap v){switch(v){case NativePostprocessWrap::Repeat:return ScreenWrap::Repeat;case NativePostprocessWrap::ClampEdge:return ScreenWrap::ClampEdge;case NativePostprocessWrap::LegacyClamp:return ScreenWrap::LegacyClamp;}throw std::runtime_error("Private postprocess wrap differs");};
 binding->sampler.wrapS=wrap(i.sampler.wrapS);binding->sampler.wrapT=wrap(i.sampler.wrapT);
 Create(binding,source,0);
}
void NativeUITextureHistory::DeletePostprocess(const std::shared_ptr<NativePostprocessBinding>& b)
{
 if(auto*e=FindPostprocess(b)){auto owned=e->binding;Delete(owned);}
}
unsigned NativeUITextureHistory::BindPostprocess(unsigned unit,const std::shared_ptr<NativePostprocessBinding>& b)
{
 auto*e=FindPostprocess(b);Check(e,"Private postprocess binding has no original texture name");return LuaBind(unit,CaptureBind(e->binding,{}));
}
std::shared_ptr<NativePostprocessPublication> NativeUITextureHistory::PublishPostprocess(const std::shared_ptr<NativePostprocessBinding>& binding,const std::shared_ptr<const NativePostprocessVersion>& version,uint64_t frame)
{
 auto*entry=FindPostprocess(binding);if(!entry)return {}; // An internal image with no public/source texture name is not a GL binding event.
 Check(version&&version->image==binding->image&&!binding->deleted,"Postprocess source publication image differs");
 // Build the prospective immutable descriptor without mutating the live cell.
 auto prospective=std::make_shared<NativePostprocessBinding>(*binding);prospective->current=version;
 auto ui=std::make_shared<NativeUITextureBinding>();ui->postprocess=std::move(prospective);ui->sampler=entry->binding->sampler;
 auto image=ui->Resolve().image;const auto previous=registry.Find(entry->name);
 Check(bool(previous),"Postprocess producer refers to an actually deleted texture");
 struct Receipt final:NativePostprocessPublication {
  NativeUITextureHistory* owner;std::weak_ptr<const int> lifetime;NativeUITexturePublication value;
  void Cancel()noexcept override{if(!lifetime.expired())owner->CancelPublication(value);}
 };
 auto receipt=std::make_shared<Receipt>();receipt->owner=this;receipt->lifetime=publicationLifetime;receipt->value.previous=previous;
 auto backing=std::make_shared<NativeScreenTextureBacking>();backing->image=image;backing->storageGeneration=previous->identity;backing->contentVersion=previous->version+1;backing->content=NativeTextureContent::ProducerVersion;backing->sourceDrawFrame=frame;
 // All allocation precedes the registry's own strong transaction. Afterwards
 // only shared-pointer assignments publish the receipt and descriptor.
 registry.PublishContent(entry->name,{},std::move(backing));receipt->value.published=registry.Find(entry->name);entry->image=std::move(image);return receipt;
}
void NativeUITextureHistory::Delete(const std::shared_ptr<NativeUITextureBinding>&b){auto it=bindings.find(Root(b));if(it==bindings.end())return;registry.Delete(it->second.name);bindings.erase(it);}
void NativeUITextureHistory::Params(const std::shared_ptr<NativeUITextureBinding>&b,ScreenSampler sampler){auto&e=Find(b);auto unit=history.ActiveUnit();Check(bool(unit),"Texture parameters lack actual active unit");auto prior=history.Binding(*unit,target);registry.Bind(*unit,e.name,target);registry.SetSampling(e.name,Sampling(sampler));registry.Bind(*unit,prior.object?prior.object->name:0,target);e.sampler=sampler;}
NativeUITexturePublication NativeUITextureHistory::Publish(const std::shared_ptr<NativeUITextureBinding>&b){auto&e=Find(b);const auto previous=registry.Find(e.name);auto resolved=b->Resolve();PublishImage(e,std::move(resolved.image),resolved.sampler);return{previous,registry.Find(e.name)};}
void NativeUITextureHistory::CancelPublication(const NativeUITexturePublication&p)noexcept{
 if(p.previous==p.published||!registry.CancelContentPublication(p.published,p.previous))return;
 for(auto&[key,e]:bindings)if(e.name==p.previous->name){
  const auto actual=std::dynamic_pointer_cast<const NativeScreenTextureBacking>(p.previous->backing);
  e.image=actual?actual->image:nullptr;
 }
}
void NativeUITextureHistory::ActiveTexture(unsigned unit){CheckContext();history.ActiveTexture(unit);}
unsigned NativeUITextureHistory::LuaEnable(unsigned requested,bool enabled){CheckContext();auto unit=requested?requested:history.ActiveUnit().value();history.LuaTexture(requested,enabled);return unit;}
unsigned NativeUITextureHistory::LuaNoBind(unsigned requested,bool parsed){CheckContext();auto unit=requested?requested:history.ActiveUnit().value();history.LuaTextureLookup(requested,parsed,{});return unit;}
std::shared_ptr<const NativeCUSTextureObject> NativeUITextureHistory::CaptureBind(const std::shared_ptr<NativeUITextureBinding>&binding,const std::shared_ptr<const ScreenTexture>&image)
{
 CheckContext();if(binding){auto&e=Find(binding);auto current=registry.Find(e.name);return current?current:e.declared;}
 for(auto&[name,e]:named)if(e.image==image)return registry.Find(e.name);
 throw std::runtime_error("Named UI image has no actual source loader registration");
}
unsigned NativeUITextureHistory::LuaBind(unsigned requested,std::shared_ptr<const NativeCUSTextureObject> recorded)
{
 CheckContext();auto unit=requested?requested:history.ActiveUnit().value();
 registry.LuaTexture(requested,[&]{return NativeTextureLookup{true,recorded?registry.ResolveRecordedName(std::move(recorded)):nullptr,false};});return unit;
}
unsigned NativeUITextureHistory::LuaLookup(unsigned requested,const std::function<NativeTextureLookup()>&parse){CheckContext();const auto actual=requested?requested:history.ActiveUnit().value();registry.LuaTexture(requested,parse);return actual;}
NativeUITextureObserver::ObservedImage NativeUITextureHistory::BoundImage(unsigned unit)
{
 CheckContext();return DescribeBinding(history.Binding(unit,target));
}
ScreenSampler NativeUITextureHistory::DescribeSampler(const NativeCUSTextureBinding& b)
{
 ScreenSampler sampler;const auto&p=b.object?b.object->sampling:b.defaultSampling;
 const auto filter=p.minFilter;sampler.filter=(filter==0x2600||filter==0x2700||filter==0x2702)?ScreenFilter::Nearest:ScreenFilter::Linear;
 sampler.mipFilter=filter==0x2600||filter==0x2601?ScreenMipFilter::None:filter==0x2700||filter==0x2701?ScreenMipFilter::Nearest:ScreenMipFilter::Linear;
 sampler.magnificationFilter=p.magFilter==0x2600?ScreenFilter::Nearest:ScreenFilter::Linear;
 const auto wrap=[](unsigned v){switch(v){case 0x2901:return ScreenWrap::Repeat;case 0x812F:return ScreenWrap::ClampEdge;case 0x812D:return ScreenWrap::TransparentBorder;case 0x2900:return ScreenWrap::LegacyClamp;}throw std::runtime_error("Original UI sampler wrapping needs actual consumer");};
 sampler.wrapS=wrap(p.wrapS);sampler.wrapT=wrap(p.wrapT);sampler.anisotropy=unsigned(p.anisotropy);sampler.whiteBorder=p.borderColor==std::array<float,4>{1,1,1,1};
 Check(p.compareMode==0&&p.lodBias==0&&p.baseLevel==0,"Original UI sampler comparison/LOD requires actual consumer");
 return sampler;
}
bool NativeUITextureHistory::SamplingIncomplete(const NativeCUSTextureBinding& b)
{
 if(b.knowledge==NativeCUSBindingKnowledge::DefaultObject)return b.defaultStorageKnownEmpty;
 if(!b.object)return false;
 if(b.object->storage==NativeTextureStorageKnowledge::Empty)return true;
 const auto backing=std::dynamic_pointer_cast<const NativeScreenTextureBacking>(b.object->backing);
 if(!backing||!backing->image||!backing->image->levelDefinitions)return false;
 const auto& sampling=b.object->sampling;
 const bool mipmapped=sampling.minFilter!=0x2600&&sampling.minFilter!=0x2601;
 return !NativeUITextureLevelsComplete(*backing->image->levelDefinitions,mipmapped,sampling.baseLevel,sampling.maxLevel);
}
NativeUITextureObserver::ObservedImage NativeUITextureHistory::DescribeBinding(const NativeCUSTextureBinding& b)
{
 CheckContext();Check(b.object||b.knowledge==NativeCUSBindingKnowledge::DefaultObject,"Observed UI image has no original binding");
 const auto sampler=DescribeSampler(b);
 if(!b.object){Check(b.defaultStorageKnownEmpty,"Default UI texture has unobserved source storage");auto empty=std::make_shared<ScreenTexture>();empty->incomplete=true;empty->sourceName="original-default-object0";return{std::move(empty),sampler};}
 const auto&o=*b.object;
 if(auto backing=std::dynamic_pointer_cast<const NativeScreenTextureBacking>(o.backing))return {SelectNativeUITextureSample(backing->image,sampler,o.sampling.maxLevel),sampler};
 if(o.storage==NativeTextureStorageKnowledge::Empty){auto empty=std::make_shared<ScreenTexture>();empty->incomplete=true;empty->sourceName=o.source;return{std::move(empty),sampler};}
 if((o.backing||o.image)&&externalImages){
  auto descriptor=externalImages(b);
  Check(descriptor&&descriptor->sourceTexture==b.object&&descriptor->width==o.allocation.width&&descriptor->height==o.allocation.height&&
   !descriptor->incomplete&&!descriptor->renderTarget&&!descriptor->postprocessVersion&&descriptor->bytes.empty()&&!descriptor->bytesPerRow&&descriptor->mips.empty()&&!descriptor->generateMipmaps,
   "External image descriptor differs from its actual frozen object/backing");
  const auto map=std::dynamic_pointer_cast<const NativeMapTextureBacking>(o.backing);
  // The paired height provider exposes the original R32F storage through an
  // RRRR view. Other external images still require their own channel consumer.
  const bool heightChannels=map&&map->role==NativeMapTextureRole::Height&&descriptor->format==ScreenTextureFormat::R32Float&&
   o.sampling.swizzle==std::array<unsigned,4>{0x1903,0x1903,0x1903,0x1903};
  Check(o.sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906}||heightChannels,"External UI swizzle requires its actual sampled-channel view");
  if(descriptor->format==ScreenTextureFormat::Depth32Float||descriptor->format==ScreenTextureFormat::Depth24Unorm)
   Check(o.sampling.depthMode==0x1909,"External fixed-function depth requires original luminance sampling");
  return {std::move(descriptor),sampler};
 }
 // A real Lua allocation has a retained RTT/copy descriptor, not invented
 // contents. The renderer must allocate/resolve that descriptor before use.
 for(auto&[key,e]:bindings)if(e.name==o.name&&e.binding&&e.binding->current)return{e.binding->current,sampler};
 throw std::runtime_error("Original UI texture needs its actual immutable image provider");
}
bool NativeUITextureHistory::Enabled(unsigned unit)const{CheckContext();const auto b=history.Binding(unit,target);Check(b.enableObserved,"Original UI texture enable was not observed");return b.enabled;}
void NativeUITextureHistory::Push(bool texture,bool enables){CheckContext();history.PushAttributes(texture,enables);}
void NativeUITextureHistory::Pop(){CheckContext();history.PopAttributes();}
void NativeUITextureHistory::Reset2D(){CheckContext();Check(bool(history.ActiveUnit()),"Original ResetGLState texture unit unobserved");history.Enable(*history.ActiveUnit(),target,false);}
void NativeUITextureHistory::UploadNamed(Entry&e){
 const auto image=e.image;const auto unit=history.ActiveUnit();Check(bool(unit),"Original named upload lacks current texture unit");
 const auto prior=history.Binding(*unit,target);
 if(e.flatDDS)history.PushAttributes(true,true);
 // Bitmap CreateTexture restores its temporary binding, then NamedTextures
 // binds it again for qualifiers and finally binds zero. DDS instead wraps
 // its real upload in GL_TEXTURE_BIT, preserving inherited names and enables.
 if(e.flatDDS)history.Enable(*unit,target,true);
 registry.Bind(*unit,e.name,target);
 auto uploadSampling=Sampling(e.sampler);
 if(!e.flatDDS){uploadSampling.wrapS=uploadSampling.wrapT=0x2901;uploadSampling.borderColor={};}
 registry.SetSampling(e.name,uploadSampling);registry.Allocate(e.name,Allocation(*image));e.image.reset();PublishImage(e,image,e.sampler);
 if(e.flatDDS)history.PopAttributes();
 else{
  registry.Bind(*unit,prior.object?prior.object->name:0,target);
  registry.Bind(*unit,e.name,target);registry.SetSampling(e.name,Sampling(e.sampler));registry.Bind(*unit,0,target);
 }
 e.deferred=false;
}
void NativeUITextureHistory::Named(const std::string&name,const std::shared_ptr<const ScreenTexture>&image,ScreenSampler sampler,bool compiling,bool flatDDS){
 CheckContext();if(named.contains(name))return;Check(bool(image),"Named image registration needs actual decoded image");auto o=registry.Create(target,name);Entry e{o->name,{},image,sampler,compiling,flatDDS};
 if(!compiling)UploadNamed(e);
 named.emplace(name,std::move(e));if(compiling)pendingNames.push_back(name);
}
void NativeUITextureHistory::UpdateNamed(){CheckContext();if(pendingNames.empty())return;history.PushAttributes(true,true);for(const auto&name:pendingNames){auto it=named.find(name);if(it==named.end()||!it->second.deferred)continue;UploadNamed(it->second);}history.PopAttributes();pendingNames.clear();}
void NativeUITextureHistory::DeleteNamed(const std::string&name){CheckContext();auto it=named.find(name);if(it==named.end())return;registry.Delete(it->second.name);named.erase(it);}
} // namespace rendering::ui
