/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
namespace rendering {
// Availability is published only by the actual native terrain/SMF-shading
// pipeline owner. It is independent of GL4 and the user's active shading mode.
inline std::atomic<bool> nativeTerrainShadingConsumerAvailable{false};
inline bool IsNativeTerrainShadingConsumerAvailable() noexcept { return nativeTerrainShadingConsumerAvailable.load(); }
inline void SetNativeTerrainShadingConsumerAvailable(bool value) noexcept { nativeTerrainShadingConsumerAvailable.store(value); }
}
