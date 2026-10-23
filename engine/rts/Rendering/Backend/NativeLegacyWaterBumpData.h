/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace rendering {
// Bytes must come from CAdvWater's original raw-bump construction, including
// its fastmath and byte conversion behavior. No native replacement waveform.
struct NativeLegacyWaterBumpMaterial {
	uint64_t generation = 0;
	std::array<std::array<uint8_t, 64 * 64 * 4>, 3> rgba;
};
enum class LegacyWaterBumpCull { None, Front, Back, FrontAndBack };
struct NativeLegacyWaterBumpFrame {
	uint64_t sourceDrawFrame = 0;
	int simulationFrame = 0;
	bool sourceStateObserved = false;
	bool fogEnabled = false;
	std::array<float, 4> fogColor{}, fogParameters{0, 1, 1, 1};
	LegacyWaterBumpCull cull = LegacyWaterBumpCull::None;
	bool frontFaceClockwise = false, wireframe = false;
	// Scissored glClear retains previous bump pixels; no substitute full clear.
	bool scissorEnabled = false;
	std::shared_ptr<const NativeLegacyWaterBumpMaterial> material;
};
inline void ValidateNativeLegacyWaterBump(const NativeLegacyWaterBumpFrame& frame)
{
	if (!frame.sourceStateObserved)
		throw std::invalid_argument("Legacy water bump lacks actual inherited source state");
	if (frame.scissorEnabled)
		throw std::invalid_argument("Legacy water scissored bump requires prior-target preservation");
	if (frame.fogEnabled) {
		for (float v: frame.fogParameters)
			if (!std::isfinite(v)) throw std::invalid_argument("Legacy water bump fog range is not finite");
		for (float v: frame.fogColor)
			if (!std::isfinite(v)) throw std::invalid_argument("Legacy water bump fog color is not finite");
		if (frame.fogParameters[0] == frame.fogParameters[1])
			throw std::invalid_argument("Legacy water bump fog range is empty");
	}
	if (!frame.material || frame.material->generation == 0)
		throw std::invalid_argument("Legacy water bump has no immutable source material generation");
}
}
