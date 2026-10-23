/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeBufferData.h"
#include "Rendering/Backend/ScreenDrawList.h"
#include <array>
#include <memory>
#include <vector>

namespace rendering {
// Exact test30770 four MissileThrusterGL4 programs. The original gadget still owns all
// emission, budgeting, visibility, pause, lifetime and offscreen replay logic.
enum class NativeMissileThrusterProgram : std::uint8_t { Flame, CrossFlame, CrossSection, Glow, Flame31657, CrossSection31657, Glow31657 };
struct NativeMissileThrusterDraw {
	NativeMissileThrusterProgram program = NativeMissileThrusterProgram::Flame;
	std::uint64_t sequence = 0;
	NativeBufferSnapshotRef vertices, indices, instances;
	std::uint32_t instanceCount = 0, vertexCount = 6;
	bool indexed = false;
	std::array<float,12> defines{};
	std::shared_ptr<const ScreenTexture> texture;
};
struct NativeMissileThrusterFrame {
	std::array<float,16> cameraViewProjection{}, cameraViewInverse{};
	// Original UniformConstants, including fractional simulation interpolation.
	std::array<float,4> timeInfo{};
	std::vector<NativeMissileThrusterDraw> draws;
};
void ValidateNativeMissileThrusterDraw(const NativeMissileThrusterDraw& draw);
}
