/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <limits>

// Match the masked x86 CVTTSS2SI value for this COB getter.
// The range test precedes the cast so overflow and NaN have defined C++ behavior.
inline int ShieldPowerToCobInt(float power, float cobScale)
{
	static_assert(std::numeric_limits<int>::digits == 31);

	const float scaledPower = power * cobScale;
	constexpr float minInt = -0x1p31f;
	constexpr float maxIntExclusive = 0x1p31f;

	if (!(scaledPower >= minInt && scaledPower < maxIntExclusive))
		return std::numeric_limits<int>::min();

	return static_cast<int>(scaledPower);
}
