/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>

namespace rendering::metal4 {

// The original Start commits caller-staged membership. Opt-in StartDiff220
// additionally stages the exact immutable old/next diff on this same worker.
// Selected-slot GPU retirement precedes either path. Avoid ALL residency-set
// access until Finish; independent command encoding is allowed before Finish.
class ResidencyCommitTask {
public:
	struct Timing { uint64_t commitNs=0, dispatchNs=0, joinNs=0;
  uint64_t diffNs=0, added=0, removed=0, requested=0; bool workerDiff220=false; };
	ResidencyCommitTask() = default;
	ResidencyCommitTask(const ResidencyCommitTask&) = delete;
	ResidencyCommitTask& operator=(const ResidencyCommitTask&) = delete;
	~ResidencyCommitTask() { try { Finish(); } catch (...) {} }
	bool Active() const noexcept { return bool(pending); }
	void Start(dispatch_queue_t queue, id<MTLResidencySet> residency, bool measure) {
		if(pending || !queue || !residency)throw std::runtime_error("Invalid asynchronous residency commit");
		auto job=std::make_shared<Job>();job->residency=residency;job->measure=measure;
		job->dispatched=measure?Now():0;pending=job;
		dispatch_async(queue, ^{
			@autoreleasepool {
				const auto start=job->measure?Now():0;
				try {
					@try { [job->residency commit]; }
					@catch(NSException* error) {
						throw std::runtime_error(std::string("Residency commit failed: ")+(error.reason.UTF8String?:"unknown exception"));
					}
				} catch (...) { job->error=std::current_exception(); }
				if(job->measure){job->timing.commitNs=Now()-start;job->timing.dispatchNs=start-job->dispatched;}
				dispatch_semaphore_signal(job->done);
			}
		});
	}
 // Snapshot ownership is acquired before publishing pending or dispatching.
 // No table mutation, renderer callback or frame access occurs on the worker.
 void StartDiff220(dispatch_queue_t queue, id<MTLResidencySet> residency,
                   NSHashTable* old, NSHashTable* next, uint64_t requested, bool measure) {
  if(pending || !queue || !residency || !next)throw std::runtime_error("Invalid asynchronous residency diff");
  auto job=std::make_shared<Job>();job->residency=residency;job->measure=measure;
  job->old220=old;job->next220=next;job->timing.workerDiff220=true;job->timing.requested=requested;
  job->dispatched=measure?Now():0;pending=job;
  dispatch_async(queue, ^{
   @autoreleasepool {
    const auto start=job->measure?Now():0;
    try {
     @try {
      for(id<MTLAllocation> allocation in job->old220)if(![job->next220 containsObject:allocation]){
       [job->residency removeAllocation:allocation];if(job->measure)++job->timing.removed;
      }
      for(id<MTLAllocation> allocation in job->next220)if(![job->old220 containsObject:allocation]){
       [job->residency addAllocation:allocation];if(job->measure)++job->timing.added;
      }
      const auto commitStart=job->measure?Now():0;
      if(job->measure){job->timing.diffNs=commitStart-start;job->timing.dispatchNs=start-job->dispatched;}
      [job->residency commit];
      if(job->measure)job->timing.commitNs=Now()-commitStart;
     } @catch(NSException* error) {
      throw std::runtime_error(std::string("Residency diff/commit failed: ")+(error.reason.UTF8String?:"unknown exception"));
     }
    } catch(...) {job->error=std::current_exception();}
    dispatch_semaphore_signal(job->done);
   }
  });
 }
	Timing Finish() {
		if(!pending)return {};
		auto job=std::move(pending);const auto start=job->measure?Now():0;
		dispatch_semaphore_wait(job->done,DISPATCH_TIME_FOREVER);
		if(job->measure)job->timing.joinNs=Now()-start;
		if(job->error)std::rethrow_exception(job->error);
		return job->timing;
	}
private:
	struct Job {
		__strong id<MTLResidencySet> residency=nil;
  __strong NSHashTable* old220=nil;
  __strong NSHashTable* next220=nil;
		dispatch_semaphore_t done=dispatch_semaphore_create(0);
		std::exception_ptr error;
		Timing timing;
		uint64_t dispatched=0;
		bool measure=false;
	};
	static uint64_t Now() noexcept {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
	std::shared_ptr<Job> pending;
};

} // namespace rendering::metal4
