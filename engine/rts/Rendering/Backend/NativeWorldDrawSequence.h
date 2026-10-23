/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace rendering {
// Actual successful unsynced draw-call order across independent typed families.
// Never reset between recorders or frames: owned frame/stage DTOs select which
// ordinals are consumed. Gaps after an allocation failure do not reorder draws.
inline std::uint64_t NextNativeWorldDrawSequence()
{
	static std::atomic<std::uint64_t> next{1};
	auto value = next.load(std::memory_order_relaxed);
	for (;;) {
		if (value == std::numeric_limits<std::uint64_t>::max())
			throw std::overflow_error("Native world draw sequence exhausted");
		if (next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))
			return value;
	}
}
}
