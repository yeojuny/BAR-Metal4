/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>
#include <limits>

namespace rendering::video_memory {

enum class Reference { LegacyGL, MetalRecommendedWorkingSet, MetalUnknown };

struct UsageMiB {
	double used;
	double reference;
	Reference kind;
};

struct MetalSample {
	std::uint64_t recommendedWorkingSetBytes;
	std::uint64_t currentAllocatedBytes;
};

// Exact opt-in, sampled once by the caller; never a renderer/allocator selection.
constexpr bool OptedIn(const char* value) noexcept
{
	return value != nullptr && value[0] == '1' && value[1] == '\0';
}

// Existing Platform.gpuMemorySize is signed int KiB, despite the old LuaDoc.
// Keep the raw device values 64-bit and divide BEFORE saturating/narrowing.
constexpr int LegacyBudgetKiB(std::uint64_t budgetBytes) noexcept
{
	const auto kib = budgetBytes / 1024u;
	const auto limit = static_cast<std::uint64_t>(std::numeric_limits<int>::max());
	return static_cast<int>(kib > limit ? limit : kib);
}

// The exposed Lua number must not round a budget above the device's byte count.
template<class Number>
Number LuaBudgetKiB(int budgetKiB) noexcept
{
    static_assert(std::is_floating_point_v<Number>);
    if (budgetKiB <= 0)
        return Number{0};
    const auto rounded = static_cast<Number>(budgetKiB);
    if (static_cast<long double>(rounded) > static_cast<long double>(budgetKiB))
        return std::nextafter(rounded, Number{0});
    return rounded;
}

constexpr UsageMiB FromMetal(MetalSample sample) noexcept
{
	if (sample.recommendedWorkingSetBytes < 1024u)
		return {0.0, 0.0, Reference::MetalUnknown};
	constexpr double bytesPerMiB = 1024.0 * 1024.0;
	// A performance recommendation, NOT a hard cap: allocation can exceed it.
	return {sample.currentAllocatedBytes / bytesPerMiB,
	        sample.recommendedWorkingSetBytes / bytesPerMiB,
	        Reference::MetalRecommendedWorkingSet};
}

// Existing GL contract: library reports total/free in KiB and the two Lua
// values historically rounded as float MiB. No interpretation change on GL.
constexpr UsageMiB FromLegacyGL(int totalKiB, int freeKiB) noexcept
{
	const auto usedKiB = static_cast<std::int64_t>(totalKiB) - freeKiB;
	return {static_cast<float>(usedKiB) / 1024.0f, totalKiB / 1024.0f, Reference::LegacyGL};
}

UsageMiB QueryUsageMiB();

} // namespace rendering::video_memory
