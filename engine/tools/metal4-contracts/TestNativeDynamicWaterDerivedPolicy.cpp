#include "Rendering/Backend/NativeDynamicWaterDerivedPolicy.h"
#include <cassert>
#include <iostream>
#include <utility>
using namespace rendering;
static DynamicWaterDraw Draw(DynamicWaterPass pass) { DynamicWaterDraw d; d.pass=pass; return d; }
template<class F> static void Reject(F&& f) {
 bool rejected=false;
 try { std::forward<F>(f)(); } catch(const std::logic_error&) { rejected=true; }
 assert(rejected);
}
int main() {
 for(unsigned n=0;n<1000;++n) {
  NativeDynamicWaterFrame f;
  f.derived={Draw(DynamicWaterPass::DerivedHeight),Draw(DynamicWaterPass::DetailNormal)};
  f.initializesDetail=true;
  auto update=std::make_shared<NativeDynamicWaterUpdate>();
  update->draws={Draw(DynamicWaterPass::DetailNormal),Draw(DynamicWaterPass::Flow)};
  f.updates.push_back(update);
  const auto* derivedData=f.derived.data();
  const auto* updateData=update->draws.data();
  // Captured before any commit: failed/pre-Begin predecessor cannot suppress initialization.
  assert(NativeDynamicWaterDerivedCount(f,false)==2);
  assert(NativeDynamicWaterDerivedCount(f,false)==2);
  // A later committed exact-material predecessor supplies copied detail pixels.
  assert(NativeDynamicWaterDerivedCount(f,true)==1);
  assert(NativeDynamicWaterDerivedCount(f,false)==2);
  assert(f.initializesDetail && f.derived.size()==2 && f.derived.data()==derivedData);
  assert(f.updates.front()==update && update->draws.data()==updateData);
  assert(update->draws.size()==2 && update->draws.front().pass==DynamicWaterPass::DetailNormal);
  auto ordinary=f; ordinary.initializesDetail=false; ordinary.derived.resize(1);
  assert(NativeDynamicWaterDerivedCount(ordinary,true)==1);
  // Lack of bootstrap is not repaired here: renderer's detailReady check remains authoritative.
  assert(NativeDynamicWaterDerivedCount(ordinary,false)==1);
  auto bad=f; bad.initializesDetail=false;
  Reject([&]{NativeDynamicWaterDerivedCount(bad,true);});
  bad=f; bad.derived.pop_back();
  Reject([&]{NativeDynamicWaterDerivedCount(bad,false);});
  bad=f; bad.derived.clear();
  Reject([&]{NativeDynamicWaterDerivedCount(bad,false);});
  bad=f; std::swap(bad.derived[0],bad.derived[1]);
  Reject([&]{NativeDynamicWaterDerivedCount(bad,true);});
  bad=f; bad.derived.push_back(Draw(DynamicWaterPass::DetailNormal));
  Reject([&]{NativeDynamicWaterDerivedCount(bad,true);});
  bad=f; bad.derived[1].pass=DynamicWaterPass::HeightFoam;
  Reject([&]{NativeDynamicWaterDerivedCount(bad,false);});
  bad=ordinary; bad.derived[0].pass=DynamicWaterPass::DetailNormal;
  Reject([&]{NativeDynamicWaterDerivedCount(bad,true);});
 }
 std::cout<<"Actual derived bootstrap policy: 1000 retry/committed-prefix/immutable update/malformed checks PASS\n";
}
