/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace rendering::metal4 {

// Diagnostic bookkeeping only. The caller serializes budget access. A ticket
// permits one already-submitted completion to finish even after the ticket cap.
struct NativeSubmissionTraceBudget {
 static constexpr std::uint64_t DurationNS=300ull*1000ull*1000ull*1000ull;
 static constexpr std::size_t MaxRecords=8192,MaxBytes=4u*1024u*1024u;
 std::uint64_t start=0;
 std::size_t tickets=0,records=0,bytes=0;
 bool begun=false,stopped=false;
 bool Open(std::uint64_t now) noexcept {
  if(stopped)return false;
  if(!begun){start=now;begun=true;}
  if(now<start || now-start>=DurationNS){stopped=true;return false;}
  return true;
 }
 bool TakeSample(std::uint64_t now) noexcept {
  if(!Open(now)||tickets>=MaxRecords)return false;
  ++tickets;return true;
 }
 bool TakeRecord(std::uint64_t now,std::size_t length) noexcept {
  if(!Open(now)||records>=tickets)return false;
  if(length>MaxBytes-bytes){stopped=true;return false;}
  ++records;bytes+=length;return true;
 }
 std::uint64_t Deadline() const noexcept {
  const auto maximum=std::numeric_limits<std::uint64_t>::max();
  return start>maximum-DurationNS?maximum:start+DurationNS;
 }
};

struct NativeSubmissionTraceSample {
 bool active=false;
 std::uint64_t deadline=0,lease=0,encode=0,submit=0;
 bool drawableTimed=false;
 std::uint64_t drawableBegin=0,drawableEnd=0;
};

// Measures only the existing nextDrawable call. It neither acquires a drawable
// nor changes its lifetime, presentation order, timeout, or failure behavior.
void BeginNativeSubmissionTraceDrawable(const void* owner) noexcept;
void EndNativeSubmissionTraceDrawable(const void* owner) noexcept;
struct NativeSubmissionTraceDrawableScope {
 explicit NativeSubmissionTraceDrawableScope(const void* value) noexcept:owner(value) {BeginNativeSubmissionTraceDrawable(owner);}
 ~NativeSubmissionTraceDrawableScope() {EndNativeSubmissionTraceDrawable(owner);}
 NativeSubmissionTraceDrawableScope(const NativeSubmissionTraceDrawableScope&)=delete;
 NativeSubmissionTraceDrawableScope& operator=(const NativeSubmissionTraceDrawableScope&)=delete;
 const void* owner;
};

inline std::size_t FormatNativeSubmissionTrace(char* output,std::size_t capacity,
 const NativeSubmissionTraceSample& sample,std::uint64_t submitted,std::uint64_t eventDone,
 std::uint64_t feedbackDone,std::uint64_t event,std::uint64_t frame,bool drawable,
 double gpuStart,double gpuEnd,std::uint64_t cleanupDone=0) noexcept
{
 if(!sample.active || !(sample.lease<=sample.encode && sample.encode<=sample.submit &&
  sample.submit<=submitted && submitted<=eventDone && eventDone<=feedbackDone))return 0;
 if(!cleanupDone)cleanupDone=submitted;
 if(cleanupDone<submitted || cleanupDone>eventDone)return 0;
 const auto gpuUS=(gpuEnd-gpuStart)*1e6;
 const bool gpuValid=std::isfinite(gpuStart)&&std::isfinite(gpuEnd)&&gpuStart>0&&gpuEnd>=gpuStart&&std::isfinite(gpuUS);
 const auto us=[](std::uint64_t a,std::uint64_t b){return double(b-a)/1000.0;};
 const bool split=sample.drawableTimed && sample.lease<=sample.drawableBegin &&
  sample.drawableBegin<=sample.drawableEnd && sample.drawableEnd<=sample.encode;
 const double before=split?us(sample.lease,sample.drawableBegin):-1.0;
 const double acquire=split?us(sample.drawableBegin,sample.drawableEnd):-1.0;
 const double after=split?us(sample.drawableEnd,sample.encode):-1.0;
 const auto size=std::snprintf(output,capacity,
  "[NativeSubmitTrace] event=%llu index=%llu drawable=%u prepare_us=%.3f encode_us=%.3f submit_us=%.3f event_wait_us=%.3f feedback_wait_us=%.3f request_to_feedback_us=%.3f gpu_valid=%u gpu_us=%.3f drawable_timed=%u resource_prepare_us=%.3f drawable_wait_us=%.3f pre_drawable_us=%.3f post_drawable_us=%.3f overlap_cleanup_us=%.3f",
  static_cast<unsigned long long>(event),static_cast<unsigned long long>(frame),unsigned(drawable),
  us(sample.lease,sample.encode),us(sample.encode,sample.submit),us(sample.submit,submitted),
  us(cleanupDone,eventDone),us(eventDone,feedbackDone),us(sample.lease,feedbackDone),
  unsigned(gpuValid),gpuValid?gpuUS:-1.0,unsigned(split),split?before+after:-1.0,acquire,before,after,us(submitted,cleanupDone));
 return size>0 && std::size_t(size)<capacity?std::size_t(size):0;
}

} // namespace rendering::metal4
