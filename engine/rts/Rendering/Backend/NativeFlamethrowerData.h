/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeCUSUniformData.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <array>
#include <memory>
#include <vector>

namespace rendering {
// The complete original gadget owns projectile/LOS dispatch, particle budgets,
// pause and replay, scavenger colors, wind, emission and lifetime queues.
// This packet retains the actual source buffers and original upload publication.
struct NativeFlamethrowerUniforms {
	std::uint64_t sourceDrawFrame = 0;
	std::shared_ptr<const NativeCUSUniformPublication> original;
};
struct NativeFlamethrowerUniformValues {
	std::array<float,16> cameraViewProjection{}, cameraViewInverse{};
	std::array<float,4> timeInfo{};
	ClipDepthConvention clipDepth = ClipDepthConvention::Unsupported;
};
struct NativeFlamethrowerDraw {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::World;
	// game31657's helper selects an indexed rectangle. Ring holes, expired
	// rows and in-place kill writes stay in their original slot order; instances
	// is the actual usedElements range, not a reconstructed live-particle list.
	NativeBufferSnapshotRef vertices, indices, instances;
	std::uint32_t firstIndex = 0, indexCount = 6;
	std::uint32_t firstInstance = 0, instanceCount = 0;
	// Values are parsed from the original Lua-generated GLSL define literals.
	std::array<float,12> defines{};
	std::array<float,3> windVelocity{};
	std::array<std::shared_ptr<const ScreenTexture>,2> textures;
	std::array<ScreenSampler,2> samplers;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeFlamethrowerFrame {
	NativeFlamethrowerUniforms uniforms;
	std::vector<NativeFlamethrowerDraw> draws;
};
NativeFlamethrowerUniformValues ReadNativeFlamethrowerUniforms(const NativeFlamethrowerUniforms&);
void ValidateNativeFlamethrowerDraw(const NativeFlamethrowerDraw&);
class NativeFlamethrowerRecorder {
public:
	void Begin(NativeFlamethrowerUniforms uniforms);
	void Record(NativeFlamethrowerDraw draw);
	std::shared_ptr<const NativeFlamethrowerFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeFlamethrowerFrame> frame;
};
// Kept false until the complete actual consumer and source-stage hooks exist.
void SetNativeFlamethrowerConsumerAvailable(bool) noexcept;
bool IsNativeFlamethrowerConsumerAvailable() noexcept;
}
