/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>

namespace rendering {
// Rendering-only metadata; does not alter or serialize simulation state.
void InvalidateNativeMetalDensity() noexcept;
std::uint64_t NativeMetalDensityRevision() noexcept;
}
