/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// Standalone diagnostic: link with System/Sync/FPUCheck.cpp, not the engine.
// Only logging and CPUID are stubbed; FPCR access and restoration use streflop.
#include "System/Sync/FPUCheck.h"
#include "lib/streflop/streflop_cond.h"
#include "System/Log/ILog.h"

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdio>
#include <thread>

static std::atomic<unsigned int> warnings{0};

extern "C" void log_frontend_record(int level, const char*, const char*, ...)
{
	if (level == LOG_LEVEL_WARNING)
		++warnings;
}

namespace springproc {
	void ExecCPUID(unsigned int* a, unsigned int* b, unsigned int* c, unsigned int* d)
	{
		*a = *b = *c = *d = 0;
	}
}

static uint64_t ReadFpcr()
{
	streflop::fpenv_t env;
	streflop::fegetenv(&env);
	return env.fpcr;
}

static void SetFpcr(uint64_t bits)
{
	const streflop::fpenv_t env{bits};
	streflop::fesetenv(&env);
}

static void CheckThread()
{
	const uint64_t original = ReadFpcr();
	constexpr uint64_t mask = uint64_t(streflop::FE_ROUND_MASK) | uint64_t(streflop::FE_FLUSH_TO_ZERO);
#ifdef STREFLOP_NO_DENORMALS
	constexpr uint64_t expected = streflop::FE_FLUSH_TO_ZERO;
#else
	constexpr uint64_t expected = streflop::FE_TONEAREST;
#endif
	good_fpu_init();
	assert((ReadFpcr() & mask) == expected);
	const uint64_t initialized = ReadFpcr();
	for (const auto mode : {streflop::FE_UPWARD, streflop::FE_DOWNWARD, streflop::FE_TOWARDZERO}) {
		SetFpcr((initialized & ~uint64_t(streflop::FE_ROUND_MASK)) | uint64_t(mode));
		assert((ReadFpcr() & mask) != expected);
		good_fpu_control_registers("rounding drift diagnostic");
		assert((ReadFpcr() & mask) == expected);
	}
	SetFpcr(initialized ^ uint64_t(streflop::FE_FLUSH_TO_ZERO));
	assert((ReadFpcr() & mask) != expected);
	good_fpu_control_registers("flush drift diagnostic");
	assert((ReadFpcr() & mask) == expected);
	volatile float small = std::bit_cast<float>(uint32_t(0x00400000));
	volatile float two = 2.0f;
	const float result = small * two;
#ifdef STREFLOP_NO_DENORMALS
	assert(std::bit_cast<uint32_t>(result) == 0);
#else
	assert(std::bit_cast<uint32_t>(result) == 0x00800000);
#endif
	// Preserve fields outside upstream streflop's policy, including Default NaN.
	SetFpcr(initialized | (uint64_t(1) << 25));
	good_fpu_control_registers("preserved FPCR fields diagnostic");
	assert((ReadFpcr() & (uint64_t(1) << 25)) != 0);
	SetFpcr(original);
}

int main()
{
	const uint64_t mainOriginal = ReadFpcr();
	CheckThread();
	assert(ReadFpcr() == mainOriginal);
	std::thread worker(CheckThread);
	worker.join();
	assert(ReadFpcr() == mainOriginal);
	assert(warnings == 8);
	std::printf("FPCR initialization + 8 drift restorations across main/worker threads passed; initial FPCR=%08llX\n", static_cast<unsigned long long>(mainOriginal));
}
