/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rendering::deferredLightProgram {

inline constexpr unsigned MeshBufferSlot = 0;
inline constexpr unsigned InstanceBufferSlot = 1;
inline constexpr unsigned MatrixBufferSlot = 2;
inline constexpr unsigned ParamsBufferSlot = 3;
inline constexpr unsigned ModelTransformBufferSlot = 4;
inline constexpr unsigned ModelUniformBufferSlot = 5;
inline constexpr unsigned DrawUniformBufferSlot = 6;
inline constexpr unsigned InstanceStride = 116;
inline constexpr unsigned MatrixBytes = 26 * 64;
// Required declaration prefix; retain the whole actual 2048-team-row UBO owner.
inline constexpr unsigned ParamsDeclaredBytes = 18 * 16 + 255 * 16;
inline constexpr unsigned ModelUniformStride = 128;
inline constexpr unsigned QuaternionStride = 32;
inline constexpr unsigned ModelMatrixStride = 64;

// Field order matches NativeDeferredLightDrawUniforms in the generated Metal.
// Texture slots are sampler names, not resolved OpenGL texture units. Each
// binding is resolved from that sampler's captured original integer value.
struct alignas(16) DrawUniforms {
	float pointbeamcone = 0;
	float attachedtounitID = 0;
	float windX = 0;
	float windZ = 0;
	float radiusMultiplier = 1;
	float intensityMultiplier = 1;
	float nightFactor = 1;
	std::int32_t screenSpaceShadows = 0;
	// x=image Y-flip mask; y=clip flags (bit0: -1..1 -> 0..1, bit1: invert Y);
	// z=native base instance, subtracted for original gl_InstanceID; w=reserved.
	std::array<std::uint32_t, 4> transport{};
	// Logical/native pixel scale.xy, native target height.z, reserved.w.
	std::array<float, 4> framebuffer{};
	// Original alpha-test enable, reference, comparison enum [Never..Always], pad.
	std::array<float, 4> alpha{};
	std::array<std::array<std::uint32_t, 4>, 10> imageSwizzle{};
	// Original GL_DEPTH_TEXTURE_MODE, indexed by sampler slot / 4 and % 4.
	std::array<std::array<std::uint32_t, 4>, 3> depthMode{};
	// Actual original texture width, height, depth, mip-count for each sampler.
	std::array<std::array<std::uint32_t, 4>, 10> imageSize{};
};

static_assert(sizeof(DrawUniforms) == 448);
static_assert(offsetof(DrawUniforms, transport) == 32);
static_assert(offsetof(DrawUniforms, imageSwizzle) == 80);
static_assert(offsetof(DrawUniforms, depthMode) == 240);
static_assert(offsetof(DrawUniforms, imageSize) == 288);

} // namespace rendering::deferredLightProgram
