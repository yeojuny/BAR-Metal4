#include "Rendering/Backend/NativeUITextureHistory.h"
#include <cassert>
#include <iostream>
using namespace rendering;using namespace rendering::ui;
struct ExternalBacking final:NativeTextureBacking {NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}};
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();
 for(unsigned n=0;n<1000;++n){
  auto object=registry.Create(NativeCUSTextureTarget::Image2D,"allocation",77);
  registry.Allocate(object->name,{8,8,1,1,1,n%2?0x8CACu:0x8058u});
  auto initial=std::make_shared<ScreenTexture>();initial->width=initial->height=8;initial->format=n%2?ScreenTextureFormat::Depth32Float:ScreenTextureFormat::RGBA8Unorm;
  initial->renderTarget=std::make_shared<ScreenRenderTargetIdentity>(ScreenRenderTargetIdentity{77,8,8});
  PublishNativeUIAllocationDescriptor(registry,object->name,initial);
  auto captured=registry.Find(object->name);auto backing=std::dynamic_pointer_cast<const NativeScreenTextureBacking>(captured->backing);
  assert(backing&&backing->image==initial&&backing->content==NativeTextureContent::AllocationOnly&&backing->sourceDrawFrame==0);
  assert(captured->storage==NativeTextureStorageKnowledge::AllocatedUndefined);
  NativeCUSTextureBinding frozen;frozen.knowledge=NativeCUSBindingKnowledge::BoundObject;frozen.object=captured;
  assert(NativeUITextureHistory::DescribeRetainedBinding(frozen,{}).image==initial);
  registry.SetSampling(object->name,NativeTextureSampling{});assert(registry.Find(object->name)->backing==backing);
  auto newer=std::make_shared<ScreenTexture>(*initial);newer->generation=1;
  auto content=std::make_shared<NativeScreenTextureBacking>();content->image=newer;content->content=NativeTextureContent::ProducerVersion;content->storageGeneration=captured->identity;content->contentVersion=registry.Find(object->name)->version+1;
  registry.PublishContent(object->name,{},content);
  assert(backing->image==initial&&backing->image->generation==0&&registry.Find(object->name)->backing==content);
  registry.Delete(object->name);auto replacement=registry.Create(NativeCUSTextureTarget::Image2D,"replacement",77);
  assert(replacement->identity!=captured->identity&&backing->image==initial);
  assert(NativeUITextureHistory::DescribeRetainedBinding(frozen,{}).image==initial);
  registry.Delete(replacement->name);
 }
 auto object=registry.Create(NativeCUSTextureTarget::Image2D,"invalid");registry.Allocate(object->name,{1,1,1,1,1,0x8058});auto prior=registry.Find(object->name);
 auto image=std::make_shared<ScreenTexture>();image->width=image->height=1;image->generation=1;
 try{PublishNativeUIAllocationDescriptor(registry,object->name,image);assert(false);}catch(const std::runtime_error&){}
 assert(registry.Find(object->name)==prior);
 // Capture the actual service, then replace/reset/destroy its source history.
 auto external=std::make_shared<ExternalBacking>();external->storageGeneration=prior->identity;external->contentVersion=prior->version+1;external->content=NativeTextureContent::ProducerVersion;
 registry.PublishContent(object->name,{},external);
 NativeCUSTextureBinding token;token.object=registry.Find(object->name);token.knowledge=NativeCUSBindingKnowledge::BoundObject;
 NativeUICapturedBindingResolver captured;unsigned callsA=0,callsB=0;
 {
  NativeUITextureHistory source(history,registry,[]{return uint64_t(1);});
  const auto provider=[&](unsigned& calls){return [&calls](const NativeCUSTextureBinding& b){++calls;auto d=std::make_shared<ScreenTexture>();d->width=d->height=1;d->format=ScreenTextureFormat::ExternalColor;d->sourceTexture=b.object;return d;};};
  source.SetExternalImageDescriptorProvider(provider(callsA));captured=source.CaptureResolver();
  source.SetExternalImageDescriptorProvider(provider(callsB));source.Reset();
  assert(captured(token).image->sourceTexture==token.object&&callsA==1&&callsB==0);
 }
 registry.NewContext();assert(captured(token).image->sourceTexture==token.object&&callsA==2&&callsB==0);
 std::cout<<"Captured resolver survives provider replacement/reset/history destruction/context renewal; actual UI allocation publication1000:undefined content,RTT/depth,retained token,sampler,producer replacement,delete/recreate and invalid metadata passed\n";
}
