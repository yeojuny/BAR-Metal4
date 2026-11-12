/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>
namespace rendering::metal4 {
struct CoveragePoolKey220 {
 std::uintptr_t device=0;
 std::uint64_t width=0,height=0,format=0;
 bool operator==(const CoveragePoolKey220&)const=default;
};
// This cache contains ONLY expired logical versions with a successful producer
// receipt. Every writer and reader must pin its logical owner in the runtime's
// submission graph. The weak owner is not, on its own, a GPU completion fence.
template<class Image,std::size_t MaxSlots=16,std::uint64_t MaxBytes=(128ull<<20)>
class CoverageRetiredPool220 {
 struct Slot {CoveragePoolKey220 key;Image image;std::uint64_t bytes;};
 std::vector<Slot> idle;
 std::uint64_t bytes=0;
public:
 template<class Owner>
 bool Recycle(CoveragePoolKey220 key,Image image,std::uint64_t size,
  const std::weak_ptr<Owner>& owner,bool producerCompleted){
  if(!owner.expired()||!producerCompleted||!size||size>MaxBytes)return false;
  // Evict only already-idle storage. Busy owners never enter this container.
  while(!idle.empty()&&(idle.size()>=MaxSlots||size>MaxBytes-bytes)){
   bytes-=idle.front().bytes;idle.erase(idle.begin());
  }
  if(idle.size()>=MaxSlots||size>MaxBytes-bytes)return false;
  idle.push_back({key,std::move(image),size});bytes+=size;return true;
 }
 Image Acquire(CoveragePoolKey220 key){
  for(auto i=idle.begin();i!=idle.end();++i)if(i->key==key){
   auto result=std::move(i->image);bytes-=i->bytes;idle.erase(i);return result;
  }
  return {};
 }
 std::size_t Size()const{return idle.size();}
 std::uint64_t Bytes()const{return bytes;}
};
}
