#include "Rendering/Backend/NativeSourceResourceCache.h"
#include <cassert>
#include <iostream>
#include <vector>
using namespace rendering;
struct Source {std::vector<float> heights;};
struct Resource {std::shared_ptr<const Source> source;std::vector<float> buffer,normals;};
int main(){
 NativeSourceResourceCache<Source,Resource> cache;unsigned calls=0;
 auto first=std::make_shared<Source>();first->heights={1,2,3};
 const auto build=[&](const auto& source){++calls;auto r=std::make_shared<Resource>();r->source=source;r->buffer=source->heights;r->normals.resize(source->heights.size());return r;};
 auto old=cache.GetOrCreate(first,build);assert(calls==1);assert(cache.GetOrCreate(first,build)==old&&calls==1);
 auto second=std::make_shared<Source>();second->heights={7,8,9};
 for(unsigned i=0;i<1000;++i){
  for(unsigned boundary=0;boundary<3;++boundary){
   try{cache.GetOrCreate(second,[&](const auto& source)->std::shared_ptr<Resource>{if(boundary==0)throw std::bad_alloc();auto r=std::make_shared<Resource>();r->source=source;r->buffer=source->heights;if(boundary==1)throw std::bad_alloc();r->normals.resize(source->heights.size());return {};});assert(false);}catch(const std::exception&){}
   assert(cache.GetOrCreate(first,build)==old);assert(old->buffer==first->heights);
  }
 }
 // The actual cache transaction also gates companion tile publication on
 // successful retention, including when the height source itself is unchanged.
 unsigned companion=0,prepared=0;
 for(unsigned i=0;i<1000;++i){
  for(bool cacheHit:{false,true}){
   try{cache.GetOrCreatePrepared(cacheHit?first:second,build,[&](const auto&){++prepared;throw std::bad_alloc();},[&]() noexcept {++companion;});assert(false);}catch(const std::bad_alloc&){}
   assert(companion==0);assert(cache.GetOrCreate(first,build)==old);
  }
 }
 assert(prepared==2000);
 auto same=cache.GetOrCreatePrepared(first,build,[&](const auto& r){assert(r==old);},[&]() noexcept {++companion;});
 assert(same==old&&companion==1);
 auto replacement=cache.GetOrCreatePrepared(second,build,[&](const auto& r){assert(r!=old&&r->buffer==second->heights);},[&]() noexcept {++companion;});
 assert(companion==2&&replacement!=old);
 auto newer=cache.GetOrCreate(second,build);assert(newer!=old&&newer->buffer==second->heights&&old->buffer==first->heights);assert(newer->buffer.data()!=old->buffer.data());
 std::weak_ptr<const Source> oldSource=first;first.reset();assert(!oldSource.expired());
 cache.Clear();assert(newer->buffer==second->heights&&old->buffer[0]==1);auto recreated=cache.GetOrCreate(second,build);assert(recreated!=newer);
 old.reset();same.reset();assert(oldSource.expired());
 std::cout<<"Source resource cache:2000 failed preparations and companion cache-hit/replacement commits;3000 failed pair creations preserve prior owner;reuse/same-size version isolation/retained source/clear-recreate passed\n";
}
