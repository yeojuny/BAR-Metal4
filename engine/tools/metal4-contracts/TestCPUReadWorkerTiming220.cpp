#include "Rendering/Backend/NativeCUSCPUReadWorker220.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace rendering;
int main(){
 {
  NativeCUSCPUReadWorker220::Job job;
  job.RecordThreadCPU({10,true},{25,true});assert(job.threadCPUValid&&job.threadCPUNs==15);
  job.RecordThreadCPU({10,false},{25,true});assert(!job.threadCPUValid&&job.threadCPUNs==0);
  job.RecordThreadCPU({10,true},{25,false});assert(!job.threadCPUValid&&job.threadCPUNs==0);
  job.RecordThreadCPU({25,true},{10,true});assert(!job.threadCPUValid&&job.threadCPUNs==0);
  job.RecordThreadCPU({25,true},{25,true});assert(job.threadCPUValid&&job.threadCPUNs==0);
 }
 NativeCUSCPUReadWorker220 worker;
 {
  NativeCUSCPUReadWorker220::Job job;
  job.sources={nullptr,std::make_shared<const NativeCUSSourceFrame>()};
  worker.Start(job);worker.Wait(job);
  assert(job.done&&!job.error&&job.packet.Count()==0);
  assert(job.workerBeginNs==0&&job.workerEndNs==0&&job.threadCPUNs==0&&!job.threadCPUValid);
 }
 bool observedWait=false;
 for(int attempt=0;attempt<3&&!observedWait;++attempt){
  NativeCUSCPUReadWorker220::Job job;job.measure=true;job.ExpectTail();
  job.sources={nullptr,std::make_shared<const NativeCUSSourceFrame>()};
  job.submitted=NativeCUSCPUReadWorker220::Clock::now();
  worker.Start(job);
  // No unsynchronized reads of worker-owned timing fields while it runs.
  std::this_thread::sleep_for(std::chrono::milliseconds(30+attempt*50));
  job.PublishTail({nullptr},{std::make_shared<const NativeCUSSourceFrame>()});
  worker.Wait(job);
  assert(job.done&&!job.error&&!job.optionalFailed&&job.packet.Count()==0);
  assert(job.workerBeginNs>0&&job.workerEndNs>=job.workerBeginNs);
  const auto wall=job.workerEndNs-job.workerBeginNs;
  assert(wall>=job.tailWaitNs&&wall>=job.workNs);
  // Whole-Build thread CPU is not wall-minus-wait. Tolerate clock granularity;
  // unavailable CPU timing must remain explicitly invalid, not zero-work proof.
  if(job.threadCPUValid)assert(job.threadCPUNs<=wall+10000000ULL);
  else assert(job.threadCPUNs==0);
  if(job.tailWaitNs>=15000000ULL){
   observedWait=true;
   if(job.threadCPUValid)assert(job.threadCPUNs+5000000ULL<wall);
  }
  std::cout<<"worker_wall_ns="<<wall<<" tail_wait_ns="<<job.tailWaitNs
           <<" thread_cpu_ns="<<job.threadCPUNs<<" valid="<<job.threadCPUValid<<'\n';
 }
 assert(observedWait); // Bounded retry accommodates a delayed initial worker start.
 std::cout<<"Worker timing measured/off and blocked-tail behavior PASS; no simultaneous CPU-slice proof\n";
}
