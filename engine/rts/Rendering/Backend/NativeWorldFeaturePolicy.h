/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>

namespace rendering {
struct NativeFeaturePasses { bool opaque, fading, shadow; };

// Viewer/frustum/distance/LOS/void policy remains in FeatureDrawerData. This
// only classifies its finished flags using the original ordinary pass masks,
// without ShouldDraw*'s mutation of Lua material queues.
constexpr NativeFeaturePasses ClassifyNativeFeaturePasses(uint8_t flags, uint8_t engineMask)
{
	constexpr uint8_t OPAQUE = 1, ALPHA = 2, SHADOW_OPAQUE = 16;
	return {
		(flags & OPAQUE) != 0 && (flags & ALPHA) == 0 && (engineMask & OPAQUE) != 0,
		(flags & ALPHA) != 0 && (flags & OPAQUE) == 0 && (engineMask & ALPHA) != 0,
		(flags & SHADOW_OPAQUE) != 0 && (engineMask & SHADOW_OPAQUE) != 0,
	};
}
} // namespace rendering
