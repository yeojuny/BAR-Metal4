/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeFlamethrowerData.h"
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
using NativeFireGL4Uniforms = NativeFlamethrowerUniforms;
using NativeFireGL4UniformValues = NativeFlamethrowerUniformValues;
struct NativeFireGL4Draw {
	std::uint64_t sequence = 0;
	NativeWorldDrawStage stage = NativeWorldDrawStage::World;
	NativeBufferSnapshotRef vertices, instances;
	std::uint32_t firstVertex = 0, vertexCount = 6;
	std::uint32_t firstInstance = 0, instanceCount = 0;
	// Values are parsed from the original Lua-generated GLSL define literals.
	std::array<float,13> defines{};
	std::array<float,3> windVelocity{};
	std::array<std::shared_ptr<const ScreenTexture>,2> textures;
	std::array<ScreenSampler,2> samplers;
	ScreenDrawState surface;
	NativeWorldDrawState world;
};
struct NativeFireGL4Frame {
	NativeFireGL4Uniforms uniforms;
	std::vector<NativeFireGL4Draw> draws;
};
NativeFireGL4UniformValues ReadNativeFireGL4Uniforms(const NativeFireGL4Uniforms&);
void ValidateNativeFireGL4Draw(const NativeFireGL4Draw&);
class NativeFireGL4Recorder {
public:
	void Begin(NativeFireGL4Uniforms uniforms);
	void Record(NativeFireGL4Draw draw);
	std::shared_ptr<const NativeFireGL4Frame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeFireGL4Frame> frame;
};
// Kept false until the complete actual consumer and source-stage hooks exist.
void SetNativeFireGL4ConsumerAvailable(bool) noexcept;
bool IsNativeFireGL4ConsumerAvailable() noexcept;
}
