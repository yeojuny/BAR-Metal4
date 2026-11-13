#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <cassert>
#include <cstdio>
#include "Rendering/Backend/NativeCPUPhaseTrace.h"
#include "Rendering/Backend/NativeSubmissionTrace.h"
#include "Rendering/Backend/NativeSerialRenderWorker.h"
static std::vector<std::string> logs;
#define LOG(format,text) logs.emplace_back(text)
namespace rendering::metal4 {
static std::uint64_t SubmissionTraceNow() noexcept {
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
#include "cpu-phase-runtime.inc"
}
using namespace rendering::metal4;
int main() {
 rendering::NativeSerialRenderWorker worker;
 unsigned sampled=0;
 for(int i=1;i<=100;++i){
  NativeCPUPhaseScope loop(1,i);
  NativeCPUPhaseScope capture(2,i);
  const auto context=CaptureNativeCPUPhaseContext();
  assert(context.sampled==loop.active);
  if(loop.active)++sampled;
  worker.Invoke([&]{
   assert(!CaptureNativeCPUPhaseContext().sampled);
   {NativeCPUPhaseContextScope transferred(context);
    assert(CaptureNativeCPUPhaseContext().parent==context.parent);
    NativeCPUPhaseScope encode(3,i);
    assert(encode.active==loop.active);
    if(encode.active){
     const auto local=CaptureNativeCPUPhaseContext();
     NativeCPUPhaseContextScope noOverwrite({999,true});
     assert(CaptureNativeCPUPhaseContext().parent==local.parent);
    }
    NativeCPUPhaseScope prepare(4,i);prepare.Mark(0);prepare.Counts(5,6);
    assert(prepare.active==loop.active);
   }
   assert(!CaptureNativeCPUPhaseContext().sampled);
   try{NativeCPUPhaseContextScope unwind(context);throw std::runtime_error("test");}catch(const std::runtime_error&){}
   assert(!CaptureNativeCPUPhaseContext().sampled);
  });
 }
 const auto* enabled=std::getenv("BAR_NATIVE_FRAME_TRACE");
 const auto* every=std::getenv("BAR_NATIVE_FRAME_TRACE_EVERY_LOOP");
 const bool traceEnabled=enabled && std::string(enabled)=="1";
 assert(sampled==(traceEnabled?((every && std::string(every)=="1")?100u:7u):0u));
 assert(logs.size()==sampled*4);
 worker.Invoke([]{
  const auto original=CaptureNativeCPUPhaseContext();
  {NativeCPUPhaseContextScope outer({456,true});
   assert(CaptureNativeCPUPhaseContext().parent==456);
   try {NativeCPUPhaseContextScope inner({789,false});
    assert(!CaptureNativeCPUPhaseContext().sampled && CaptureNativeCPUPhaseContext().parent==789);
    throw std::runtime_error("depth-zero nested unwind");
   }catch(const std::runtime_error&){}
   assert(CaptureNativeCPUPhaseContext().sampled && CaptureNativeCPUPhaseContext().parent==456);
  }
  const auto restored=CaptureNativeCPUPhaseContext();
  assert(restored.parent==original.parent && restored.sampled==original.sampled);
 });
 // Completion order is prepare, encode, capture, loop. All IDs are globally
 // unique budget tickets, and worker children retain the main capture parent.
 for(std::size_t i=0;i<logs.size();i+=4){
  unsigned long long ids[4],parents[4];unsigned kinds[4];
  for(int j=0;j<4;++j)assert(std::sscanf(logs[i+j].c_str(),"[NativeCPUTrace] id=%llu parent=%llu kind=%u",&ids[j],&parents[j],&kinds[j])==3);
  assert(kinds[0]==4&&kinds[1]==3&&kinds[2]==2&&kinds[3]==1);
  assert(parents[0]==ids[1]&&parents[1]==ids[2]&&parents[2]==ids[3]&&parents[3]==0);
  assert(ids[3]<ids[2]&&ids[2]<ids[1]&&ids[1]<ids[0]);
 }
 NativeCPUPhaseScope pregame(1,0);assert(!pregame.active);
 std::printf("CPU phase handoff: 100 invocations, %u sampled complete ancestry chains PASS\n",sampled);
}
