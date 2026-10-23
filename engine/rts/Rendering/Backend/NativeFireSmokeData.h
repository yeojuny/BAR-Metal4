/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/ScreenDrawList.h"
#include <array>
#include <memory>
#include <vector>

namespace rendering {
// Exact test30770 FireSmokeGL4 program. The original gadget still owns all
// emission, budgeting, visibility, pause, lifetime and offscreen replay logic.
struct NativeFireSmokeDraw {
	std::uint64_t sequence = 0;
	NativeBufferSnapshotRef vertices, indices, instances;
	std::uint32_t instanceCount = 0;
	bool indexed = false;
	std::array<float,8> defines{};
	std::array<float,3> windVelocity{};
	std::array<float,4> highlightPass{};
	std::array<std::shared_ptr<const ScreenTexture>,2> textures;
};
struct NativeFireSmokeFrame {
	std::array<float,16> cameraViewProjection{}, cameraViewInverse{};
	// Original UniformConstants, including fractional simulation interpolation.
	std::array<float,4> timeInfo{};
	std::vector<NativeFireSmokeDraw> draws;
};
void ValidateNativeFireSmokeDraw(const NativeFireSmokeDraw& draw);
}
