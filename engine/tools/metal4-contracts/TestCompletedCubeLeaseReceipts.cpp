#include <array>
#include <optional>
#include <new>
#include <memory>
#include <functional>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <stdexcept>
struct NativeCUSTextureObject{};
struct NativeCubeTextureResource{virtual ~NativeCubeTextureResource()=default;};
enum class NativeTextureContent{AllocationOnly,ProducerVersion,CPUUpload};
enum class NativeCubeTextureKind{Reflection,SkyReflection,Specular};
struct NativeCubeTextureBacking {
 NativeTextureContent content=NativeTextureContent::ProducerVersion;
 NativeCubeTextureKind kind=NativeCubeTextureKind::Reflection;
 mutable std::shared_ptr<const NativeCubeTextureBacking> prior;
 mutable std::atomic<bool> completed{false};
 mutable std::atomic<uint64_t> dependenciesSubmission{0},encodedSubmission{0};
};
struct Resource:NativeCubeTextureResource{bool cpuUploaded=false;};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
#include "completed-cube-eligibility.inc"
#include "completed-cube-readable.inc"
struct Query{const void* context=nullptr;bool(*inspect)(const void*)=nullptr;};
struct CUSSourceImage {
 void* texture=nullptr;void* sampler=nullptr;
 std::shared_ptr<const void> owner;
 std::function<void()> requireReadable,didComplete;
 Query dependencyQuery160;
};
#include "completed-cube-cache.inc"
enum class SourceDependencyWork160 {NoCommands,CommandsRequired};
struct SourceDependencyResult160 {SourceDependencyWork160 work;bool known;};
struct PreparedCubeTexture {
 struct Storage {void* texture=nullptr;};
 struct Resource {std::shared_ptr<Storage> storage=std::make_shared<Storage>();};
 struct Impl {
  std::shared_ptr<const NativeCubeTextureBacking> image;
  std::shared_ptr<Resource> storage=std::make_shared<Resource>();
  std::shared_ptr<PreparedCubeTexture> prior;
  void* sampler=nullptr;
  uint64_t submission=0,predecessor=0;
  bool completion=false;
 };
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 SourceDependencyResult160 InspectDependencies160()const;
 void DidComplete();
 CUSSourceImage Image()const;
};
#include "completed-cube-prepared.inc"
struct Owner {
 std::shared_ptr<NativeCubeTextureBacking> image;
 int calls=0;bool complete=false;
 bool Inspect()const{return image->completed;}
};
struct ThrowingCopy {
 static inline bool fail=false;
 char large[128]{};
 ThrowingCopy()=default;
 ThrowingCopy(const ThrowingCopy&){if(fail)throw std::bad_alloc();}
 void operator()()const{}
};
int main(){
 // Actual Prepared methods: sharing an already completed prior-free read
 // cannot change producer flags or release a prior. Query remains live via owner.
 {
  auto backing=std::make_shared<NativeCubeTextureBacking>();backing->completed=true;
  backing->dependenciesSubmission=9;backing->encodedSubmission=9;
  auto make=[&]{auto p=std::make_shared<PreparedCubeTexture>();p->impl->image=backing;p->impl->submission=10;return p;};
  auto first=make(),second=make();first->DidComplete();second->DidComplete();first->DidComplete();
  assert(backing->completed&&!backing->prior&&backing->dependenciesSubmission==9&&backing->encodedSubmission==9);
  assert(first->InspectDependencies160().work==SourceDependencyWork160::NoCommands);
  auto receipt=first->Image();receipt.owner=first;
  receipt.didComplete=[first]{first->DidComplete();};
  receipt.dependencyQuery160={first.get(),[](const void* p){return static_cast<const PreparedCubeTexture*>(p)->InspectDependencies160().work==SourceDependencyWork160::NoCommands;}};
  std::weak_ptr<PreparedCubeTexture> lifetime=first;first.reset();second.reset();
  receipt.requireReadable();receipt.didComplete();assert(receipt.dependencyQuery160.inspect(receipt.dependencyQuery160.context));
  receipt={};assert(lifetime.expired());
  auto borrowed=make();backing->completed=false;borrowed->impl->predecessor=9;
  try{borrowed->DidComplete();assert(false);}catch(const std::runtime_error&){}
  assert(!borrowed->impl->completion);backing->completed=true;borrowed->DidComplete();
  assert(borrowed->impl->completion);
 }
 Resource resource;NativeCubeTextureBacking source;
 assert(!CompletedCubeReadEligible(source,resource,true));
 source.completed=true;assert(CompletedCubeReadEligible(source,resource,true));
 assert(!CompletedCubeReadEligible(source,resource,false));
 source.content=NativeTextureContent::AllocationOnly;assert(!CompletedCubeReadEligible(source,resource,true));
 source.content=NativeTextureContent::ProducerVersion;
 source.prior=std::make_shared<NativeCubeTextureBacking>();assert(!CompletedCubeReadEligible(source,resource,true));source.prior.reset();
 source.kind=NativeCubeTextureKind::Specular;assert(!CompletedCubeReadEligible(source,resource,true));resource.cpuUploaded=true;
 assert(CompletedCubeReadEligible(source,resource,true));
 // Upload/producer transitions are observed only on the next explicit predicate.
 source.completed=false;const bool before=CompletedCubeReadEligible(source,resource,true);source.completed=true;assert(!before);
 // Actual readability preserves incomplete-face and borrowed-predecessor rules.
 source.completed=false;source.dependenciesSubmission=3;source.encodedSubmission=0;
 try{Readable(source,4,3);assert(false);}catch(const std::runtime_error&){}
 source.encodedSubmission=3;Readable(source,4,3);
 try{Readable(source,4);assert(false);}catch(const std::runtime_error&){}
 source.completed=true;Readable(source,4);
 for(int cycle=0;cycle<1000;++cycle){
  CUSSourceImage returned;std::weak_ptr<Owner> life;
  {
   CompletedCubeLeaseReceipts cache,other;
   CompletedCubeLeaseReceipts::Key key{std::make_shared<NativeCUSTextureObject>(),std::make_shared<Resource>(),7,23};
   auto owner=std::make_shared<Owner>();owner->image=std::make_shared<NativeCubeTextureBacking>();owner->image->completed=true;life=owner;
   CUSSourceImage image;image.owner=owner;
   image.requireReadable=[owner]{++owner->calls;Readable(*owner->image,7);};
   image.didComplete=[owner]{owner->complete=true;};
   image.dependencyQuery160={owner.get(),[](const void* p){return static_cast<const Owner*>(p)->Inspect();}};
   cache.Remember(key,image);assert(!other.Find(key));
   for(int i=0;i<4;++i){auto k=key;switch(i){case 0:k.object=std::make_shared<NativeCUSTextureObject>();break;case 1:k.resource=std::make_shared<Resource>();break;case 2:++k.submission;break;case 3:++k.sampler;break;}assert(!cache.Find(k));}
   auto* hit=cache.Find(key);assert(hit&&hit->owner==image.owner&&hit->dependencyQuery160.context==owner.get());
   hit->requireReadable();hit->requireReadable();assert(owner->calls==2);
   for(int i=1;i<32;++i){auto k=key;k.submission+=i;cache.Remember(k,image);assert(cache.Find(k));}
   auto full=key;full.submission+=32;cache.Remember(full,image);assert(!cache.Find(full));
   returned=*hit;
  }
  assert(!life.expired());assert(returned.dependencyQuery160.inspect(returned.dependencyQuery160.context));
  returned.requireReadable();returned.didComplete();assert(life.lock()->complete);
  returned={};assert(life.expired());
 }
 CompletedCubeLeaseReceipts cache;
 CompletedCubeLeaseReceipts::Key key{};
 CUSSourceImage image;image.owner=std::make_shared<int>(1);image.requireReadable=ThrowingCopy{};
 ThrowingCopy::fail=true;cache.Remember(key,image);assert(!cache.Find(key));
 ThrowingCopy::fail=false;cache.Remember(key,image);assert(cache.Find(key));
 ThrowingCopy::fail=true;try{auto copy=*cache.Find(key);(void)copy;assert(false);}catch(const std::bad_alloc&){}
 ThrowingCopy::fail=false;auto copy=*cache.Find(key);copy.requireReadable();
}
