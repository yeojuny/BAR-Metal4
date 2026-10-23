/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

// The scalar predicate is shared with the embedded MSL source. CPU tests use
// an independent rational oracle, not this function, to calculate expectations.
// This is an ideal region rule, not a claim about GL hardware boundary setup.
#define RECOIL_NATIVE_POINT_DISK_BODY \
	const float dx = sampleX - centerX; \
	const float dy = sampleY - centerY; \
	return dx * dx + dy * dy <= radius * radius;

// Metal framebuffer coordinates: top/left inclusive, bottom/right exclusive.
#define RECOIL_NATIVE_POINT_SQUARE_BODY \
	return sampleX >= centerX - radius && sampleX < centerX + radius && \
		sampleY >= centerY - radius && sampleY < centerY + radius;

namespace rendering {
inline bool NativePointSquareContains(float sampleX, float sampleY, float centerX, float centerY, float radius)
{
	RECOIL_NATIVE_POINT_SQUARE_BODY
}

inline bool NativePointDiskContains(float sampleX, float sampleY, float centerX, float centerY, float radius)
{
	RECOIL_NATIVE_POINT_DISK_BODY
}
}
