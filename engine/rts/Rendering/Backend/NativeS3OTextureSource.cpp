/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeS3OTextureSource.h"
#include <algorithm>
#include <cctype>
#include <exception>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <utility>
namespace rendering {
namespace {
void Check(bool condition,const char* why){if(!condition)throw std::logic_error(why);}
bool DDS(const std::string& path){auto dot=path.find_last_of('.');if(dot==std::string::npos)return false;auto ext=path.substr(dot+1);std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});return ext=="dds";}
bool OriginalDummy(const WorldTexture& image){return image.sourcePath.starts_with("Original CS3OTextureHandler::AllocDummy ");}
unsigned Levels(const WorldTexture& image){Check(!image.mips.empty(),"Original S3O image has no mip data");if(!image.generateMipmaps)return image.mips.size();unsigned levels=1;for(auto n=std::max(image.mips.front().width,image.mips.front().height);n>1;n/=2)++levels;return levels;}
[[noreturn]] void Rethrow(std::exception_ptr failure,std::initializer_list<std::exception_ptr> cleanup){
 std::string detail;
 for(const auto& error:cleanup)if(error){try{std::rethrow_exception(error);}catch(const std::exception& value){detail+="; ";detail+=value.what();}catch(...){detail+="; non-standard exception";}}
 if(detail.empty())std::rethrow_exception(failure);
 try{std::rethrow_exception(failure);}catch(...){std::throw_with_nested(std::runtime_error("Original S3O restore/mirror failed"+detail));}
}
}
void NativeS3OTextureSource::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h,Loader load,std::function<uint64_t()> frame,std::function<void(unsigned)> mirror){
 Check(!registry&&r.ContextGeneration()&&r.MatchesHistory(h)&&load&&frame&&mirror,"Original S3O source needs its single actual context, loader and UI mirror");registry=&r;history=&h;loader=std::move(load);sourceFrame=std::move(frame);syncObservedTextureBinding=std::move(mirror);context=r.ContextGeneration();
}
void NativeS3OTextureSource::Detach(){Check(entries.empty(),"Original S3O texture owner must be killed before context detach");registry=nullptr;history=nullptr;loader={};sourceFrame={};syncObservedTextureBinding={};context=0;}
unsigned NativeS3OTextureSource::BoundUIUnits(uint64_t name)const{
 unsigned units=0;for(unsigned unit=0;unit<2;++unit){const auto binding=history->Binding(unit,NativeCUSTextureTarget::Image2D);if(binding.object&&binding.object->name==name)units|=1u<<unit;}return units;
}
void NativeS3OTextureSource::SyncUIUnits(unsigned units){
 std::exception_ptr first,second;
 for(unsigned unit=0;unit<2;++unit)if(units&(1u<<unit)){try{syncObservedTextureBinding(unit);}catch(...){if(!first)first=std::current_exception();else second=std::current_exception();}}
 if(first)Rethrow(first,{second});
}
void NativeS3OTextureSource::Delete(uint32_t name){
 if(!name)return;Check(registry&&registry->ContextGeneration()==context,"Original S3O deletion outlived its actual context");
 const auto units=BoundUIUnits(name);if(registry->Delete(name))SyncUIUnits(units);
}
std::shared_ptr<const WorldTexture> NativeS3OTextureSource::LoadPixels(const std::string& name,unsigned number,bool invertAxis,bool invertAlpha,bool dummy,bool reload)const{
 auto image=loader(name,reload);if(!image)image=loader("unittextures/"+name,reload);
 if(!image){
  if(!dummy)return {};
  auto out=std::make_shared<WorldTexture>();out->sourcePath="Original CS3OTextureHandler::AllocDummy "+name;out->originalInternalFormat=0x8058;out->generateMipmaps=true;
  // Original missing-file alpha is 255*(1-invertAlpha), then InvertAlpha
  // is applied by the same source path, so the final alpha is always255.
  out->mips.push_back({1,1,4,{uint8_t(number==0?255:0),0,0,255}});return out;
 }
 // Original CBitmap transformations intentionally do nothing for DDS.
 if(DDS(image->sourcePath)||(!invertAxis&&!invertAlpha))return image;
 Check(image->format==WorldTextureFormat::RGBA8Unorm&&image->mips.size()==1,"Original S3O ordinary bitmap channel format differs");
 auto out=std::make_shared<WorldTexture>(*image);auto& m=out->mips.front();
 Check(m.bytesPerRow==size_t(m.width)*4&&m.bytes.size()==m.bytesPerRow*m.height,"Original S3O ordinary bitmap extent differs");
 if(invertAxis)for(size_t y=0;y<m.height/2;++y)for(size_t x=0;x<m.bytesPerRow;++x)std::swap(m.bytes[y*m.bytesPerRow+x],m.bytes[(m.height-1-y)*m.bytesPerRow+x]);
 if(invertAlpha)for(size_t n=3;n<m.bytes.size();n+=4)m.bytes[n]=255-m.bytes[n];
 return out;
}
void NativeS3OTextureSource::Upload(const std::string& name,Entry& entry){
 Check(registry&&registry->ContextGeneration()==context&&entry.pixels,"Original S3O upload lost its actual source/context");const auto active=history->ActiveUnit();Check(bool(active),"Original S3O upload has unobserved active texture unit");
 const auto& image=*entry.pixels;Check(!image.mips.empty(),"Original S3O source image has no base level");const auto& base=image.mips.front();const auto levels=Levels(image);
 Check(base.width&&base.height&&levels,"Original S3O source image extent differs");const auto prior=history->Binding(*active,NativeCUSTextureTarget::Image2D);
 Check(prior.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original S3O saved binding was not observed");const bool created=!entry.name;
 // An empty original texture filename is a valid AllocDummy cache key.
 // The decoded/dummy image still owns its source; keep the filename key intact.
 if(created)entry.name=registry->Create(NativeCUSTextureTarget::Image2D,name.empty()?image.sourcePath:name)->name;
 Check(entry.name<=std::numeric_limits<uint32_t>::max(),"Original S3O GLuint namespace overflow");
 bool attributes=false,restored=false;
 const auto restore=[&]{
  if(restored)return;
  if(attributes){history->PopAttributes();attributes=false;restored=true;SyncUIUnits(3);}
  else{registry->Bind(*active,prior.object?prior.object->name:0,NativeCUSTextureTarget::Image2D);restored=true;if(*active<2)SyncUIUnits(1u<<*active);}
 };
 try{
  if(entry.dds){history->PushAttributes(true,true);attributes=true;history->Enable(*active,NativeCUSTextureTarget::Image2D,true);if(*active<2)SyncUIUnits(1u<<*active);}
  registry->Bind(*active,entry.name,NativeCUSTextureTarget::Image2D);
  if(*active<2)SyncUIUnits(1u<<*active);
  auto sampling=registry->Find(entry.name)->sampling;
  // Ordinary CBitmap::CreateTexture writes wrap before uploading storage,
  // then MAG/MIN afterward. DDS only writes MIN after its actual upload.
  if(!entry.dds){sampling.wrapS=sampling.wrapT=0x2901;registry->SetSampling(entry.name,sampling);SyncUIUnits(BoundUIUnits(entry.name));}
  registry->Allocate(entry.name,{base.width,base.height,1,levels,1,image.originalInternalFormat});SyncUIUnits(BoundUIUnits(entry.name));
  if(!entry.dds){sampling.baseLevel=0;sampling.maxLevel=levels-1;registry->SetSampling(entry.name,sampling);SyncUIUnits(BoundUIUnits(entry.name));}
  const auto current=registry->Find(entry.name);auto backing=std::make_shared<NativeModelTextureBacking>();backing->storageGeneration=current->identity;backing->contentVersion=current->version+1;backing->sourceDrawFrame=sourceFrame();backing->content=NativeTextureContent::ProducerVersion;backing->pixels=entry.pixels;
  registry->PublishContent(entry.name,entry.pixels,std::move(backing));SyncUIUnits(BoundUIUnits(entry.name));
  sampling=registry->Find(entry.name)->sampling;
  sampling.minFilter=entry.dds?(image.mipmappedSampling?0x2703:0x2601):(levels>1?0x2703:0x2601);
  if(!entry.dds)sampling.magFilter=0x2601;
  registry->SetSampling(entry.name,sampling);SyncUIUnits(BoundUIUnits(entry.name));
  restore();
 }catch(...){
  const auto failure=std::current_exception();std::exception_ptr restoreFailure,deleteFailure;
  try{restore();}catch(...){restoreFailure=std::current_exception();}
  if(created){const auto failed=entry.name;entry.name=0;try{Delete(uint32_t(failed));}catch(...){deleteFailure=std::current_exception();}}
  Rethrow(failure,{restoreFailure,deleteFailure});
 }
}
std::shared_ptr<const WorldTexture> NativeS3OTextureSource::Pixels(const std::string& name)const{
 Check(registry&&registry->ContextGeneration()==context,"Original S3O pixels queried outside their actual context");const auto it=entries.find(name);
 Check(it!=entries.end()&&it->second.name&&it->second.pixels,"Original model texture was not uploaded by S3OTextureHandler");return it->second.pixels;
}
NativeS3OTextureSource::Result NativeS3OTextureSource::Load(const std::string& name,unsigned number,bool invertAxis,bool invertAlpha,bool preload){
 Check(registry&&registry->ContextGeneration()==context&&number<2,"Original S3O source load is outside its actual lifetime");
 auto it=entries.find(name);
 if(it==entries.end()){
  Entry e;e.invertAxis=invertAxis;e.invertAlpha=invertAlpha;e.pixels=LoadPixels(name,number,invertAxis,invertAlpha,true,false);
  // AllocDummy is an ordinary RGBA bitmap even when the missing requested
  // filename ends in .dds. Preserve its ordinary upload/sampler lifecycle.
  e.dds=!OriginalDummy(*e.pixels)&&DDS(e.pixels->sourcePath);e.missingPrimary=number==0&&OriginalDummy(*e.pixels);
  it=entries.emplace(name,std::move(e)).first;
 }
 auto& entry=it->second;if(!preload&&!entry.name)Upload(name,entry);
 const auto& base=entry.pixels->mips.front();const bool missing=std::exchange(entry.missingPrimary,false);return{uint32_t(entry.name),base.width,base.height,entry.invertAxis,entry.invertAlpha,missing};
}
void NativeS3OTextureSource::Reload(const std::string& name){
 Check(registry&&registry->ContextGeneration()==context,"Original S3O reload is outside its actual lifetime");
 const auto found=entries.find(name);Check(found!=entries.end()&&found->second.name,"Original S3O reload has no original uploaded texture");
 auto& entry=found->second;auto image=LoadPixels(name,0,entry.invertAxis,entry.invertAlpha,false,true);if(!image)return;Entry next=entry;next.pixels=std::move(image);next.dds=DDS(next.pixels->sourcePath);Upload(name,next);entry=std::move(next);
}
void NativeS3OTextureSource::Kill(){if(registry)for(const auto& [name,entry]:entries)if(entry.name)Delete(uint32_t(entry.name));entries.clear();}
std::shared_ptr<const WorldTexture> NativeS3OTexturePixels(const NativeS3OTexturePair& pair,unsigned slot){
 Check(slot<2&&pair.contextGeneration&&pair.objects[slot],"Original S3O source pair has no actual immutable object");
 const auto& object=*pair.objects[slot];const auto backing=std::dynamic_pointer_cast<const NativeModelTextureBacking>(object.backing);
 Check(object.contextGeneration==pair.contextGeneration&&object.target==NativeCUSTextureTarget::Image2D&&object.storage==NativeTextureStorageKnowledge::Published&&backing&&backing->pixels&&object.image==backing->pixels,"Original S3O source pair lost its actual published model backing");
 Check(!backing->pixels->mips.empty()&&object.allocation.width==backing->pixels->mips.front().width&&object.allocation.height==backing->pixels->mips.front().height&&object.allocation.levels&&object.allocation.depth==1&&object.allocation.samples==1,"Original S3O source pair allocation differs from its actual image");
 return backing->pixels;
}
std::shared_ptr<const NativeCUSTextureObject> NativeS3OTextureSource::CurrentObject(uint64_t actualContext,uint64_t name,uint64_t identity)const{
 Check(registry&&actualContext==registry->ContextGeneration(),"Original bound S3O object outlived its actual context");
 auto object=registry->Find(name);
 Check(object&&object->identity==identity&&object->contextGeneration==actualContext,"Original bound S3O object was deleted or its name reused");
 NativeS3OTexturePair current;current.contextGeneration=actualContext;current.objects[0]=object;
 (void)NativeS3OTexturePixels(current,0);return object;
}
std::shared_ptr<const NativeS3OTexturePair> NativeS3OTextureSource::CapturePair(unsigned bin,uint32_t primary,uint32_t secondary)const{
 Check(registry&&registry->ContextGeneration()==context&&sourceFrame,"Original S3O pair captured outside its actual source context");
 Check(primary&&secondary,"Original S3O material has no uploaded primary/secondary names");
 auto pair=std::make_shared<NativeS3OTexturePair>();pair->contextGeneration=context;pair->sourceDrawFrame=sourceFrame();pair->originalTextureBin=bin;
 pair->objects={registry->Find(primary),registry->Find(secondary)};
 for(unsigned slot=0;slot<2;++slot)(void)NativeS3OTexturePixels(*pair,slot);
 return pair;
}
NativeS3OTextureSource& OriginalNativeS3OTextures(){static NativeS3OTextureSource source;return source;}
}
