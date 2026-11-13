/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSCPUReadScope.h"
#include "NativeThreadCPUTime.h"
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

namespace rendering {
// Opt-in single CPU-read worker. Jobs and every input owner stay main-owned;
// no Session, GPU object, texture provider or producer callback is visited.
class NativeCUSCPUReadWorker220 {
public:
 using Clock=std::chrono::steady_clock;
 struct Job {
  std::vector<std::shared_ptr<const NativeCUSSourceFrame>> sources;
  NativeCUSCPUReadScope::Packet packet;
  std::exception_ptr error;
  bool measure=false,done=false;
  bool optionalFailed=false; // Reject speculative suffix, never advance its error boundary.
  // Main owns every source list. One worker scope preserves opaque/alpha/shadow
  // deduplication and capacity order; terminal tail state shares one mutex.
  enum class TailState { Disabled, Waiting, Published, Cancelled };
  std::mutex tailMutex;std::condition_variable tailReady;
  TailState tailState=TailState::Disabled;
  std::vector<std::shared_ptr<const NativeCUSSourceFrame>> tailSources,optionalSources;
  void ExpectTail(){std::lock_guard lock(tailMutex);if(tailState!=TailState::Disabled)throw std::logic_error("CPU read tail already configured");tailState=TailState::Waiting;}
  void PublishTail(std::vector<std::shared_ptr<const NativeCUSSourceFrame>> sources,
                   std::vector<std::shared_ptr<const NativeCUSSourceFrame>> optional={}){
   {std::lock_guard lock(tailMutex);if(tailState!=TailState::Waiting)throw std::logic_error("CPU read tail already terminal");tailSources=std::move(sources);optionalSources=std::move(optional);tailState=TailState::Published;}
   tailReady.notify_one();
  }
  void CancelTail(){
   {std::lock_guard lock(tailMutex);if(tailState==TailState::Waiting)tailState=TailState::Cancelled;}
   tailReady.notify_one();
  }
  bool AwaitTail(){std::unique_lock lock(tailMutex);tailReady.wait(lock,[&]{return tailState!=TailState::Waiting;});return tailState==TailState::Published;}

  uint64_t queueNs=0,workNs=0,waitNs=0,tailWaitNs=0;
  uint64_t workerBeginNs=0,workerEndNs=0,threadCPUNs=0;
  bool threadCPUValid=false;
  void RecordThreadCPU(metal4::NativeThreadCPUStamp begin,metal4::NativeThreadCPUStamp end)noexcept{
   threadCPUValid=begin.valid&&end.valid&&end.ns>=begin.ns;
   threadCPUNs=threadCPUValid?end.ns-begin.ns:0;
  }
  size_t cacheBefore=0,cacheAfter=0;
  Clock::time_point submitted{};
 };
 static uint64_t Elapsed(Clock::time_point from){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-from).count();}
 static uint64_t Timestamp(Clock::time_point value){return std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch()).count();}
 // The original mandatory prefix/tail may throw to Build's existing error
 // receipt. Only the newly speculative suffix falls back without early error
 // delivery. No partially extended packet may be sealed on suffix failure.
 // Factored so fault tests exercise the production ordering/catch boundary.
 template<class Prime> static bool VisitSources(Job& job,Prime&& prime){
  for(const auto& source:job.sources)prime(source);
  const auto tailBegin=job.measure?Clock::now():Clock::time_point{};
  const bool hasTail=job.AwaitTail();
  if(job.measure)job.tailWaitNs=Elapsed(tailBegin);
  if(hasTail){
   for(const auto& source:job.tailSources)prime(source);
   try{for(const auto& source:job.optionalSources)prime(source);}
   catch(...){job.optionalFailed=true;return false;}
  }
  return true;
 }
 static void Build(Job& job)noexcept{
  const auto start=job.measure?Clock::now():Clock::time_point{};
  const auto cpuStart=job.measure?metal4::ReadNativeThreadCPUTime():metal4::NativeThreadCPUStamp{};
  if(job.measure){job.workerBeginNs=Timestamp(start);job.queueNs=std::chrono::duration_cast<std::chrono::nanoseconds>(start-job.submitted).count();}
  try{
   NativeCUSCPUReadScope scope;
   if(job.measure)job.cacheBefore=NativeCUSCPUReadScope::GeometryCacheSize();
   if(VisitSources(job,[&](const auto& source){scope.Prime(source);}))
    job.packet=scope.Seal();
  }catch(...){job.error=std::current_exception();}
  if(job.measure){
   job.cacheAfter=NativeCUSCPUReadScope::GeometryCacheSize();
   const auto finish=Clock::now();job.workerEndNs=Timestamp(finish);
   job.workNs=std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count()-job.tailWaitNs;
   const auto cpuEnd=metal4::ReadNativeThreadCPUTime();
   job.RecordThreadCPU(cpuStart,cpuEnd);
  }
 }
 NativeCUSCPUReadWorker220():thread([this]{Loop();}){}
 ~NativeCUSCPUReadWorker220(){
  {std::lock_guard lock(mutex);if(pending)std::terminate();stopping=true;}
  wake.notify_one();thread.join();
 }
 NativeCUSCPUReadWorker220(const NativeCUSCPUReadWorker220&)=delete;
 NativeCUSCPUReadWorker220& operator=(const NativeCUSCPUReadWorker220&)=delete;
 void Start(Job& job){
  // All throwing operations precede publication. notify_one is noexcept;
  // after this function returns the caller must arm its nonthrowing join guard.
  {std::lock_guard lock(mutex);if(pending||stopping)throw std::logic_error("CUS CPU worker already owns a scoped job");pending=&job;}
  wake.notify_one();
 }
 void Wait(Job& job){
  const auto start=job.measure?Clock::now():Clock::time_point{};
  {std::unique_lock lock(mutex);finished.wait(lock,[&]{return job.done;});}
  if(job.measure)job.waitNs=Elapsed(start);
 }
 // Construct before Start, arm only after Start succeeds. Destruction always
 // joins before the main thread can discard a failed/aborted capture's owners.
 class Join {
  NativeCUSCPUReadWorker220* worker=nullptr;
  Job* job=nullptr;
 public:
  Join()=default;
  Join(const Join&)=delete;
  Join& operator=(const Join&)=delete;
  void Arm(NativeCUSCPUReadWorker220& w,Job& j)noexcept{worker=&w;job=&j;}
  // Transfer only to a fresh main-owned guard; never orphan an active job.
  void TransferTo(Join& target)noexcept{
   if(target.worker)std::terminate();
   target.worker=std::exchange(worker,nullptr);target.job=std::exchange(job,nullptr);
  }
  void Finish(){if(worker){job->CancelTail();worker->Wait(*job);worker=nullptr;}}
  ~Join(){Finish();}
 };
private:
 void Loop(){
  for(;;){
   Job* job;
   {std::unique_lock lock(mutex);wake.wait(lock,[&]{return stopping||pending;});if(stopping)return;job=pending;}
   Build(*job);
   {std::lock_guard lock(mutex);job->done=true;pending=nullptr;}
   finished.notify_one();
  }
 }
 std::mutex mutex;
 std::condition_variable wake,finished;
 Job* pending=nullptr;
 bool stopping=false;
 std::thread thread;
};
} // namespace rendering
