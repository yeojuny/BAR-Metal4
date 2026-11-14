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
 // Capacity three is explicit and remains bounded. Model GPU N-1, CPU N,
 // and capture N+1 without turning a CPU join into GPU retirement.
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits pool(3);
  auto gpu=pool.Acquire(90),worker=pool.Acquire(4),capture=pool.Acquire(2);
  pool.Hold(gpu);pool.Hold(worker);
  assert(!pool.HasCapacity());Reject([&]{pool.Acquire(1);});
  Reject([&]{pool.Acquire(4);});Reject([&]{pool.Hold(worker);});
  gpu.reset();worker.reset();assert(!pool.HasCapacity());
  pool.ValidateHeld(90);pool.ReleaseHeld(90);assert(pool.HasCapacity());
  auto newest=pool.Acquire(1);assert(!pool.HasCapacity());
  pool.Validate(capture,2);pool.Validate(newest,1);
  pool.ValidateHeld(4);pool.ReleaseHeld(4);assert(pool.HasCapacity());
  pool.Hold(capture);capture.reset();pool.ValidateHeld(2);pool.ReleaseHeld(2);
  newest.reset();assert(pool.HasCapacity());
  NativeSourceFrameCredits other(3);auto foreign=other.Acquire(1);
  Reject([&]{pool.Validate(foreign,1);});
  std::async(std::launch::async,[&]{Reject([&]{pool.HasCapacity();});Reject([&]{pool.Acquire(9);});}).get();
 }
 // Middle-frame failure quarantines its suffix by ordinal even when draw IDs
 // decrease; the older GPU receipt may still retire, but admission stays shut.
 for(unsigned i=0;i<1000;++i){
  NativeSourceFrameCredits pool(3);
  auto old=pool.Acquire(900),middle=pool.Acquire(2),young=pool.Acquire(0);
  pool.Hold(old);pool.Hold(middle);pool.Hold(young);
  std::weak_ptr<const NativeSourceFrameCredits::Token> middleLife=middle,youngLife=young;
  pool.QuarantineHeld(2);pool.QuarantineHeld(2);
  pool.Validate(old,900);pool.ValidateHeld(900);pool.ReleaseHeld(900);old.reset();
  Reject([&]{pool.Validate(middle,2);});Reject([&]{pool.Validate(young,0);});
  Reject([&]{pool.ValidateHeld(2);});Reject([&]{pool.ValidateHeld(0);});
  middle.reset();young.reset();assert(!middleLife.expired()&&!youngLife.expired());
  Reject([&]{pool.HasCapacity();});Reject([&]{pool.Acquire(1);});
 }
 for(unsigned invalid:{0u,1u,4u,std::numeric_limits<unsigned>::max()})
  Reject([&]{NativeSourceFrameCredits invalidPool(invalid);});
 std::cout<<"5000 default-two/opt-in-three source-budget/lineage cycles: capturing+GPU held, CPU-release not retirement, stale/foreign/repeated identities, foreign thread, retained completed CPU owner PASS\n";
}
