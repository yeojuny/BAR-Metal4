/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace rendering {
// The original legacy ModelFragProg reads these derived FFP uniform values,
// not the unresolved Lua light parameters. World positions use identity MV.
struct alignas(16) NativeModelLight {
	std::array<float,4> position{};
	std::array<float,4> halfVector{0,0,1,1};
	std::array<float,4> directionCutoff{0,0,0,0};
	std::array<float,4> ambient{}, diffuse{}, specular{};
	std::array<float,4> attenuation{1,0,0,0};
};
static_assert(sizeof(NativeModelLight) == 112);
struct NativeModelLightFrame {
	uint64_t sourceDrawFrame = 0;
	int32_t simulationFrame = 0;
	uint32_t baseLight = 0, configuredLights = 0;
	std::vector<NativeModelLight> lights;
};
// Mirror the original identity-MV Light updates. The GL cutoff setter rejects
// (90,180) and preserves the previous accepted state; it does not clamp to90.
void SetNativeModelLightPosition(NativeModelLight&, const float*);
void SetNativeModelLightDirection(NativeModelLight&, const float*);
void SetNativeModelLightCutoff(NativeModelLight&, float);
void ValidateNativeModelLightFrame(const NativeModelLightFrame&);
// Header is one uint4(count,reserved...) followed by the exact aligned records.
// Empty frames still provide a valid buffer and execute no lighting loop.
std::vector<uint8_t> PackNativeModelLightFrame(const NativeModelLightFrame&);
}
