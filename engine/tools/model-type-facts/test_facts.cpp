#include "Rendering/Backend/NativePositiveBackingFacts.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

static std::size_t allocations=0;
void* operator new(std::size_t size){++allocations;if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete[](void* p)noexcept{::operator delete(p);}

using namespace rendering;
using Cache=NativePositiveBackingFacts<NativeModelTextureBacking,2>;
struct Other final:NativeTextureBacking {NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}};
int main(){
 int lease=0,context=0,otherLease=0,otherContext=0;
 Cache::Scope scope{&lease,&context,1};Cache cache(scope,true);
 auto model=std::make_shared<NativeModelTextureBacking>();
 std::shared_ptr<const NativeTextureBacking> base=model;
 assert(!cache.Find(scope,base));
 auto proof=Cache::Classify(base);assert(proof.Typed().get()==model.get());
 assert(cache.Remember(scope,proof));assert(cache.Find(scope,base).get()==model.get());
 const auto before=allocations;for(int i=0;i<1000;++i)assert(cache.Find(scope,base).get()==model.get());assert(allocations==before);
 assert(!cache.Find({&otherLease,&context,1},base));assert(!cache.Find({&lease,&otherContext,1},base));
 assert(!cache.Find({&lease,&context,2},base));assert(!cache.Find({&lease,&context,0},base));
 std::shared_ptr<const NativeTextureBacking> differentControl(model.get(),[](const NativeTextureBacking*){});
 assert(!cache.Find(scope,differentControl));
 auto unknown=std::make_shared<Other>();auto negative=Cache::Classify(unknown);
 assert(!negative.Typed()&&!cache.Remember(scope,negative));assert(!cache.Find(scope,unknown));
 auto second=std::make_shared<NativeModelTextureBacking>();auto secondProof=Cache::Classify(second);
 assert(cache.Remember(scope,secondProof));assert(cache.Size()==2);
 auto third=std::make_shared<NativeModelTextureBacking>();auto thirdProof=Cache::Classify(third);
 assert(!cache.Remember(scope,thirdProof));assert(!cache.Find(scope,third));assert(thirdProof.Typed().get()==third.get());
 // A type fact never snapshots payload or readiness. The same typed object
 // still exposes its current payload for the original provider to validate.
 auto pixels=std::make_shared<WorldTexture>();model->pixels=pixels;
 assert(cache.Find(scope,base)->pixels==pixels);model->pixels.reset();assert(!cache.Find(scope,base)->pixels);
 struct Pair {NativeModelTextureBacking first,second;};auto pair=std::make_shared<Pair>();
 Cache aliases(scope);std::shared_ptr<const NativeTextureBacking> a(pair,&pair->first),b(pair,&pair->second);
 auto aliasProof=Cache::Classify(a);assert(aliases.Remember(scope,aliasProof));assert(!aliases.Find(scope,b));
 // Destroy and reconstruct the actual class at the same address. Weak cache
 // entries neither keep it alive nor mistake a new control block for a hit.
 alignas(NativeModelTextureBacking) unsigned char storage[sizeof(NativeModelTextureBacking)];
 Cache recycled(scope);std::weak_ptr<const NativeModelTextureBacking> weak;
 {
  auto* raw=std::construct_at(reinterpret_cast<NativeModelTextureBacking*>(storage));
  auto owned=std::shared_ptr<NativeModelTextureBacking>(raw,[](auto* p){std::destroy_at(p);});weak=owned;
  auto p=Cache::Classify(owned);assert(recycled.Remember(scope,p));
 }
 assert(weak.expired());
 {
  auto* raw=std::construct_at(reinterpret_cast<NativeModelTextureBacking*>(storage));
  auto owned=std::shared_ptr<NativeModelTextureBacking>(raw,[](auto* p){std::destroy_at(p);});
  assert(!recycled.Find(scope,owned));auto p=Cache::Classify(owned);assert(recycled.Remember(scope,p));assert(recycled.Find(scope,owned).get()==raw);
 }
 try {Cache unwind(scope);auto owner=std::make_shared<NativeModelTextureBacking>();weak=owner;auto p=Cache::Classify(owner);unwind.Remember(scope,p);throw 1;}catch(int){}
 assert(weak.expired());
 std::cout<<"PASS actual-model-class; ownership/scope/expiry/ABA/aliases/capacity/unknown/payload/unwind; 1000 allocation-free hits\n";
}
