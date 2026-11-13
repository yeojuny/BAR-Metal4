#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeDeferredColorData.h"
#include "Rendering/Backend/NativeUnitStencilData.h"
#include "Rendering/Backend/NativeBloomData.h"
#include "Rendering/Backend/NativeSSAOData.h"
#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
using namespace rendering;
template<class Base> struct Resource final:Base {
 std::atomic<bool> completed{false};
 unsigned initialized=42;
 bool Completed()const noexcept override{return completed.load(std::memory_order_acquire);}
};
template<class Backing,class Base> void Check(Backing& backing){
 assert(!backing.Completed()&&!backing.Resource());
 auto value=std::make_shared<Resource<Base>>();
 std::promise<void> published,release;auto ready=release.get_future();
 auto worker=std::async(std::launch::async,[&]{backing.Fulfill(value);published.set_value();ready.wait();value->completed.store(true,std::memory_order_release);});
 published.get_future().wait();
 for(int i=0;i<100;++i){auto owned=std::dynamic_pointer_cast<const Resource<Base>>(backing.Resource());assert(owned&&owned->initialized==42&&!backing.Completed());}
 release.set_value();while(!backing.Completed())std::this_thread::yield();worker.get();
 bool duplicate=false;try{backing.Fulfill(std::make_shared<Resource<Base>>());}catch(const std::exception&){duplicate=true;}
 assert(duplicate&&backing.Completed()&&backing.Resource()==value);
 bool absent=false;try{backing.Fulfill({});}catch(const std::exception&){absent=true;}assert(absent&&backing.Completed());
}
void CheckPublicationRace(){
 NativeMapTextureBacking backing;
 auto worker=std::async(std::launch::async,[&]{
  auto value=std::make_shared<Resource<NativeMapTextureResource>>();
  value->initialized=1234;value->completed.store(true,std::memory_order_relaxed);
  backing.Fulfill(value);
 });
 // No promise or completed future supplies a publication edge to this reader.
 std::shared_ptr<const NativeMapTextureResource> captured;
 while(!(captured=backing.Resource()))std::this_thread::yield();
 assert(std::dynamic_pointer_cast<const Resource<NativeMapTextureResource>>(captured)->initialized==1234);
 assert(backing.Completed());worker.get();
 NativeMapTextureBacking newer;assert(!newer.Completed()&&backing.Completed());
 auto a=std::make_shared<Resource<NativeMapTextureResource>>();
 auto b=std::make_shared<Resource<NativeMapTextureResource>>();
 std::atomic<unsigned> successes{0},failures{0};std::atomic<bool> start{false};
 auto race=[&](auto value){while(!start.load(std::memory_order_acquire))std::this_thread::yield();
  try{newer.Fulfill(value);++successes;}catch(const std::exception&){++failures;}};
 auto first=std::async(std::launch::async,race,a);auto second=std::async(std::launch::async,race,b);
 start.store(true,std::memory_order_release);first.get();second.get();
 assert(successes==1&&failures==1&&(newer.Resource()==a||newer.Resource()==b));
 assert(!newer.Completed()&&backing.Completed());
}
template<class Producer> void CheckProducer(){
 Producer p;assert(!p.completed.load(std::memory_order_acquire));
 auto worker=std::async(std::launch::async,[&]{p.completed.store(true,std::memory_order_release);});
 while(!p.completed.load(std::memory_order_acquire))std::this_thread::yield();worker.get();
 Producer next;assert(!next.completed.load(std::memory_order_acquire));
}
int main(){
 for(int i=0;i<100;++i){
  CheckPublicationRace();
  NativeShieldDepthMetadata m;m.width=m.height=8;m.viewport={0,0,8,8};
  NativeShieldDepthBacking shield(NativeTextureBackingKind::MapDepth,1,2,3,NativeTextureContent::ProducerVersion,m);
  Check<NativeShieldDepthBacking,NativeShieldDepthResource>(shield);
  NativeDeferredColorBacking color(1,2,3,NativeTextureContent::ProducerVersion,NativeDeferredColorMetadata{NativeGeometryTextureRole::Map,0,m});
  Check<NativeDeferredColorBacking,NativeDeferredColorResource>(color);
  NativeMapTextureBacking map;Check<NativeMapTextureBacking,NativeMapTextureResource>(map);
  // CPU-uploaded heights are ready immediately when the exact resource is published.
  NativeMapTextureBacking height;auto ready=std::make_shared<Resource<NativeMapTextureResource>>();ready->completed=true;height.Fulfill(ready);assert(height.Completed());
  CheckProducer<NativeUnitStencilProducer>();CheckProducer<NativeBloomProducerState>();CheckProducer<NativeSSAOProducerState>();
 }
 std::cout<<"300 actual backing publication/completion + 300 producer transitions; 100 ready-at-upload height versions\n";
}
