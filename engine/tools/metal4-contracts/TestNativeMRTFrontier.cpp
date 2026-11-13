#include "Rendering/Metal4/Metal4MRTFrontier209.h"
#include "Rendering/Metal4/Metal4MRTWriteReuse209.h"
#include <cassert>
#include <iostream>
using namespace rendering;
using namespace rendering::metal4;
struct TestBacking final:NativeTextureBacking {
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}
};
static std::shared_ptr<const NativeCUSTextureObject> Publish(NativeTextureRegistry& registry,uint64_t name){
 auto old=registry.Find(name);auto backing=std::make_shared<TestBacking>();
 backing->storageGeneration=old->identity;backing->contentVersion=old->version+1;
 backing->content=NativeTextureContent::ProducerVersion;
 registry.PublishContent(name,{},backing);return registry.Find(name);
}
int main(){
 NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();
 for(unsigned iteration=0;iteration<1000;++iteration){
  NativeOpaqueModelFrame frame;frame.sourceDrawFrame=9;frame.passes.resize(4);
  auto unit=std::make_shared<NativeCUSDeferredPhase>();auto feature=std::make_shared<NativeCUSDeferredPhase>();
  unit->sourceDrawFrame=feature->sourceDrawFrame=9;feature->feature=true;
  frame.passes[0].mrt=unit;frame.passes[0].deferred=true;
  frame.passes[2].mrt=feature;frame.passes[2].feature=true;frame.passes[2].deferred=true;
  std::array<uint64_t,6> names{};std::array<uint64_t,6> restoredVersions{};
  for(unsigned n=0;n<6;++n){
   auto object=registry.Create(NativeCUSTextureTarget::Image2D,"frontier");names[n]=object->name;
   registry.Allocate(object->name,{8,8,1,1,1,n==5?0x8CACu:0x8058u});
   auto u=Publish(registry,object->name);auto f=Publish(registry,object->name);
   if(n<5){unit->colors[n].current=u;feature->colors[n].previous=u;feature->colors[n].current=f;}
   else{unit->depth.published=u;feature->depth.previous=u;feature->depth.published=f;}
   // Force Find's real lazy normalization: temporary later content is withdrawn.
   auto later=Publish(registry,object->name);assert(registry.CancelContentPublication(later,f));
   restoredVersions[n]=later->version+1;
  }
  std::array<long,6> counts{};
  for(unsigned n=0;n<6;++n)counts[n]=n<5?unit->colors[n].current.use_count():unit->depth.published.use_count();
  auto captured=MRTFrontier209::Capture(frame,registry);assert(captured&&captured->context==registry.ContextGeneration());
  auto repeated=MRTFrontier209::Capture(frame,registry);assert(repeated);
  for(unsigned n=0;n<6;++n)assert(repeated->current[n]==captured->current[n]);
  for(unsigned n=0;n<6;++n){
   const auto& f=n<5?feature->colors[n].current:feature->depth.published;
   assert(captured->current[n]&&captured->current[n]->backing==f->backing);
   assert(captured->current[n]->version==restoredVersions[n]);
   assert(registry.Find(names[n])==captured->current[n]);
   assert((n<5?unit->colors[n].current.use_count():unit->depth.published.use_count())==counts[n]);
   auto newer=Publish(registry,names[n]);assert(newer->backing!=captured->current[n]->backing);
   assert(!MRTFrontier209::Capture(frame,registry)); // Latest producer is no longer captured F.
   registry.Delete(names[n]);assert(captured->current[n]->backing==f->backing);
  }
 }
 NativeOpaqueModelFrame malformed;
 assert(!MRTFrontier209::Capture(malformed,registry));
 malformed.passes.resize(4);assert(!MRTFrontier209::Capture(malformed,registry));
 malformed.passes[2].mrt=std::make_shared<NativeCUSDeferredPhase>();
 assert(!MRTFrontier209::Capture(malformed,registry));
 registry.CloseContext();assert(!MRTFrontier209::Capture(malformed,registry));
 std::cout<<"MRT frontier1000:actual lazy normalization,exact F tokens,later publication/deletion isolation,no extra U owners,malformed capture passed\n";
}
