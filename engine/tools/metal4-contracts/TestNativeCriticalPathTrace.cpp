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
#include "Rendering/Backend/NativeRenderJobSettlement.h"
static std::vector<std::string> logs;
#define LOG(format,text) logs.emplace_back(text)
namespace rendering::metal4 {
static std::uint64_t SubmissionTraceNow() noexcept {
 return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
#include "cpu-phase-runtime.inc"
}
using namespace rendering;
using namespace rendering::metal4;
static unsigned long long Field(const std::string& row,const std::string& key) {
 auto at=row.find(" "+key+"=");assert(at!=std::string::npos);
 return std::strtoull(row.c_str()+at+key.size()+2,nullptr,10);
}
int main(int argc,char** argv) {
 const std::string mode=argc>1?argv[1]:"success";
 const bool tail=mode=="tail";
 const bool enabled=std::string(std::getenv("BAR_NATIVE_FRAME_TRACE"))=="1";
 NativeSerialRenderWorker worker;
 std::atomic<bool> go=false;
 unsigned target=0,retired=0,notified=0;
 auto receipt=std::make_shared<NativeRenderDispatchReceipt>(nullptr,[&]{++retired;if(mode=="retirement")throw std::runtime_error("retirement");});
 NativeRenderJobSettlement<int> job(receipt,[&]{++target;if(mode=="target")throw std::runtime_error("target");},[&](const int*,std::exception_ptr){++notified;});
 std::shared_ptr<NativeCPUPhaseBuffer> owned;
 NativeCPUTraceIdentity source;
 std::weak_ptr<int> callableOwner;
 std::unique_ptr<NativeSerialRenderWorker::Task<int>> occupied;
 {NativeCPUPhaseScope loop(1,299);NativeCPUCaptureTraceScope pregame(1,299);assert(!MakeNativeCPUJobTrace(1,299));}
 assert(CPUPhaseTrace().ids==0&&CPUPhaseTrace().requests==0&&CPUPhaseTrace().jobs==0);
 {
  NativeCPUPhaseScope loop(1,tail?306:300);NativeCPUCaptureTraceScope capture(100,tail?306:300);
  NativeCPUPhaseScope incoming(NativeCPUPhaseKind::CaptureInvocation);
  owned=MakeNativeCPUJobTrace(100,tail?306:300);
  assert(bool(owned)==enabled);
  auto context=CaptureNativeCPUPhaseContext();
  if(owned){source=owned->identity;context.job=source;context.buffer=owned.get();}
  auto token=std::make_shared<int>(1);callableOwner=token;
  if(mode=="enqueue")occupied=worker.Enqueue([]{return 1;});
  try {
   job.Submit(worker,[&,context,traceOwner=owned,token=std::move(token)]()->int{
    NativeCPUPhaseContextScope transferred(context);
    NativeCPUPhaseScope whole(NativeCPUPhaseKind::WorkerJob);
    while(!go.load(std::memory_order_acquire))std::this_thread::yield();
    {
     const NativeCPUTraceIdentity predecessor{.request=11,.draw=45,.job=73,.lease=91,.submission=19,.event=87,.runtimeIndex=15,.simulation=299};
     NativeCPUTraceRelationScope retiredIdentity(predecessor);
     NativeCPUPhaseScope retirement(NativeCPUPhaseKind::RuntimeRetire);
     {NativeCPUPhaseScope event(NativeCPUPhaseKind::EventWait);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
     {NativeCPUPhaseScope feedback(NativeCPUPhaseKind::FeedbackWait);}
     if(mode=="worker")throw std::runtime_error("worker");
     {NativeCPUPhaseScope callback(NativeCPUPhaseKind::GPUCallback);}
    }
    {NativeCPUPhaseScope submit(NativeCPUPhaseKind::Submit);}
    receipt->MarkRetirementReady();
    return 17;
   });
   assert(mode!="enqueue");
  }catch(const std::logic_error&){assert(mode=="enqueue");}
 }
 assert(logs.empty()); // No formatter/LOG publication from the worker/capture.
 {
  NativeCPUPhaseScope loop(1,tail?307:301);NativeCPUCaptureTraceScope capture(905,tail?307:301);
  const auto waiting=CaptureNativeCPUPhaseContext().incoming;
  if(enabled&&!tail){assert(waiting.request!=source.request&&waiting.draw!=source.draw+1);}
  {NativeCPUPhaseScope prefix(8,301);prefix.Draw(905);}
  if(mode!="enqueue")assert(!job.WorkerCallableReleased());
  go.store(true,std::memory_order_release);
  try {
   NativeCPUTraceRelationScope relation(source,NativeCPUFinishReason::CapturePrefix);
   NativeCPUPhaseScope finish(NativeCPUPhaseKind::MainFinish);
   job.Finish();assert(mode=="success"||tail);
  }catch(const std::exception&){assert(mode!="success");}
  assert(job.WorkerCallableReleased());assert(callableOwner.expired());
  QueueJoinedNativeCPUTrace(owned);assert(!owned);
  if(!tail)assert(logs.empty());
  {NativeCPUPhaseScope highest(unsigned(NativeCPUPhaseKind::Count)-1,301);assert(highest.active==(enabled&&!tail));}
  {NativeCPUPhaseScope invalid(unsigned(NativeCPUPhaseKind::Count),301);assert(!invalid.active);}
 }
 if(occupied)occupied->Join();
 if(!tail)assert(logs.empty()); // Emission remains outside the attribution window.
 {
  NativeCPUPhaseScope loop(1,tail?308:307);NativeCPUCaptureTraceScope after(1100,tail?308:307);
  assert(!MakeNativeCPUJobTrace(1100,307));
 }
 if(enabled){
  unsigned joins=0,waits=0,workerRows=0,highest=0,submits=0;bool emission=false;
  for(const auto& row:logs){
   if(row.starts_with("[NativeCPUTraceEmission]")){assert(Field(row,"outside_window")==1);emission=true;continue;}
   if(!row.starts_with("[NativeCPUTrace]"))continue;
   const auto kind=Field(row,"kind");
   if(kind==13){++joins;assert(Field(row,"request")!=Field(row,"job_request"));assert(Field(row,"request_draw")==905&&Field(row,"job_draw")==100);}
   if(kind==19){++waits;assert(Field(row,"job")==source.job&&Field(row,"retired_job")==73);assert(Field(row,"retired_event")==87&&Field(row,"retired_submission")==19&&Field(row,"retired_runtime_index")==15);}
   if(kind==27)++workerRows;
   if(kind==25)++submits;
   if(kind==unsigned(NativeCPUPhaseKind::Count)-1)++highest;
   if(kind==18)assert(Field(row,"status")==unsigned(mode=="worker"?2:0));
   if(kind==12)assert(Field(row,"status")==unsigned(mode=="success"?0:2));
  }
  assert(emission&&highest==unsigned(!tail));
  if(tail)assert(submits==1);
  assert(joins==unsigned(mode!="enqueue"&&!tail)&&waits==unsigned(mode!="enqueue")&&workerRows==unsigned(mode!="enqueue"));
 }else{
  assert(logs.empty()&&CPUPhaseTrace().ids==0&&CPUPhaseTrace().requests==0&&CPUPhaseTrace().jobs==0&&CPUPhaseTrace().leases==0);
 }
 assert(target==unsigned(mode!="enqueue"));
 assert(notified==1);
 assert(retired==unsigned(mode!="enqueue"&&mode!="worker"));
 // A fixed record ring drops explicitly, never grows or overwrites a chain.
 NativeCPUPhaseBuffer bounded;
 NativeCPUPhaseSample sample;sample.start=1;sample.kind=unsigned(NativeCPUPhaseKind::Count)-1;
 for(std::size_t i=0;i<NativeCPUPhaseBuffer::Capacity+3;++i)bounded.Append(sample,2);
 assert(bounded.size==NativeCPUPhaseBuffer::Capacity&&bounded.dropped==3);
 char formatted[2048];sample.incoming.request=sample.job.job=sample.retired.event=~std::uint64_t{};
 assert(FormatNativeCPUPhase(formatted,sizeof(formatted),sample,2)>0);
 assert(!FormatNativeCPUPhase(formatted,32,sample,2));
 if(enabled){
  FlushNativeCPUPhaseBuffer(bounded);
  bool overflowReported=false;
  for(const auto& row:logs)if(row.starts_with("[NativeCPUTraceBuffer]")&&Field(row,"dropped")==3)overflowReported=true;
  assert(overflowReported);
  for(unsigned i=0;i<100&&!CPUPhaseTrace().finished;++i){bounded.emitted=false;FlushNativeCPUPhaseBuffer(bounded);}
  assert(CPUPhaseTrace().finished);
  assert(CPUPhaseTrace().budget.bytes<=NativeSubmissionTraceBudget::MaxBytes);
  assert(CPUPhaseTrace().budget.tickets<=NativeSubmissionTraceBudget::MaxRecords);
 }
 std::printf("Critical trace %s enabled=%u: exact three identities, real Join/callable release, deferred emission, highest kind, bounded/OFF PASS\n",mode.c_str(),unsigned(enabled));
}
