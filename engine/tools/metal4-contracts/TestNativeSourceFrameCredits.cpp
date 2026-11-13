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
 // Failure follows capture ordinal, not draw numbering, and keeps held owners.
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits pool;
  auto old=pool.Acquire(900),young=pool.Acquire(2); // Decreasing draw ID is intentional.
  std::weak_ptr<const NativeSourceFrameCredits::Token> oldLife=old;
  pool.Hold(old);pool.QuarantineHeld(900);pool.QuarantineHeld(900);
  Reject([&]{pool.Validate(old,900);});Reject([&]{pool.Validate(young,2);});
  Reject([&]{pool.Hold(young);});Reject([&]{pool.ValidateHeld(900);});
  old.reset();young.reset();assert(!oldLife.expired());
  Reject([&]{pool.HasCapacity();});Reject([&]{pool.Acquire(1);});
  // Scene replacement creates a new lineage; old/foreign credits still reject.
  NativeSourceFrameCredits fresh;auto restarted=fresh.Acquire(900);fresh.Validate(restarted,900);
  Reject([&]{fresh.Validate(oldLife.lock(),900);});
 }
 // A younger failure does not invalidate a genuinely older success receipt.
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits pool;auto old=pool.Acquire(42),young=pool.Acquire(0);
  pool.Hold(old);pool.Hold(young);pool.QuarantineHeld(0);
  pool.Validate(old,42);pool.ValidateHeld(42);pool.ReleaseHeld(42);
  Reject([&]{pool.Validate(young,0);});Reject([&]{pool.ValidateHeld(0);});
  Reject([&]{pool.Acquire(43);});
 }
 std::cout<<"3000 source-budget/lineage cycles: capturing+GPU held, CPU-release not retirement, stale/foreign/repeated identities, foreign thread, retained completed CPU owner PASS\n";
}
