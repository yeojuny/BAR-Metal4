#include "Rendering/Backend/NativeSourceFrameCredits.h"
#include <cassert>
#include <iostream>
#include <future>
using namespace rendering;
template<class F>void Reject(F&& f){bool caught=false;try{f();}catch(const std::logic_error&){caught=true;}assert(caught);}
static_assert(!std::is_copy_constructible_v<NativeSourceFrameCredits> && !std::is_move_constructible_v<NativeSourceFrameCredits>);
int main(){
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits pool;auto first=pool.Acquire(0);auto second=pool.Acquire(1);
  assert(!pool.HasCapacity());Reject([&]{pool.Acquire(2);});Reject([&]{pool.Acquire(1);});
  pool.Validate(first,0);Reject([&]{pool.Validate(first,1);});pool.Hold(first);Reject([&]{pool.Hold(first);});
  first.reset();assert(!pool.HasCapacity()); // CPU destruction is not GPU credit release.
  pool.ValidateHeld(0);pool.ReleaseHeld(0);assert(pool.HasCapacity());auto third=pool.Acquire(2);
  assert(!pool.HasCapacity());second.reset();assert(pool.HasCapacity());
  pool.Hold(third);pool.ValidateHeld(2);pool.ReleaseHeld(2);assert(pool.HasCapacity());
  auto fourth=pool.Acquire(3);assert(!pool.HasCapacity()); // third still retained by CPU owner.
  third.reset();fourth.reset();assert(pool.HasCapacity());
  NativeSourceFrameCredits other;auto foreign=other.Acquire(0);Reject([&]{pool.Validate(foreign,0);});
  std::async(std::launch::async,[&]{Reject([&]{pool.Acquire(4);});}).get();
 }
 std::cout<<"1000 source-budget2 cycles: capturing+GPU held, CPU-release not retirement, stale/foreign/repeated identities, foreign thread, retained completed CPU owner PASS\n";
}
