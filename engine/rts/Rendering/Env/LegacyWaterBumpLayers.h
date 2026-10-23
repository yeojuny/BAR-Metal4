/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>

namespace rendering {

// The five ordered quads in CAdvWater::UpdateWater, shared by Water1 and
// Water2. This is source-owned draw data, not a Water4 approximation.
struct LegacyWaterBumpLayer {
	unsigned int rawTextureIndex;
	float uScale;
	float vScale;
	float vOffset;
};

inline std::array<LegacyWaterBumpLayer, 5> MakeLegacyWaterBumpLayers(int frameNum)
{
	return {{
		{0, 2.0f, 2.0f, frameNum * 0.0046f},
		{0, 2.0f, 4.0f, frameNum * 0.0026f},
		{0, 2.0f, 8.0f, frameNum * 0.0012f},
		{1, 1.0f, 1.0f, frameNum * 0.0036f},
		{2, 1.0f, 1.0f, frameNum * 0.0082f},
	}};
}

}
