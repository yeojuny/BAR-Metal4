#include "Rendering/Backend/NativeIdleResourcePool.h"
#include <cassert>
#include <algorithm>
#include <iostream>
using namespace rendering;
struct Resource {unsigned key,generation;};
struct Frame {std::shared_ptr<const Resource> backing;};
int main(){
 NativeIdleResourcePool<Resource,2> pool;unsigned generation=0,allocations=0;
 auto acquire=[&](unsigned key,int fail=0){
  std::shared_ptr<Frame> frame;
  auto selected=pool.Acquire([&](const Resource& r){return r.key==key;},[&]{
   if(fail==1)throw std::bad_alloc();++allocations;
   return std::make_shared<Resource>(Resource{key,generation+1});
  },[&](const auto& r){auto prepared=std::make_shared<Frame>(Frame{r});if(fail==2)throw std::bad_alloc();frame=std::move(prepared);});
  generation=std::max(generation,selected->generation);return frame;
 };
 auto first=acquire(1);auto firstAddress=first->backing.get();
 auto second=acquire(1);assert(second->backing!=first->backing&&allocations==2);
 auto third=acquire(1);assert(third->backing!=first->backing&&third->backing!=second->backing&&allocations==3);
 auto copied=std::make_shared<Frame>(*first);first.reset();
 auto fourth=acquire(1);assert(fourth->backing!=copied->backing);fourth.reset();third.reset();
 copied.reset();first=acquire(1);assert(first->backing.get()==firstAddress);
 second.reset();auto count=allocations;
 for(unsigned i=0;i<1000;++i){auto next=acquire(1);assert(next->backing!=first->backing);first=std::move(next);}
 assert(allocations==count);
 first.reset();auto stableGeneration=generation;
 for(unsigned i=0;i<1000;++i)for(int failure:{1,2}){
  try{acquire(2,failure);assert(false);}catch(const std::bad_alloc&){}
  assert(generation==stableGeneration);
  auto unchanged=acquire(1);assert(unchanged->backing->key==1&&generation==stableGeneration);
 }
 for(unsigned i=0;i<1000;++i){
  try{acquire(1,2);assert(false);}catch(const std::bad_alloc&){}
  assert(generation==stableGeneration);
 }
 // Compatible idle slot must win over an earlier idle mismatched victim.
 pool.Clear();auto a=acquire(2);auto b=acquire(3);auto bp=b->backing.get();a.reset();b.reset();count=allocations;
 auto reused=acquire(3);assert(reused->backing.get()==bp&&allocations==count);
 auto retained=reused->backing;pool.Clear();assert(retained->key==3);
 auto replacement=acquire(3);assert(replacement->backing!=retained);
 // A pinned mismatched cache entry may not be evicted/reused.
 auto other=acquire(4);auto extra=acquire(5);assert(extra->backing!=other->backing&&retained->key==3);
 std::cout<<"Idle pool:1000 alternating frames stabilize at2 cached allocations;3000 factory/retention failures;copied/continuation pins,fallback,compatibility,clear passed\n";
}
