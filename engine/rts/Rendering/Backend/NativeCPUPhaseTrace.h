/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace rendering::metal4 {

struct NativeCPUPhaseSample {
	std::uint64_t id=0,parent=0,start=0,previous=0;
	std::int64_t frame=0;
	unsigned kind=0,count=0;
	std::array<std::uint64_t,12> marks{};
	std::uint64_t bytes=0,allocations=0;
	bool invalid=false;
};

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
		"[NativeCPUTrace] id=%llu parent=%llu kind=%u frame=%lld phases=%u total_us=%.3f interval_us=%.3f tail_us=%.3f p0=%.3f p1=%.3f p2=%.3f p3=%.3f p4=%.3f p5=%.3f p6=%.3f p7=%.3f p8=%.3f p9=%.3f p10=%.3f p11=%.3f count_a=%llu count_b=%llu",
		static_cast<unsigned long long>(sample.id),static_cast<unsigned long long>(sample.parent),sample.kind,
		static_cast<long long>(sample.frame),sample.count,double(end-sample.start)/1000.0,interval,double(end-previous)/1000.0,
		phases[0],phases[1],phases[2],phases[3],phases[4],phases[5],phases[6],phases[7],phases[8],phases[9],phases[10],phases[11],
		static_cast<unsigned long long>(sample.bytes),static_cast<unsigned long long>(sample.allocations));
	return size>0 && std::size_t(size)<capacity?std::size_t(size):0;
}

bool BeginNativeCPUPhase(const void* owner,unsigned kind,std::int64_t frame) noexcept;
void MarkNativeCPUPhase(const void* owner,unsigned phase) noexcept;
void CountNativeCPUPhase(const void* owner,std::uint64_t a,std::uint64_t b) noexcept;
void FinishNativeCPUPhase(const void* owner) noexcept;
struct NativeCPUPhaseScope {
	explicit NativeCPUPhaseScope(unsigned kind,std::int64_t frame) noexcept:active(BeginNativeCPUPhase(this,kind,frame)) {}
	~NativeCPUPhaseScope() {if(active)FinishNativeCPUPhase(this);}
	void Mark(unsigned phase) noexcept {if(active)MarkNativeCPUPhase(this,phase);}
	void Counts(std::uint64_t a,std::uint64_t b) noexcept {if(active)CountNativeCPUPhase(this,a,b);}
	NativeCPUPhaseScope(const NativeCPUPhaseScope&)=delete;
	NativeCPUPhaseScope& operator=(const NativeCPUPhaseScope&)=delete;
	bool active=false;
};

} // namespace rendering::metal4
