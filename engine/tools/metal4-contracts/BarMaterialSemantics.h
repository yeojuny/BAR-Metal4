/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Material equations adapted from BAR cus_gl4.frag.glsl, Copyright (c) 2025
// Beherith (mysterme@gmail.com), MIT license. See README.md for exact source pins.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace spring::metal4::bar {

using Rgb = std::array<float, 3>;
using Rgba = std::array<float, 4>;

inline constexpr float MIN_ROUGHNESS = 0.04f;
inline constexpr float DEFAULT_F0 = 0.04f;
inline constexpr float EPSILON = 1e-4f;
inline constexpr std::array<float, 5> DEFAULT_TONEMAP = {4.75f, 0.75f, 3.5f, 0.85f, 1.0f};

// Value contract only; this is not a shared C++/MSL struct memory layout.
struct Material {
	Rgb albedo;
	float emissiveness;
	float metalness;
	float roughness;
	float alpha;
	float alphaBin;
};

inline Rgb ToLinear(Rgb color, float modelGamma)
{
	if (modelGamma != 1.0f) {
		for (float& channel: color)
			channel = std::pow(channel, modelGamma);
	}
	return color;
}

inline Rgb FromLinear(Rgb color, float modelGamma)
{
	if (modelGamma != 1.0f) {
		for (float& channel: color)
			channel = std::pow(channel, 1.0f / modelGamma);
	}
	return color;
}

// Base texture-driven specialization. Inputs must be finite; color/gamma domain
// must be valid for pow. Damage, override defines, normal AA and other options
// require their own variants. Gamma is BAR's power law, not IEC sRGB decoding.
inline Material DecodeMaterial(const Rgba& texture1, const Rgba& texture2, const Rgb& teamColor, float selfIllumination = 1.0f, float modelGamma = 1.0f)
{
	Rgb color;
	for (std::size_t i = 0; i < color.size(); ++i)
		color[i] = texture1[i] * (1.0f - texture1[3]) + teamColor[i] * texture1[3];
	return {
		ToLinear(color, modelGamma),
		std::clamp(texture2[0] * selfIllumination, 0.0f, 1.0f),
		std::clamp(texture2[1], 0.0f, 1.0f),
		std::clamp(texture2[2], MIN_ROUGHNESS, 1.0f),
		texture2[3],
		texture2[3] < 0.5f ? 0.0f : 1.0f,
	};
}

inline bool AlphaShadowDiscard(const Material& material, bool hasAlphaShadows)
{
	return hasAlphaShadows && material.alpha < 0.5f;
}

inline Rgb ReflectanceF0(const Material& material)
{
	Rgb result;
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = DEFAULT_F0 * (1.0f - material.metalness) + material.albedo[i] * material.metalness;
	return result;
}

inline float ReflectanceF90(const Rgb& f0)
{
	return std::clamp(*std::max_element(f0.begin(), f0.end()) * 50.0f, 0.0f, 1.0f);
}

inline Rgb FresnelSchlick(const Rgb& f0, const Rgb& f90, float viewDotHalf)
{
	const float weight = std::pow(std::clamp(1.0f - viewDotHalf, 0.0f, 1.0f), 5.0f);
	Rgb result;
	for (std::size_t i = 0; i < result.size(); ++i)
		result[i] = f0[i] + (f90[i] - f0[i]) * weight;
	return result;
}

inline float VisibilityOcclusionFast(float normalDotLight, float normalDotView, float roughnessSquared)
{
	const float ggxV = normalDotLight * (normalDotView * (1.0f - roughnessSquared) + roughnessSquared);
	const float ggxL = normalDotView * (normalDotLight * (1.0f - roughnessSquared) + roughnessSquared);
	const float ggx = ggxV + ggxL;
	// Branch preserves the intended zero result without evaluating 0.5 / 0.
	return ggx > 0.0f ? 0.5f / ggx : 0.0f;
}

inline float MicrofacetDistribution(float normalDotHalf, float roughnessFourth)
{
	const float f = (normalDotHalf * roughnessFourth - normalDotHalf) * normalDotHalf + 1.0f;
	// BAR intentionally cancels PI against the subsequent lighting equations.
	return roughnessFourth / (f * f);
}

inline Rgb CustomTonemap(Rgb radiance, float modelGamma = 1.0f, const std::array<float, 5>& parameters = DEFAULT_TONEMAP)
{
	for (float& channel: radiance) {
		const float numerator = channel * (parameters[0] * channel + parameters[1]);
		const float denominator = channel * (parameters[2] * channel + parameters[3]) + parameters[4];
		channel = numerator / denominator;
	}
	return FromLinear(radiance, modelGamma);
}

// Explicit upload format derived from ModelsVBO attributes; convert from engine
// vertices rather than assuming this type aliases SVertexData on every platform.
struct ModelVertex {
	Rgb position;
	Rgb normal;
	Rgb tangent;
	Rgb bitangent;
	Rgba uv;
	std::array<std::uint8_t, 4> boneIdsLow;
	std::array<std::uint8_t, 4> boneWeights;
	std::array<std::uint8_t, 4> boneIdsHigh;
};

static_assert(std::is_standard_layout_v<ModelVertex>);
static_assert(sizeof(float) == 4 && sizeof(ModelVertex) == 76);
static_assert(offsetof(ModelVertex, normal) == 12);
static_assert(offsetof(ModelVertex, tangent) == 24);
static_assert(offsetof(ModelVertex, bitangent) == 36);
static_assert(offsetof(ModelVertex, uv) == 48);
static_assert(offsetof(ModelVertex, boneIdsLow) == 64);
static_assert(offsetof(ModelVertex, boneWeights) == 68);
static_assert(offsetof(ModelVertex, boneIdsHigh) == 72);

// Native bit packing. Do not reinterpret host endian-dependent uint16 arrays.
constexpr std::uint32_t PackInstanceInfo(std::uint16_t paletteIndex, std::uint16_t numPieces)
{
	return std::uint32_t(paletteIndex) | (std::uint32_t(numPieces) << 16u);
}

constexpr std::uint16_t PaletteIndex(std::uint32_t instanceInfo)
{
	return instanceInfo & 0x7FFu; // Recoil's 11-bit palette field
}

constexpr std::uint16_t PieceCount(std::uint32_t instanceInfo)
{
	return instanceInfo >> 16u;
}

constexpr std::uint8_t BarTeamIndex(std::uint32_t instanceInfo)
{
	return instanceInfo & 0xFFu; // Pinned BAR shader's narrower team-color lookup
}

constexpr std::uint8_t UnpackByte(std::uint32_t word, std::uint32_t index)
{
	return (word >> (8u * index)) & 0xFFu; // index must be 0..3
}

constexpr std::uint16_t BoneId(std::uint32_t idsLow, std::uint32_t idsHigh, std::uint32_t index)
{
	return UnpackByte(idsLow, index) | (std::uint16_t(UnpackByte(idsHigh, index)) << 8u);
}

constexpr float BoneWeight(std::uint32_t weights, std::uint32_t index)
{
	return float(UnpackByte(weights, index)) / 255.0f;
}

} // namespace spring::metal4::bar
