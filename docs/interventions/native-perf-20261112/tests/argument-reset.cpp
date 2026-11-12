#include "Rendering/Backend/NativeArgumentResetMask.h"
#include <array>
#include <cassert>
#include <cstdint>
using namespace rendering;
int main(){
 constexpr std::array<uint64_t,5> masks{0,2,3,7,0x4f};
 for(auto mask:masks){
  std::array<uint64_t,70> old{},optimized{},fresh{};
  for(unsigned n=0;n<70;++n)old[n]=optimized[n]=n+91;
  for(unsigned n=0;n<70;++n){
   if(NeedsArgumentBufferReset(n,mask))optimized[n]=0;
   if(n<64&&(mask&(uint64_t(1)<<n)))optimized[n]=fresh[n]=n+123;
  }
  assert(optimized==fresh);
 }
 assert(NeedsArgumentBufferReset(64,~uint64_t(0)));
 assert(!NeedsArgumentBufferReset(63,~uint64_t(0)));
}
