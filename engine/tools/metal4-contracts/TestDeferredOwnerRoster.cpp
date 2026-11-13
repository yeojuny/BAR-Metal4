#include "Rendering/Metal4/Metal4GroupedOwnerRoster.h"
#include <cassert>
#include <array>
#include <memory>
#include <functional>
#include <random>
#include <stdexcept>
#include <cstdlib>
#include <new>
using namespace rendering::metal4;
static int failAfter=-1;
void* operator new(std::size_t n){if(failAfter==0)throw std::bad_alloc();if(failAfter>0)--failAfter;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
struct Image{std::shared_ptr<const void> owner;std::function<void(void*)> encodeDependencies;std::function<void()> didComplete;};
struct Draw{std::array<Image,2> images;};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Probe{int reports=0;bool fail=false;void Report(){++reports;if(fail)throw std::runtime_error("probe");}};
struct PreparedEngineModelDeferred {
 struct Impl {
  std::vector<Draw> before,after;TerrainOwnerRoster firstOwners;
  bool dependencies=false,beforeEncoded=false,afterEncoded=false,completed=false;bool ordered=false;size_t orderedEncoded=0;
  std::unique_ptr<Probe> setterProbe176;
  const Draw& At(size_t n)const{Require(n<before.size()+after.size(),"index");return n<before.size()?before[n]:after[n-before.size()];}
 };
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 void EncodeDependencies(void*);
 void DidComplete();
 size_t DrawCount()const;void FinishOrderedDraws();
};
#include "deferred-owner-phases.inc"
void Seal(PreparedEngineModelDeferred& p){p.impl->firstOwners=BuildGroupedOwnerRoster(p.impl->before,p.impl->after);}
void Reference(PreparedEngineModelDeferred& p,bool complete){std::set<const void*> seen;for(auto list:{&p.impl->before,&p.impl->after})for(auto& draw:*list)for(auto& image:draw.images)if(image.owner&&seen.insert(image.owner.get()).second){if(complete){if(image.didComplete)image.didComplete();}else if(image.encodeDependencies)image.encodeDependencies((void*)1);}}
int main(){
 std::mt19937 rng(0xdefe);
 for(int run=0;run<2000;++run){PreparedEngineModelDeferred p;std::vector<int> log,expected;int phase=1;
  std::vector<std::shared_ptr<int>> owners;for(int i=0;i<8;++i)owners.push_back(std::make_shared<int>(i));
  p.impl->before.resize(rng()%16);p.impl->after.resize(rng()%16);int ordinal=0;
  for(auto list:{&p.impl->before,&p.impl->after})for(auto& draw:*list)for(auto& image:draw.images){auto n=rng()%10;if(n<8)image.owner=owners[n];int id=ordinal++;if(rng()%3)image.encodeDependencies=[&,id](void*){log.push_back(phase*1000+id);};if(rng()%3)image.didComplete=[&,id]{log.push_back(phase*1000+id);};}
  Seal(p);assert(log.empty());auto moved=std::move(p.impl->after);p.impl->after=std::move(moved);
  Reference(p,false);expected=log;log.clear();p.EncodeDependencies((void*)1);assert(log==expected&&p.impl->dependencies);
  phase=2;log.clear();Reference(p,true);expected=log;log.clear();p.impl->beforeEncoded=p.impl->afterEncoded=true;p.DidComplete();assert(log==expected&&p.impl->completed);
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}
  try{p.EncodeDependencies((void*)1);assert(false);}catch(const std::runtime_error&){}
 }
 // Cross-list first-empty masks later callbacks; null owners never execute.
 {PreparedEngineModelDeferred p;auto owner=std::make_shared<int>(1);int calls=0;p.impl->before.resize(1);p.impl->after.resize(1);p.impl->before[0].images[0].owner=owner;auto& i=p.impl->after[0].images[0];i.owner=owner;i.encodeDependencies=[&](void*){++calls;};i.didComplete=[&]{++calls;};p.impl->before[0].images[1]=i;p.impl->before[0].images[1].owner.reset();Seal(p);assert(p.impl->firstOwners.size()==1);p.EncodeDependencies((void*)1);p.impl->beforeEncoded=p.impl->afterEncoded=true;p.DidComplete();assert(calls==0);}
 // Failure flags and successful retry remain phase-local.
 for(bool completion:{false,true}){PreparedEngineModelDeferred p;int calls=0;bool fail=true;p.impl->after.resize(1);auto& i=p.impl->after[0].images[0];i.owner=std::make_shared<int>(1);auto cb=[&]{++calls;if(fail)throw std::runtime_error("callback");};if(completion)i.didComplete=cb;else i.encodeDependencies=[&](void*){cb();};Seal(p);p.impl->beforeEncoded=p.impl->afterEncoded=true;
  try{if(completion)p.DidComplete();else p.EncodeDependencies((void*)1);assert(false);}catch(const std::runtime_error&){}assert(calls==1&&!p.impl->completed&&!p.impl->dependencies);fail=false;if(completion)p.DidComplete();else p.EncodeDependencies((void*)1);assert(calls==2);}
 // Existing completion guard does not add a dependencies check; Report follows flag.
 {PreparedEngineModelDeferred p;p.impl->setterProbe176=std::make_unique<Probe>();p.impl->setterProbe176->fail=true;
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(!p.impl->completed);
  p.impl->beforeEncoded=true;try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(!p.impl->completed);
  p.impl->afterEncoded=true;try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(p.impl->completed&&p.impl->setterProbe176->reports==1);
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(p.impl->setterProbe176->reports==1);}
 // Actual ordered finish requires exact draw count and never double-finishes.
 {PreparedEngineModelDeferred p;p.impl->before.resize(1);p.impl->after.resize(2);Seal(p);
  try{p.FinishOrderedDraws();assert(false);}catch(const std::runtime_error&){}
  p.EncodeDependencies((void*)1);p.impl->orderedEncoded=2;try{p.FinishOrderedDraws();assert(false);}catch(const std::runtime_error&){}
  p.impl->orderedEncoded=3;p.FinishOrderedDraws();assert(p.impl->ordered&&p.impl->beforeEncoded&&p.impl->afterEncoded);
  try{p.FinishOrderedDraws();assert(false);}catch(const std::runtime_error&){}p.DidComplete();assert(p.impl->completed);}
 // Same control block / different raw owners remain distinct across groups.
 {PreparedEngineModelDeferred p;auto owner=std::make_shared<std::array<int,2>>();p.impl->before.resize(1);p.impl->after.resize(1);p.impl->before[0].images[0].owner=std::shared_ptr<const void>(owner,&(*owner)[0]);p.impl->after[0].images[0].owner=std::shared_ptr<const void>(owner,&(*owner)[1]);Seal(p);assert(p.impl->firstOwners.size()==2);}
 std::weak_ptr<int> weak;{PreparedEngineModelDeferred p;auto owner=std::make_shared<int>(1);weak=owner;p.impl->after.resize(1);p.impl->after[0].images[0].owner=owner;Seal(p);owner.reset();assert(!weak.expired());}assert(weak.expired());
 // All allocation failures preserve preexisting destination until a full seal.
 {PreparedEngineModelDeferred p;p.impl->before.resize(4);p.impl->after.resize(4);for(auto list:{&p.impl->before,&p.impl->after})for(auto& d:*list)for(auto& i:d.images)i.owner=std::make_shared<int>(1);p.impl->firstOwners={{99,99}};bool success=false;for(int f=0;f<100&&!success;++f){failAfter=f;try{Seal(p);failAfter=-1;success=true;}catch(const std::bad_alloc&){failAfter=-1;assert((p.impl->firstOwners==TerrainOwnerRoster{{99,99}}));}}assert(success&&p.impl->firstOwners.size()==16);}
}
