#include "Rendering/Backend/NativeCUSDeferredData.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
static thread_local long failAfter=-1;
void* operator new(std::size_t n){if(failAfter>=0&&failAfter--==0){failAfter=-1;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
using namespace rendering;
int main(){
 unsigned failures=0,successes=0;
 for(bool map:{false,true})for(int fault=-1;fault<64;++fault){
  NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();history.ActiveTexture(0);
  NativeGeometryTextureNames names(registry,history,"receipt allocation");names.Update(8,8,NativeCUSTextureTarget::Image2D,1,map);
  WorldCamera camera;camera.viewport={0,0,8,8};
  const auto begin=[&]{return map?BeginNativeMapDeferredPhase(registry,names,3,camera):BeginNativeCUSDeferredPhase(registry,names,3,camera,false,true);};
  std::shared_ptr<NativeCUSDeferredPhase> receipt;
  bool threw=false;failAfter=fault;
  try{receipt=OwnNativeCUSDeferredPhase(begin);}catch(const std::bad_alloc&){threw=true;}
  failAfter=-1;
  if(threw){++failures;assert(!receipt);}else{++successes;assert(receipt&&receipt->depth.Changed());CancelNativeCUSDeferredPhase(registry,*receipt);}
  for(unsigned n=0;n<(map?7u:6u);++n){auto object=registry.Find(names.Name(n));assert(object&&object->storage==NativeTextureStorageKnowledge::AllocatedUndefined&&!object->backing);}
 }
 assert(failures>10&&successes>0);
 // Deliberately arm a failure after real Begin succeeds. Correct helper has no
 // remaining allocation; old make_shared(Begin()) loses published receipts.
 for(bool map:{false,true}){
  NativeCUSTextureHistory history;NativeTextureRegistry registry(history);registry.NewContext();history.ActiveTexture(0);
  NativeGeometryTextureNames names(registry,history,"post Begin");names.Update(8,8,NativeCUSTextureTarget::Image2D,1,map);
  WorldCamera camera;camera.viewport={0,0,8,8};std::shared_ptr<NativeCUSDeferredPhase> receipt;
  try{receipt=OwnNativeCUSDeferredPhase([&]{auto phase=map?BeginNativeMapDeferredPhase(registry,names,4,camera):BeginNativeCUSDeferredPhase(registry,names,4,camera,false,true);failAfter=0;return phase;});}
  catch(const std::bad_alloc&){}
  failAfter=-1;assert(receipt);CancelNativeCUSDeferredPhase(registry,*receipt);
 }
 std::cout<<"Actual MRT Begin/helper allocator faults:"<<failures<<" rolled back,"<<successes<<" retained successes;map combined and model,post-Begin no-allocation checks passed\n";
}
