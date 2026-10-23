/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include <array>
#include <map>
namespace rendering {
// Original test30770 energy-burst NoGS stream. No engine particle substitution.
struct NativeEnergyExplosionDraw {
	std::uint64_t sequence=0;
	NativeBufferSnapshotRef vertexSlots,indices,instances;
	std::uint32_t instanceCount=0,firstInstance=0;
	std::map<std::string,std::vector<float>> uniforms;
};
struct NativeEnergyExplosionFrame {
	std::array<float,16> cameraViewProjection{},cameraViewInverse{};
	std::array<float,4> timeInfo{};
	std::vector<NativeEnergyExplosionDraw> draws;
};
void ValidateNativeEnergyExplosionDraw(const NativeEnergyExplosionDraw&);
// The exact original GS emits strip-ordered faces and halo vertices. The Lua
// NoGS index stream is retained above; this consumer-only expansion reproduces
// GS triangles (including their winding) instead of its incorrect quad diagonal.
std::vector<std::uint16_t> NativeEnergyExplosionGSIndices(const NativeEnergyExplosionDraw&);
void SetNativeEnergyExplosionConsumerAvailable(bool) noexcept;
bool IsNativeEnergyExplosionConsumerAvailable() noexcept;
}
