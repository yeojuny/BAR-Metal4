/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

// Concatenated after the original single-sample shader text. The old diamond
// path stays byte-identical; only explicit MSAA entry points call this helper.
#define RECOIL_NATIVE_MULTISAMPLE_LINE_MSL R"MSL(
#ifndef RECOIL_ORIGINAL_MULTISAMPLE_LINE_COVERAGE
#define RECOIL_ORIGINAL_MULTISAMPLE_LINE_COVERAGE
inline float2 OriginalMultisampleFragmentPosition(float4 position, float2 samplePosition) {
	return floor(position.xy) + samplePosition;
}
inline bool OriginalMultisampleLineCovered(float3 a, float3 b, float2 samplePosition,
	float width, thread float& depth) {
	// OpenGL4.6 sections14.5.2.3/14.5.4: floating-width rectangle with
	// perpendicular end caps. Coverage and depth are evaluated at each actual
	// sample, independently of the original one-sample diamond-exit rule.
	const float2 delta = b.xy - a.xy;
	const float lengthSquared = dot(delta, delta);
	depth = 0.0f;
	if (!(lengthSquared > 0.0f) || !isfinite(lengthSquared) || !(width > 0.0f) ||
		!isfinite(width) || !all(isfinite(a)) || !all(isfinite(b)) || !all(isfinite(samplePosition))) return false;
	const float2 relative = samplePosition - a.xy;
	const float along = dot(relative, delta);
	const float cross = relative.x * delta.y - relative.y * delta.x;
	const float halfWidth = width * 0.5f;
	const float t = along / lengthSquared;
	// Equation14.7 uses the projection t (14.5), not a major-axis ratio.
	depth = a.z + (b.z - a.z) * t;
	return along >= 0.0f && along <= lengthSquared &&
		cross * cross <= halfWidth * halfWidth * lengthSquared &&
		depth >= 0.0f && depth <= 1.0f;
}
#endif
)MSL"
