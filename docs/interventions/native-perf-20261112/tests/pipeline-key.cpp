#include "Rendering/Backend/NativeDeferredLightPipelineKey.h"
#include <cassert>
#include <limits>
#include <map>
#include <random>
#include <vector>
using namespace rendering;
int main(){
 std::mt19937_64 rng(20261112);
 std::map<std::vector<uint64_t>,unsigned> oldCache;
 std::map<DeferredLightPipelineKey,unsigned> newCache;
 for(unsigned it=0;it<10000;++it){
  std::array<uint64_t,12> state{};for(auto&v:state)v=rng();
  std::array<bool,4>mask{};for(unsigned i=0;i<4;++i)mask[i]=(it>>i)&1;
  auto sample=it==0?std::numeric_limits<uint64_t>::max():rng();bool coverage=(it>>4)&1;
  auto key=MakeDeferredLightPipelineKey(state,mask,sample,coverage);
  std::vector<uint64_t>old(state.begin(),state.end());for(bool m:mask)old.push_back(m);
  old.push_back(sample);old.push_back(coverage);
  assert(std::equal(key.begin(),key.end(),old.begin(),old.end()));
  for(unsigned field=0;field<key.size();++field){auto k=key;auto o=old;k[field]^=1;o[field]^=1;
   assert(k!=key);assert((k<key)==(o<old));}
  oldCache.emplace(old,it);newCache.emplace(key,it);assert(oldCache.at(old)==newCache.at(key));
 }
 auto a=oldCache.begin();auto b=newCache.begin();for(;a!=oldCache.end();++a,++b){
  assert(std::equal(a->first.begin(),a->first.end(),b->first.begin(),b->first.end()));assert(a->second==b->second);}
}
