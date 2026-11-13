#include "Rendering/Backend/NativeSourceContextSnapshot.h"
#include <cassert>
#include <iostream>
#include <type_traits>
using rendering::NativeSourceContextSnapshot;
// Controlled adapter, NOT the actual engine context or a Metal runtime test.
struct Context {
 bool live=true;uint64_t generation=7,name5=99;uint32_t width=80,height=60;
 unsigned initializedCalls=0,registryCalls=0,geometryCalls=0;mutable unsigned nameCalls=0,widthCalls=0,heightCalls=0;
 bool Initialized(){++initializedCalls;return live;}
 Context& Registry(){++registryCalls;return *this;}
 uint64_t ContextGeneration()const{return generation;}
 Context& Geometry(int role){assert(role==2);++geometryCalls;return *this;}
 uint64_t Name(unsigned n)const{assert(n==5);++nameCalls;return name5;}
 uint32_t Width()const{++widthCalls;return width;}
 uint32_t Height()const{++heightCalls;return height;}
};
template<class F>void Reject(F f){try{f();assert(false);}catch(const std::runtime_error&) {}}
int main(){
 static_assert(std::is_trivially_copyable_v<NativeSourceContextSnapshot>);
 for(unsigned i=0;i<1000;++i){
  Context c;const auto captured=NativeSourceContextSnapshot::Capture(c,2);
  assert(c.initializedCalls==1&&c.registryCalls==1&&c.geometryCalls==1&&c.nameCalls==1&&c.widthCalls==1&&c.heightCalls==1);
  c.live=false;c.generation=10;c.name5=0;c.width=20;c.height=30;
  for(unsigned n=0;n<10;++n){auto copy=captured;assert(copy.ContextGeneration()==7);const auto m=copy.Models();assert(m.depthPresent&&m.width==80&&m.height==60);}
  assert(c.registryCalls==1&&c.geometryCalls==1&&c.nameCalls==1);
  const auto absent=NativeSourceContextSnapshot::Capture(c,2);
  assert(c.registryCalls==1&&c.geometryCalls==1);Reject([&]{absent.ContextGeneration();});Reject([&]{absent.Models();});
  c.live=true;c.generation=0;const auto invalid=NativeSourceContextSnapshot::Capture(c,2);
  Reject([&]{invalid.ContextGeneration();});Reject([&]{invalid.Models();});
  c.generation=8;c.width=c.height=0;const auto empty=NativeSourceContextSnapshot::Capture(c,2);
  assert(empty.ContextGeneration()==8&&!empty.Models().depthPresent&&empty.Models().width==0&&empty.Models().height==0);
 }
 std::cout<<"Production snapshot template with controlled adapter1000:copied values,depth index5,no later getters,absent/zero-context rejection,empty geometry preserved; not engine runtime\n";
}
