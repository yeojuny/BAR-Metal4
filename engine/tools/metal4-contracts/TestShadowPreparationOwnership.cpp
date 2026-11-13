#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
struct Runtime{};
struct Draw {std::array<int,2> texSquare{};bool indexed=false;std::vector<std::array<float,3>> positions;std::vector<uint32_t> indices;const auto& Positions()const{return positions;}const auto& Indices()const{return indices;}};
struct NativeTerrainShadowFrame {bool drawInvoked=true;uint64_t contextGeneration=1,sourceDrawFrame=1;std::vector<Draw> draws;};
struct ShadowTerrainGeometryBuffers {std::shared_ptr<const int> owner;};
void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct ShadowTerrainGeometryCache {
 struct Statistics{size_t hits=0,misses=0;};
 struct Key{int x,y;bool indexed;size_t ordinal;};
 int serial=0;
 ShadowTerrainGeometryBuffers Acquire(const Runtime*,const void*,uint64_t,Key,const Draw&,Statistics* stats){++stats->misses;return {std::make_shared<const int>(++serial)};}
};
#include "preparation.inc"
int main(){
 Runtime runtime,other;int device,otherDevice;ShadowTerrainGeometryCache cache;
 for(int n=0;n<1000;++n){
  auto a=std::make_shared<NativeTerrainShadowFrame>();a->draws.resize(2);a->sourceDrawFrame=1;
  auto b=std::make_shared<NativeTerrainShadowFrame>(*a);b->sourceDrawFrame=2;
  auto pa=ShadowTerrainPreparation::Prepare(cache,&runtime,&device,a,false);
  auto pb=ShadowTerrainPreparation::Prepare(cache,&runtime,&device,b,false);
  auto retained=pa->geometry.front().owner;std::weak_ptr<const int> weak=retained;retained.reset();
  assert(pa->Matches(&runtime,&device,a));assert(pb->Matches(&runtime,&device,b));
  assert(!pa->Matches(&runtime,&device,b));assert(!pb->Matches(&runtime,&device,a));
  assert(!pa->Matches(&other,&device,a));assert(!pa->Matches(&runtime,&otherDevice,a));
  std::shared_ptr<const NativeTerrainShadowFrame> forged(a.get(),[](auto*){});
  assert(!pa->Matches(&runtime,&device,forged));
  // Request A release cannot replace B; no ambient weak latest slot exists.
  pa.reset();assert(weak.expired());assert(pb->Matches(&runtime,&device,b));
  auto changed=std::make_shared<ShadowTerrainPreparation>(*pb);changed->geometry.pop_back();assert(!changed->Matches(&runtime,&device,b));
  std::shared_ptr<const ShadowTerrainPreparation> missing;
  assert(!(missing&&missing->Matches(&runtime,&device,b))); // production cache fallback
 }
}
