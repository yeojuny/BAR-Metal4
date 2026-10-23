/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

// Embedded only in explicitly reviewed native shader families. A pipeline's
// selected library defines RECOIL_NATIVE_SAMPLE_SHADING before this prelude.
// Flat/centroid declarations are retained at their source sites.
#define RECOIL_NATIVE_SAMPLE_SHADING_MSL R"SAMPLEMSL(
#ifndef RECOIL_NATIVE_SAMPLE_SHADING_PRELUDE
#define RECOIL_NATIVE_SAMPLE_SHADING_PRELUDE 1
#define RECOIL_NATIVE_SAMPLE_SHADING_ANNOTATED 1
#include <metal_stdlib>
using namespace metal;
#ifndef RECOIL_NATIVE_SAMPLE_SHADING
#define RECOIL_NATIVE_SAMPLE_SHADING 0
#endif
#if RECOIL_NATIVE_SAMPLE_SHADING
#define RECOIL_SAMPLE_PERSPECTIVE sample_perspective
#define RECOIL_SAMPLE_NOPERSPECTIVE sample_no_perspective
#define RECOIL_SAMPLE_ID_ARGUMENT , uint recoilOriginalSampleID [[sample_id]]
#define RECOIL_SAMPLE_POSITION_ARGUMENT
// Metal's built-in position remains center_no_perspective even for a sample
// function. Its depth and reciprocal-W are screen-linear planes. Evaluate those
// planes at the actual sample; xy uses the hardware sample position explicitly.
inline float4 RecoilOriginalSamplePosition(float4 center, float2 samplePosition) {
	const float2 offset=samplePosition-float2(0.5f);
	float4 result=center+dfdx(center)*offset.x+dfdy(center)*offset.y;
	result.xy=floor(center.xy)+samplePosition;
	return result;
}
#define RECOIL_SAMPLE_FIX_POSITION(value, samplePosition) value=RecoilOriginalSamplePosition(value,samplePosition)
#else
#define RECOIL_SAMPLE_PERSPECTIVE center_perspective
#define RECOIL_SAMPLE_NOPERSPECTIVE center_no_perspective
#define RECOIL_SAMPLE_ID_ARGUMENT
#define RECOIL_SAMPLE_POSITION_ARGUMENT
#define RECOIL_SAMPLE_FIX_POSITION(value, samplePosition)
#endif
#endif
)SAMPLEMSL"
