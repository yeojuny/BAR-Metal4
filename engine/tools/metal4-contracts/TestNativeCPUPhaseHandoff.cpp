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
  NativeCPUPhaseScope capture(2,i);capture.Draw(1000+i);
  const auto context=CaptureNativeCPUPhaseContext();
  assert(context.sampled==loop.active);
  if(loop.active)++sampled;
  worker.Invoke([&]{
   assert(!CaptureNativeCPUPhaseContext().sampled);
   {NativeCPUPhaseContextScope transferred(context);
    assert(CaptureNativeCPUPhaseContext().parent==context.parent);
    NativeCPUPhaseScope encode(3,i);encode.Draw(1000+i);
    assert(encode.active==loop.active);
    if(encode.active){
     const auto local=CaptureNativeCPUPhaseContext();
     NativeCPUPhaseContextScope noOverwrite({999,true});
     assert(CaptureNativeCPUPhaseContext().parent==local.parent);
    }
    NativeCPUPhaseScope prepare(4,i);prepare.Draw(1000+i);prepare.Mark(0);prepare.Counts(5,6);
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
  unsigned long long ids[4],parents[4],starts[4],ends[4];unsigned kinds[4];
  for(int j=0;j<4;++j){
   const auto endpoints=logs[i+j].find("start_ns=");assert(endpoints!=std::string::npos);
   assert(std::sscanf(logs[i+j].c_str()+endpoints,"start_ns=%llu end_ns=%llu",&starts[j],&ends[j])==2);
   assert(starts[j]<=ends[j]);
   if(j<3){const auto draw=logs[i+j].find("draw_valid=1 source_draw=");assert(draw!=std::string::npos);
    unsigned long long currentDraw=0,firstDraw=0;
    assert(std::sscanf(logs[i+j].c_str()+draw,"draw_valid=1 source_draw=%llu",&currentDraw)==1);
    assert(std::sscanf(logs[i].c_str()+logs[i].find("draw_valid=1 source_draw="),"draw_valid=1 source_draw=%llu",&firstDraw)==1);
    assert(currentDraw==firstDraw);
    assert(logs[i+j].find("thread_cpu_valid=1")!=std::string::npos);}
  }
  // Real worker intervals nest in the joined parent capture/loop envelopes;
  // this is clock/ancestry validation, not asynchronous overlap evidence.
  assert(starts[3]<=starts[2]&&starts[2]<=starts[1]&&starts[1]<=starts[0]);
  assert(ends[0]<=ends[1]&&ends[1]<=ends[2]&&ends[2]<=ends[3]);
  for(int j=0;j<4;++j)assert(std::sscanf(logs[i+j].c_str(),"[NativeCPUTrace] id=%llu parent=%llu kind=%u",&ids[j],&parents[j],&kinds[j])==3);
  assert(kinds[0]==4&&kinds[1]==3&&kinds[2]==2&&kinds[3]==1);
  assert(parents[0]==ids[1]&&parents[1]==ids[2]&&parents[2]==ids[3]&&parents[3]==0);
  assert(ids[3]<ids[2]&&ids[2]<ids[1]&&ids[1]<ids[0]);
 }
 {char buffer[768];NativeCPUPhaseSample sample;sample.start=100;sample.kind=3;
  assert(FormatNativeCPUPhase(buffer,sizeof(buffer),sample,200)>0);
  assert(std::string(buffer).find("start_ns=100 end_ns=200")!=std::string::npos);
  sample.hasSourceDraw=true;sample.sourceDraw=0;
  assert(FormatNativeCPUPhase(buffer,sizeof(buffer),sample,200)>0);
  assert(std::string(buffer).find("draw_valid=1 source_draw=0")!=std::string::npos);
  assert(FormatNativeCPUPhase(buffer,sizeof(buffer),sample,99)==0);
  assert(FormatNativeCPUPhase(buffer,8,sample,200)==0);
  sample.invalid=true;assert(FormatNativeCPUPhase(buffer,sizeof(buffer),sample,200)==0);
 }
 {
  const auto before=logs.size();
  NativeCPUPhaseContextScope context({123,true});
  NativeCPUPhaseScope source(5,1);source.Draw(0);source.Draw(0);
  {NativeCPUPhaseScope encode(6,1);encode.Draw(0);encode.Mark(0);encode.Finish();encode.Finish();}
  source.Mark(0);source.Finish();source.Finish();
  assert(logs.size()==before+(traceEnabled?2:0));
  if(traceEnabled){assert(logs.back().find("kind=5")!=std::string::npos);assert(logs.back().find("draw_valid=1 source_draw=0")!=std::string::npos);}
  const auto valid=logs.size();
  {NativeCPUPhaseScope invalid(5,1);invalid.Draw(1);invalid.Draw(2);}
  assert(logs.size()==valid); // One phase cannot be relabeled as a different draw.
 }
 {
  const auto start=ReadNativeThreadCPUTime();const auto wall=std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  const auto end=ReadNativeThreadCPUTime();const auto wallEnd=std::chrono::steady_clock::now();
  assert(start.valid&&end.valid&&end.ns>=start.ns);
  const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(wallEnd-wall).count();
  assert(end.ns-start.ns<std::uint64_t(elapsed)); // Clock excludes this intentional blocked wait.
 }
 NativeCPUPhaseScope pregame(1,0);assert(!pregame.active);
 std::printf("CPU phase handoff: 100 invocations, %u sampled complete ancestry chains PASS\n",sampled);
}
