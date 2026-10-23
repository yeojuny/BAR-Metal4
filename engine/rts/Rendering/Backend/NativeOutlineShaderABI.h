/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace rendering::outlineProgram {
inline constexpr unsigned CornerBufferSlot=0, InstanceBufferSlot=1, MatrixBufferSlot=2, ParamsBufferSlot=3,
	ModelTransformBufferSlot=4, ModelUniformBufferSlot=5, DrawUniformBufferSlot=6;
inline constexpr unsigned AllUnitsInstanceStride=32, PerUnitInstanceStride=80;
inline constexpr unsigned MatrixBytes=1664, ParamsDeclaredBytes=4368;
// Retain full actual 33056-byte engine params backing, not just GLSL's prefix.
struct alignas(16) DrawUniforms {
	float outlineWidth=0, quadPadding=0, zoomScale=0, resolutionScale=0;
	// x=sampler Y-flip bits, y=clip flags (bit0 GL -1..1 Z, bit1 invert Y).
	std::array<std::uint32_t,4> transport{};
	// xy=fragment pixel scale, z=actual native framebuffer height.
	std::array<float,4> framebuffer{};
	// Original alpha enable, reference and ScreenAlphaCompare enum.
	std::array<float,4> alpha{};
	std::array<std::array<std::uint32_t,4>,3> imageSwizzle{};
	std::array<std::uint32_t,4> depthMode{};
	std::array<std::array<std::uint32_t,4>,3> imageSize{};
};
static_assert(sizeof(DrawUniforms)==176);
static_assert(offsetof(DrawUniforms,transport)==16);
static_assert(offsetof(DrawUniforms,imageSwizzle)==64);
static_assert(offsetof(DrawUniforms,depthMode)==112);
static_assert(offsetof(DrawUniforms,imageSize)==128);
} // namespace rendering::outlineProgram
