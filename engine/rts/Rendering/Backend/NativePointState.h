/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

namespace rendering {
// Generic fixed-function points. Defaults match LuaOpenGL::ResetGLState;
// MULTISAMPLE and POINT_SMOOTH are separate enables, not attachment counts.
// Shader point outputs and sprite coordinate replacement remain typed-family
// operations and must not silently fall through this fixed-function path.
struct NativePointState {
	bool sprite = false;
	bool smooth = false;
	bool multisample = true;
	std::array<float,3> attenuation{1,0,0};
	float sizeMin = 0, sizeMax = 1.0e9f, fadeThreshold = 1;
	bool operator==(const NativePointState&) const = default;
};

inline void ValidateNativePointState(const NativePointState& point)
{
	for (float value: point.attenuation)
		if (!std::isfinite(value) || value < 0)
			throw std::runtime_error("Native point attenuation must be finite and nonnegative");
	for (float value: {point.sizeMin,point.sizeMax,point.fadeThreshold})
		if (!std::isfinite(value) || value < 0)
			throw std::runtime_error("Native point clamp/fade must be finite and nonnegative");
}

inline void RequireNativeFixedPointContract(const NativePointState& point, unsigned samples)
{
	ValidateNativePointState(point);
	if (point.attenuation != std::array<float,3>{1,0,0} || point.sizeMin != 0 ||
		point.sizeMax != 1.0e9f || point.fadeThreshold != 1)
		throw std::runtime_error("Generic point attenuation/clamp/fade requires its original effective-size contract");
	if (samples > 1 && !point.multisample)
		throw std::runtime_error("Generic point MULTISAMPLE-disabled rendering into a multisample attachment is unsupported");
	if (samples == 1 && point.smooth && !point.sprite)
		throw std::runtime_error("Generic smooth single-sample points require original area coverage");
}

inline void ResetNativePointParameters(NativePointState& point)
{
	// ResetGLState does not change POINT_SMOOTH or MULTISAMPLE.
	point.sprite = false;
	point.attenuation = {1,0,0};
	point.sizeMin = 0; point.sizeMax = 1.0e9f; point.fadeThreshold = 1;
}
}
