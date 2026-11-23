/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/NativeGroundDecalData.h"
#include <algorithm>
#include <cmath>

namespace rendering::metal4 {

struct GroundDecalExpiredTrackPrefix {
	std::size_t rows = 0;
	double validFrom = 0.0;

	bool CanReuse(float previousAdjustedFrame, float adjustedFrame) const noexcept
	{
		return rows == 0 || (std::isfinite(adjustedFrame) &&
			adjustedFrame >= previousAdjustedFrame && double(adjustedFrame) >= validFrom);
	}
};

inline GroundDecalExpiredTrackPrefix FindGroundDecalExpiredTrackPrefix(
	std::span<const NativeGroundDecalInstance> rows, float adjustedFrame) noexcept
{
	if (!std::isfinite(adjustedFrame))
		return {};

	constexpr uint32_t trackType = 3u;
	constexpr float trackDecay = 1.0f / 900.0f;
	constexpr float lastWholeFrame = 0x1p24f;
	GroundDecalExpiredTrackPrefix prefix;
	for (const auto& row: rows) {
		const float lastCreated = row.createParams3[1];
		if ((row.createParams5[0] & 0xFu) != trackType ||
		    row.createParams1[0] != 1.0f || row.createParams1[1] != trackDecay ||
		    row.createParams4[3] != 1.0f || !(lastCreated >= 0.0f && lastCreated <= lastWholeFrame))
			break;

		// One full frame of margin makes alphaMax strictly negative in the
		// binary32 vertex expression, with or without contraction.
		const double expires = double(lastCreated) + 901.0;
		if (double(adjustedFrame) < expires)
			break;
		++prefix.rows;
		prefix.validFrom = std::max(prefix.validFrom, expires);
	}
	return prefix;
}

}
