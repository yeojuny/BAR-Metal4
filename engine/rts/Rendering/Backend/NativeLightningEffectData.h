/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeProjectileEffectData.h"
#include "Rendering/Backend/NativeWorldDrawData.h"

namespace rendering {
enum class NativeLightningProgram : std::uint8_t {
	CannonBolt, CannonGlow, CannonImpact, EnvironmentBolt, EnvironmentGlow,
};
// Original31657 six-vec4 arm stream (96 bytes) and indexed segment strip.
// Body passes draw the strip; glow/impact passes draw only its first quad.
struct NativeLightningEffectDraw {
	std::uint64_t sequence = 0;
	NativeLightningProgram program = NativeLightningProgram::CannonBolt;
	NativeBufferSnapshotRef vertices, indices, instances;
	std::uint32_t instanceCount = 0, firstInstance = 0, indexCount = 0;
	bool indexed = false;
	std::map<std::string, float> parameters;
	std::shared_ptr<const NativeProjectileTexture> texture;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeLightningEffectFrame {
	std::array<float,16> cameraViewProjection{}, cameraViewInverse{};
	std::array<float,4> timeInfo{};
	std::vector<NativeLightningEffectDraw> draws;
};
void ValidateNativeLightningEffectDraw(const NativeLightningEffectDraw& draw);
void SetNativeLightningConsumerAvailable(bool available) noexcept;
bool IsNativeLightningConsumerAvailable() noexcept;
}
