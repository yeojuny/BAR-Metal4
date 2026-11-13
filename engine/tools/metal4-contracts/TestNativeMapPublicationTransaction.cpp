#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
static thread_local long failAfter=-1;
void* operator new(std::size_t n){if(failAfter>=0&&failAfter--==0){failAfter=-1;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace rendering;
int main(){
 unsigned failures=0,successes=0;
 for(bool reuseNormal:{false,true})for(int fault=-1;fault<40;++fault){
  NativeCUSTextureHistory h;NativeTextureRegistry r(h);r.NewContext();h.ActiveTexture(0);
  NativeMapTexturePublication map;map.Attach(r,h);int owner;
  map.Create(&owner,NativeMapTextureRole::Normals,8,8);map.Create(&owner,NativeMapTextureRole::Height,8,8);
  map.HeightUploaded(&owner);map.NormalsRendered(&owner);
  auto heights=std::make_shared<WorldTerrainHeights>();heights->width=heights->height=8;
  auto before=map.Publish(heights,1);
  map.HeightUploaded(&owner);
  if(!reuseNormal){map.NormalsRendered(&owner);heights=std::make_shared<WorldTerrainHeights>(*heights);}
  std::shared_ptr<const NativeMapTextureFrame> after;
  bool threw=false;failAfter=fault;
  try{after=map.Publish(heights,2);}catch(const std::bad_alloc&){threw=true;}
  failAfter=-1;
  const auto normal=map.Lookup("$normals").object,height=map.Lookup("$heightmap").object;
  if(threw){
   ++failures;assert(!after);
   assert(normal->backing==before->images[0]->backing && height->backing==before->images[1]->backing);
   // Retry sees restored contents, not a silently retained half publication.
   after=map.Publish(heights,2);
  }else ++successes;
  assert(after && after->images[1]->backing!=before->images[1]->backing);
  assert((after->images[0]->backing==before->images[0]->backing)==reuseNormal);
  auto same=map.Publish(heights,3);
  assert(same->images[0]==after->images[0] && same->images[1]==after->images[1]);
  map.Destroy(&owner);map.Detach();
 }
 assert(failures>=6 && successes>0);
 std::cout<<"Actual map pair publication: "<<failures<<" allocation failures restored, "<<successes<<" successes; retry and reused-normal preservation PASS\n";
}
