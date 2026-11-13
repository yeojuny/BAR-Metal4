#include "Rendering/Backend/NativeSerialRenderWorker.h"
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
thread_local int allocationBudget=-1;
void* operator new(std::size_t size){
 if(allocationBudget==0)throw std::bad_alloc();
 if(allocationBudget>0)--allocationBudget;
 if(auto p=std::malloc(size?size:1))return p;
 throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
int main(){
 rendering::NativeSerialRenderWorker worker;std::atomic<unsigned> calls=0;unsigned failed=0;
 for(int point=0;point<16;++point){
  const auto before=calls.load();allocationBudget=point;bool rejected=false;
  try{assert(worker.Invoke([&]{++calls;return 7;})==7);}catch(const std::bad_alloc&){rejected=true;++failed;}
  allocationBudget=-1;
  // No source work may run if constructing/publication failed.
  assert(calls==before+(rejected?0:1));
  // A failed admission cannot leak the sole busy slot.
  assert(worker.Invoke([&]{++calls;return 9;})==9);
 }
 assert(failed>=3);
 for(unsigned i=0;i<1000;++i){
  try{worker.Invoke([]{throw std::runtime_error("worker failure");});assert(false);}catch(const std::runtime_error&){}
  assert(worker.Invoke([]{return 11;})==11);
 }
 std::cout<<"Worker publication allocation failures="<<failed<<";1000 exception/recovery pairs passed\n";
}
