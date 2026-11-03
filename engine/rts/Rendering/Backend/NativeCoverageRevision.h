/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
namespace rendering {
void InvalidateNativeCoverage(const void* owner) noexcept;
std::uint64_t WatchNativeCoverage(unsigned slot, const void* owner) noexcept;
void ResetNativeCoverageWatches() noexcept;
}
