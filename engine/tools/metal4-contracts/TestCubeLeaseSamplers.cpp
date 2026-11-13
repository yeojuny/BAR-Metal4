#include <array>
#include <optional>
#include <memory>
#include <cassert>
#include <limits>
#include <iostream>
#include "Rendering/Backend/NativeTextureBacking.h"
using rendering::NativeTextureSampling;
using Sampler=std::shared_ptr<int>;
constexpr auto nil=nullptr;
#include "cube-lease-samplers.inc"
int main(){
 NativeTextureSampling p;
 std::weak_ptr<int> held;
 {
  CubeLeaseSamplers cache;
  assert(!cache.Find(p));cache.Remember(p,nil);assert(!cache.Find(p));
  auto sampler=std::make_shared<int>(1);held=sampler;cache.Remember(p,sampler);
  for(int i=0;i<2000;++i)assert(cache.Find(p)==sampler);
  sampler.reset();assert(!held.expired());
  auto changed=p;changed.wrapR=0x812f;assert(!cache.Find(changed));
  changed=p;changed.maxLod=12;assert(!cache.Find(changed));
  changed=p;changed.compareFunction++;assert(!cache.Find(changed));
  changed=p;changed.swizzle[0]++;assert(!cache.Find(changed));
  for(int i=1;i<32;++i){changed=p;changed.maxLevel=i;cache.Remember(changed,std::make_shared<int>(i));assert(*cache.Find(changed)==i);}
  changed=p;changed.maxLevel=33;cache.Remember(changed,std::make_shared<int>(33));assert(!cache.Find(changed));assert(*cache.Find(p)==1);
  CubeLeaseSamplers other;assert(!other.Find(p));
 }
 assert(held.expired());
 CubeLeaseSamplers cache;auto nan=p;nan.minLod=std::numeric_limits<float>::quiet_NaN();cache.Remember(nan,std::make_shared<int>(1));assert(!cache.Find(nan));
 std::cout<<"2000 exact hits, complete sampling key, 32-entry saturation, independent leases, nil/NaN misses and strong lifetime PASS\n";
}
