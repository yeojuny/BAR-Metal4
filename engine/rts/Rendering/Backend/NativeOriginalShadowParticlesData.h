/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldParticleData.h"
#include "NativeWorldDrawData.h"
#include "NativeCUSTextureHistory.h"

namespace rendering {
// Owned only by the actual DrawShadowTransparent indexed submission. This
// carries its real pending index range, original shader and current sampler0;
// it never reconstructs geometry or substitutes the projectile atlas binding.
struct NativeOriginalShadowParticlesDraw {
	std::uint64_t sequence = 0;
	std::array<float,16> view{}, projection{};
	bool zeroToOneDepth = false;
	NativeWorldDrawState world;
	ScreenDrawState surface;
	NativeCUSTextureBinding texture;
	std::array<float,4> alphaControl{};
	float shadowColorMode = 1.f;
	bool textureArray = false;
	std::shared_ptr<const void> compiledOriginal;
	std::vector<NativeWorldParticleVertex> vertices;
	std::vector<std::uint32_t> indices;
};
struct NativeOriginalShadowParticlesFrame {
	std::uint64_t sourceDrawFrame = 0;
	std::uint32_t size = 0;
	bool colored = true;
	std::vector<NativeOriginalShadowParticlesDraw> draws;
};
}
