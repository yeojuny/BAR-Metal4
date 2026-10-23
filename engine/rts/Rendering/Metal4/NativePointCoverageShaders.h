/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePointCoverage.h"

#define RECOIL_POINT_STRING_IMPL(value) #value
#define RECOIL_POINT_STRING(value) RECOIL_POINT_STRING_IMPL(value)
#define RECOIL_NATIVE_POINT_COVERAGE_MSL \
"inline bool RecoilPointDiskContains(float sampleX, float sampleY, float centerX, float centerY, float radius) {" \
RECOIL_POINT_STRING(RECOIL_NATIVE_POINT_DISK_BODY) "}\n" \
"inline bool RecoilPointSquareContains(float sampleX, float sampleY, float centerX, float centerY, float radius) {" \
RECOIL_POINT_STRING(RECOIL_NATIVE_POINT_SQUARE_BODY) "}\n" \
R"POINTMSL(
// disk = physical Metal center.xy, radius, signed attachment sample count.
// Positive selects a plain MSAA disk; negative selects an unsnapped sprite square.
// A zero count denotes an ordinary triangle; no point mask is then imposed.
uint RecoilPointSampleMask(float2 fragmentCenter, float4 disk) {
	if (disk.w == 0.0f) return ~0u;
	uint mask = 0u;
	const float2 pixel = floor(fragmentCenter);
	for (uint sample = 0u; sample < uint(abs(disk.w)); ++sample) {
		const float2 at = pixel + get_sample_position(sample);
		if (disk.w < 0.0f
			? RecoilPointSquareContains(at.x, at.y, disk.x, disk.y, disk.z)
			: RecoilPointDiskContains(at.x, at.y, disk.x, disk.y, disk.z))
			mask |= 1u << sample;
	}
	return mask;
}
)POINTMSL"
