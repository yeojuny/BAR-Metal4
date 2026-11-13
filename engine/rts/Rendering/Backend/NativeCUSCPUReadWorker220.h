/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSCPUReadScope.h"
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <utility>

namespace rendering {
// Diagnostic-only single worker. Jobs and every input owner stay main-owned;
// no Session, GPU object, texture provider or producer callback is visited.
class NativeCUSCPUReadWorker220 {
public:
 using Clock=std::chrono::steady_clock;
 struct Job {
  std::vector<std::shared_ptr<const NativeCUSSourceFrame>> sources;
  NativeCUSCPUReadScope::Packet packet;
  std::exception_ptr error;
  bool measure=false,done=false;
  uint64_t queueNs=0,workNs=0,waitNs=0;
  size_t cacheBefore=0,cacheAfter=0;
  Clock::time_point submitted{};
 };
 static uint64_t Elapsed(Clock::time_point from){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-from).count();}
 static uint64_t Timestamp(Clock::time_point value){return std::chrono::duration_cast<std::chrono::nanoseconds>(value.time_since_epoch()).count();}
 static void Build(Job& job)noexcept{
  const auto start=job.measure?Clock::now():Clock::time_point{};
  if(job.measure)job.queueNs=std::chrono::duration_cast<std::chrono::nanoseconds>(start-job.submitted).count();
  try{
   NativeCUSCPUReadScope scope;
   if(job.measure)job.cacheBefore=NativeCUSCPUReadScope::GeometryCacheSize();
   for(const auto& source:job.sources)scope.Prime(source);
   job.packet=scope.Seal();
  }catch(...){job.error=std::current_exception();}
  if(job.measure){job.cacheAfter=NativeCUSCPUReadScope::GeometryCacheSize();job.workNs=Elapsed(start);}
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
  void Finish(){if(worker){worker->Wait(*job);worker=nullptr;}}
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
