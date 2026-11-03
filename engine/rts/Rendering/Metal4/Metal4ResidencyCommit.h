/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <chrono>
#include <exception>
#include <memory>
#include <stdexcept>

namespace rendering::metal4 {

// Only commit runs on the worker. The caller stages membership after GPU
// retirement, then avoids ALL residency-set access until Finish. Independent
// command encoding is permitted here; queue submission always follows Finish.
class ResidencyCommitTask {
public:
	struct Timing { uint64_t commitNs=0, dispatchNs=0, joinNs=0; };
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
