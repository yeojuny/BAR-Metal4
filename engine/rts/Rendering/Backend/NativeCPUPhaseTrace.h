/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeThreadCPUTime.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <exception>

namespace rendering::metal4 {

// Append registered kinds here; the sentinel sizes the actual runtime table.
enum class NativeCPUPhaseKind : unsigned {
 Loop=1, World=2, WorkerDraw=3, Prepare=4, Capture=5, Encode=6,
 Admission=7, ImmutablePrefix=8, FullDrain=9, Boundary=10, CPUCreditReclaim=11,
 MainFinish=12, TaskJoin=13, HandoffRelease=14, TargetNotice=15,
 RetirementNotice=16, CPUNotice=17, RuntimeRetire=18, EventWait=19,
 FeedbackWait=20, AllocatorReset=21, GPUCallback=22, ResidencyCleanup=23,
 ResourceCommit=24, Submit=25, CompletedOwnerRelease=26, WorkerJob=27,
 CaptureInvocation=28, ReadChecks=29, ModelFactsRequests=30, ModelFactsPositive=31,
 ModelFactsCasts=32, ModelFactsCapacity=33, ModelFactsProviderTime=34, ModelFactsCPUValidity=35,
 // Sampled provider aggregates; count_a/count_b carry nanoseconds or counts,
 // never the emission scope's own duration. Existing kind values stay fixed.
 ModelProviderSummary=36, ModelProviderCompleted=37, ModelProviderFailures=38,
 ModelProviderDispatchTime=39, ModelProviderPreparationTime=40,
 ModelProviderOtherTime=41, ModelProviderFailureTime=42, Count=43
};
enum class NativeCPUFinishReason : unsigned {Control=1, CapturePrefix=2, Credit=3, FullDrain=4, DispatchPrior=5, JoinedFallback=6, NonPresent=7, Rollback=8, SurfaceBoundary=9};
// A copied identity is diagnostic data, never an owner or an admission token.
struct NativeCPUTraceIdentity {
 std::uint64_t request=0,draw=0,job=0,lease=0,submission=0,event=0,runtimeIndex=0;
 std::int64_t simulation=0;
};
struct NativeCPUPhaseSample {
	std::uint64_t id=0,parent=0,start=0,previous=0;
	std::int64_t frame=0;
	std::uint64_t sourceDraw=0;
	bool hasSourceDraw=false;
	NativeThreadCPUStamp threadCPUStart{};
	std::uint64_t threadCPUNs=0;
	bool threadCPUValid=false;
	unsigned kind=0,count=0;
	std::array<std::uint64_t,12> marks{};
	std::uint64_t bytes=0,allocations=0;
	bool invalid=false;
 NativeCPUTraceIdentity incoming{},job{},retired{};
 unsigned reason=0,status=0; // 0 success/ordinary, 1 no job, 2 failure, 3 no predecessor, 4 missing sampled job

};

// Common monotonic endpoints are required for cross-thread intersection tests.
// These scopes are wall-time envelopes: inclusion of a wait is NOT active CPU
// overlap evidence. Keep the original durations and ancestry for existing readers.
inline std::size_t FormatNativeCPUPhase(char* output,std::size_t capacity,
	const NativeCPUPhaseSample& sample,std::uint64_t end) noexcept
{
	if(sample.invalid || sample.count>sample.marks.size() || end<sample.start)return 0;
	double phases[12];for(auto& value:phases)value=-1;
	auto previous=sample.start;
	for(unsigned i=0;i<sample.count;++i){
		if(sample.marks[i]<previous || sample.marks[i]>end)return 0;
		phases[i]=double(sample.marks[i]-previous)/1000.0;previous=sample.marks[i];
	}
	const double interval=sample.previous && sample.previous<=sample.start?double(sample.start-sample.previous)/1000.0:-1;
	const int size=std::snprintf(output,capacity,
		"[NativeCPUTrace] id=%llu parent=%llu kind=%u frame=%lld phases=%u total_us=%.3f interval_us=%.3f tail_us=%.3f p0=%.3f p1=%.3f p2=%.3f p3=%.3f p4=%.3f p5=%.3f p6=%.3f p7=%.3f p8=%.3f p9=%.3f p10=%.3f p11=%.3f count_a=%llu count_b=%llu start_ns=%llu end_ns=%llu draw_valid=%u source_draw=%llu thread_cpu_valid=%u thread_cpu_ns=%llu request=%llu request_draw=%llu request_sim=%lld job=%llu job_request=%llu job_draw=%llu job_sim=%lld lease=%llu submission=%llu event=%llu runtime_index=%llu retired_job=%llu retired_request=%llu retired_draw=%llu retired_sim=%lld retired_lease=%llu retired_submission=%llu retired_event=%llu retired_runtime_index=%llu reason=%u status=%u",
		static_cast<unsigned long long>(sample.id),static_cast<unsigned long long>(sample.parent),sample.kind,
		static_cast<long long>(sample.frame),sample.count,double(end-sample.start)/1000.0,interval,double(end-previous)/1000.0,
		phases[0],phases[1],phases[2],phases[3],phases[4],phases[5],phases[6],phases[7],phases[8],phases[9],phases[10],phases[11],
		static_cast<unsigned long long>(sample.bytes),static_cast<unsigned long long>(sample.allocations),
		static_cast<unsigned long long>(sample.start),static_cast<unsigned long long>(end),
		unsigned(sample.hasSourceDraw),static_cast<unsigned long long>(sample.sourceDraw),
		unsigned(sample.threadCPUValid),static_cast<unsigned long long>(sample.threadCPUNs),
  static_cast<unsigned long long>(sample.incoming.request),static_cast<unsigned long long>(sample.incoming.draw),static_cast<long long>(sample.incoming.simulation),
  static_cast<unsigned long long>(sample.job.job),static_cast<unsigned long long>(sample.job.request),static_cast<unsigned long long>(sample.job.draw),static_cast<long long>(sample.job.simulation),
  static_cast<unsigned long long>(sample.job.lease),static_cast<unsigned long long>(sample.job.submission),static_cast<unsigned long long>(sample.job.event),static_cast<unsigned long long>(sample.job.runtimeIndex),
  static_cast<unsigned long long>(sample.retired.job),static_cast<unsigned long long>(sample.retired.request),static_cast<unsigned long long>(sample.retired.draw),static_cast<long long>(sample.retired.simulation),
  static_cast<unsigned long long>(sample.retired.lease),static_cast<unsigned long long>(sample.retired.submission),static_cast<unsigned long long>(sample.retired.event),static_cast<unsigned long long>(sample.retired.runtimeIndex),sample.reason,sample.status);
	return size>0 && std::size_t(size)<capacity?std::size_t(size):0;
}

struct NativeCPUPhaseRecord {NativeCPUPhaseSample sample;std::uint64_t end=0;};
// Exactly one writer until the existing task Join destroys its callable. No
// callback writes here. Source emission is legal only after that handoff.
struct NativeCPUPhaseBuffer {
 static constexpr std::size_t Capacity=128;
 NativeCPUTraceIdentity identity{};
 std::array<NativeCPUPhaseRecord,Capacity> records{};
 std::size_t size=0,dropped=0;
 bool emitted=false;
 void Append(const NativeCPUPhaseSample& sample,std::uint64_t end) noexcept {
  if(size<Capacity)records[size++]={sample,end};else ++dropped;
 }
};
// Scalars plus a borrowed diagnostic-only sink. Its explicit owner must outlive
// this installed scope; no runtime/source object pointer is transported.
struct NativeCPUPhaseContext {
 std::uint64_t parent=0;bool sampled=false;
 NativeCPUTraceIdentity incoming{},job{},retired{};
 NativeCPUPhaseBuffer* buffer=nullptr;
 unsigned reason=0;
};
std::shared_ptr<NativeCPUPhaseBuffer> MakeNativeCPUJobTrace(std::uint64_t draw,std::int64_t simulation) noexcept;
void FlushNativeCPUPhaseBuffer(NativeCPUPhaseBuffer&) noexcept;
void QueueJoinedNativeCPUTrace(std::shared_ptr<NativeCPUPhaseBuffer>&) noexcept;
NativeCPUTraceIdentity NewNativeCPUTraceLease() noexcept;
class NativeCPUCaptureTraceScope {
public:
 NativeCPUCaptureTraceScope(std::uint64_t draw,std::int64_t simulation) noexcept;
 ~NativeCPUCaptureTraceScope();
 NativeCPUCaptureTraceScope(const NativeCPUCaptureTraceScope&)=delete;
private:
 std::shared_ptr<NativeCPUPhaseBuffer> buffer;
 NativeCPUPhaseContext previous{};
};
// Temporarily identify a joined job or a retired ticket without changing the
// incoming request or borrowing the old job's buffer/owners.
class NativeCPUTraceRelationScope {
public:
 NativeCPUTraceRelationScope(NativeCPUTraceIdentity value,NativeCPUFinishReason reason) noexcept;
 explicit NativeCPUTraceRelationScope(NativeCPUTraceIdentity retired) noexcept;
 ~NativeCPUTraceRelationScope();
 NativeCPUTraceRelationScope(const NativeCPUTraceRelationScope&)=delete;
private: NativeCPUPhaseContext previous{};
};
NativeCPUPhaseContext CaptureNativeCPUPhaseContext() noexcept;
class NativeCPUPhaseContextScope {
public:
 explicit NativeCPUPhaseContextScope(NativeCPUPhaseContext context) noexcept;
 ~NativeCPUPhaseContextScope();
 NativeCPUPhaseContextScope(const NativeCPUPhaseContextScope&)=delete;
 NativeCPUPhaseContextScope& operator=(const NativeCPUPhaseContextScope&)=delete;
private:
 NativeCPUPhaseContext previous{};
 bool installed=false;
};

bool BeginNativeCPUPhase(const void* owner,unsigned kind,std::int64_t frame) noexcept;
void MarkNativeCPUPhase(const void* owner,unsigned phase) noexcept;
void SetNativeCPUPhaseDraw(const void* owner,std::uint64_t draw) noexcept;
void CountNativeCPUPhase(const void* owner,std::uint64_t a,std::uint64_t b) noexcept;
void FinishNativeCPUPhase(const void* owner) noexcept;
void SetNativeCPUPhaseIdentity(const void*,NativeCPUTraceIdentity) noexcept;
void SetNativeCPUPhaseStatus(const void*,unsigned) noexcept;
std::int64_t NativeCPUTraceFrame() noexcept;
struct NativeCPUPhaseScope {
	explicit NativeCPUPhaseScope(unsigned kind,std::int64_t frame) noexcept:active(BeginNativeCPUPhase(this,kind,frame)) {}
 explicit NativeCPUPhaseScope(NativeCPUPhaseKind kind) noexcept:NativeCPUPhaseScope(unsigned(kind),NativeCPUTraceFrame()) {}
 ~NativeCPUPhaseScope() {if(std::uncaught_exceptions()>exceptions)Status(2);Finish();}
 void Identity(NativeCPUTraceIdentity value) noexcept {if(active)SetNativeCPUPhaseIdentity(this,value);}
 void Status(unsigned value) noexcept {if(active)SetNativeCPUPhaseStatus(this,value);}
	void Finish() noexcept {if(active){FinishNativeCPUPhase(this);active=false;}}
	void Draw(std::uint64_t draw) noexcept {if(active)SetNativeCPUPhaseDraw(this,draw);}
	void Mark(unsigned phase) noexcept {if(active)MarkNativeCPUPhase(this,phase);}
	void Counts(std::uint64_t a,std::uint64_t b) noexcept {if(active)CountNativeCPUPhase(this,a,b);}
	NativeCPUPhaseScope(const NativeCPUPhaseScope&)=delete;
	NativeCPUPhaseScope& operator=(const NativeCPUPhaseScope&)=delete;
	bool active=false;
 int exceptions=std::uncaught_exceptions();
};

} // namespace rendering::metal4
