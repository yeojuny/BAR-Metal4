#include <algorithm>
#include "Rendering/Metal4/Metal4TerrainOwnerRoster.h"
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
struct Image {
 std::shared_ptr<const void> owner;
 std::function<void(void*)> encodeDependencies;
 std::function<void()> didComplete,requireReadable;
};
struct Draw{std::vector<Image> images;bool encoded=true;};
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct PreparedLeaf {
 struct Impl {
  std::vector<Draw> draws;TerrainOwnerRoster firstOwners;
  bool dependencies=false,completed=false;
 };
 std::unique_ptr<Impl> impl=std::make_unique<Impl>();
 void EncodeDependencies(void* command);
 void DidComplete();
};
#include "leaf-owner-phases.inc"
void Reference(const std::vector<Draw>& draws,bool complete){
 std::set<const void*> seen;
 for(const auto& draw:draws)for(const auto& image:draw.images)
  if(seen.insert(image.owner.get()).second){if(complete){if(image.didComplete)image.didComplete();}else if(image.encodeDependencies)image.encodeDependencies(reinterpret_cast<void*>(1));}
}
int main(){
 std::mt19937 random(0x161);
 for(unsigned cycle=0;cycle<2000;++cycle){
  PreparedLeaf p;auto& state=*p.impl;std::vector<int> log,expected;
  std::vector<std::shared_ptr<int>> owners;for(int i=0;i<8;++i)owners.push_back(std::make_shared<int>(i));
  int dynamic=1,readable=0;
  state.draws.resize(random()%16);
  int ordinal=0;
  for(auto& draw:state.draws){draw.images.resize(random()%12);for(auto& image:draw.images){
   const unsigned owner=random()%10;if(owner<8)image.owner=owners[owner];
   const int n=ordinal++;
   if(random()%3)image.encodeDependencies=[&,n](void*){log.push_back(dynamic*10000+n);};
   if(random()%3)image.didComplete=[&,n]{log.push_back(dynamic*10000+n);};
   image.requireReadable=[&]{++readable;};
  }}
  state.firstOwners=BuildTerrainOwnerRoster(state.draws);assert(log.empty()&&readable==0);
  // Moves after sealing do not invalidate index-based identity.
  auto moved=std::move(state.draws);state.draws=std::move(moved);
  Reference(state.draws,false);expected=log;log.clear();p.EncodeDependencies(reinterpret_cast<void*>(1));assert(log==expected);
  dynamic=2;log.clear();Reference(state.draws,true);expected=log;log.clear();p.DidComplete();assert(log==expected);
  assert(state.dependencies&&state.completed&&readable==0);
  try{p.EncodeDependencies(reinterpret_cast<void*>(1));assert(false);}catch(const std::runtime_error&){}
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}
 }
 // First empty callback masks later callbacks, including a nullptr owner.
 {PreparedLeaf p;int calls=0;auto owner=std::make_shared<int>(0);p.impl->draws.resize(2);
  p.impl->draws[0].images={{owner},{}};
  Image later;later.owner=owner;later.encodeDependencies=[&](void*){++calls;};later.didComplete=[&]{++calls;};
  p.impl->draws[1].images.push_back(later);later.owner.reset();p.impl->draws[1].images.push_back(later);
  p.impl->firstOwners=BuildTerrainOwnerRoster(p.impl->draws);p.EncodeDependencies(reinterpret_cast<void*>(1));p.DidComplete();assert(calls==0);
 }
 // Original failure leaves dependencies false and permits retry.
 {PreparedLeaf p;int calls=0;bool fail=true;Image image;image.owner=std::make_shared<int>(1);image.encodeDependencies=[&](void*){++calls;if(fail)throw std::runtime_error("encode");};
  p.impl->draws={{{image}}};p.impl->firstOwners=BuildTerrainOwnerRoster(p.impl->draws);
  try{p.EncodeDependencies(reinterpret_cast<void*>(1));assert(false);}catch(const std::runtime_error&){}
  assert(!p.impl->dependencies&&calls==1);fail=false;p.EncodeDependencies(reinterpret_cast<void*>(1));assert(p.impl->dependencies&&calls==2);
 }
 // Pointer identity, not shared control-block ownership, is the existing key.
 {auto array=std::make_shared<std::array<int,2>>();
  std::shared_ptr<const void> first(array,&(*array)[0]),second(array,&(*array)[1]);
  std::vector<Draw> aliased={{{Image{first},Image{second},Image{first}}}};
  assert(BuildTerrainOwnerRoster(aliased).size()==2);
 }
 // Retirement still requires dependencies and every actual draw encoded.
 {PreparedLeaf p;p.impl->draws.resize(1);p.impl->draws[0].encoded=false;
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(!p.impl->completed);
  p.impl->dependencies=true;
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(!p.impl->completed);
  p.impl->draws[0].encoded=true;p.DidComplete();assert(p.impl->completed);
 }
 // Completion failures retain original flag/order semantics: no false success.
 {PreparedLeaf p;int calls=0;bool fail=true;Image image;
  image.owner=std::make_shared<int>(1);image.didComplete=[&]{++calls;if(fail)throw std::runtime_error("complete");};
  p.impl->draws={{{image}}};p.impl->firstOwners=BuildTerrainOwnerRoster(p.impl->draws);p.impl->dependencies=true;
  try{p.DidComplete();assert(false);}catch(const std::runtime_error&){}assert(!p.impl->completed&&calls==1);
  fail=false;p.DidComplete();assert(p.impl->completed&&calls==2);
 }
 // Indices introduce no extra ownership and survive temporary roster lifetime.
 std::weak_ptr<int> weak;
 {PreparedLeaf p;auto owner=std::make_shared<int>(1);weak=owner;p.impl->draws={{{Image{owner}}}};p.impl->firstOwners=BuildTerrainOwnerRoster(p.impl->draws);owner.reset();assert(!weak.expired());}
 assert(weak.expired());
 // Every allocation boundary in the census fails before replacing an old roster.
 std::vector<Draw> draws(4);for(auto& draw:draws)for(int n=0;n<6;++n)draw.images.push_back(Image{std::make_shared<int>(n)});
 TerrainOwnerRoster target{{99,99}};bool succeeded=false;
 for(int failure=0;failure<100&&!succeeded;++failure){
  failAfter=failure;
  try{auto complete=BuildTerrainOwnerRoster(draws);failAfter=-1;target=std::move(complete);succeeded=true;}
  catch(const std::bad_alloc&){failAfter=-1;assert((target==TerrainOwnerRoster{{99,99}}));}
 }
 assert(succeeded&&target.size()==24);
}
