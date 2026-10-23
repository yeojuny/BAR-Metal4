/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Adapted from BAR cus_gl4.frag.glsl, Copyright (c) 2025 Beherith
// (mysterme@gmail.com), MIT license. This is a scoped equation include, not PBR.
#ifndef SPRING_METAL4_BAR_MATERIAL_SEMANTICS
#define SPRING_METAL4_BAR_MATERIAL_SEMANTICS
#include <metal_stdlib>
using namespace metal;

struct BarMaterial {
	float3 albedo;
	float emissiveness;
	float metalness;
	float roughness;
	float alpha;
	float alphaBin;
};

inline float3 BarToLinear(float3 color, float modelGamma)
{
	return modelGamma == 1.0f ? color : pow(color, float3(modelGamma));
}

inline float3 BarFromLinear(float3 color, float modelGamma)
{
	return modelGamma == 1.0f ? color : pow(color, float3(1.0f / modelGamma));
}

inline BarMaterial BarDecodeMaterial(float4 texture1, float4 texture2, float3 teamColor, float selfIllumination = 1.0f, float modelGamma = 1.0f)
{
	return {
		BarToLinear(mix(texture1.rgb, teamColor, texture1.a), modelGamma),
		clamp(texture2.r * selfIllumination, 0.0f, 1.0f),
		clamp(texture2.g, 0.0f, 1.0f),
		clamp(texture2.b, 0.04f, 1.0f),
		texture2.a,
		texture2.a < 0.5f ? 0.0f : 1.0f,
	};
}

inline bool BarAlphaShadowDiscard(BarMaterial material, bool hasAlphaShadows)
{
	return hasAlphaShadows && material.alpha < 0.5f;
}

inline float3 BarReflectanceF0(BarMaterial material)
{
	return mix(float3(0.04f), material.albedo, material.metalness);
}

inline float BarReflectanceF90(float3 f0)
{
	return clamp(max(f0.r, max(f0.g, f0.b)) * 50.0f, 0.0f, 1.0f);
}

inline float3 BarFresnelSchlick(float3 f0, float3 f90, float viewDotHalf)
{
	return f0 + (f90 - f0) * pow(clamp(1.0f - viewDotHalf, 0.0f, 1.0f), 5.0f);
}

inline float BarVisibilityOcclusionFast(float normalDotLight, float normalDotView, float roughnessSquared)
{
	const float ggxV = normalDotLight * (normalDotView * (1.0f - roughnessSquared) + roughnessSquared);
	const float ggxL = normalDotView * (normalDotLight * (1.0f - roughnessSquared) + roughnessSquared);
	const float ggx = ggxV + ggxL;
	return ggx > 0.0f ? 0.5f / ggx : 0.0f;
}

inline float BarMicrofacetDistribution(float normalDotHalf, float roughnessFourth)
{
	const float f = (normalDotHalf * roughnessFourth - normalDotHalf) * normalDotHalf + 1.0f;
	return roughnessFourth / (f * f);
}

inline float3 BarCustomTonemap(float3 radiance, float modelGamma = 1.0f, float a = 4.75f, float b = 0.75f, float c = 3.5f, float d = 0.85f, float e = 1.0f)
{
	return BarFromLinear((radiance * (a * radiance + b)) / (radiance * (c * radiance + d) + e), modelGamma);
}

inline uint BarUnpackByte(uint word, uint index)
{
	return (word >> (8u * index)) & 0xFFu;
}

inline uint BarBoneId(uint idsLow, uint idsHigh, uint index)
{
	return BarUnpackByte(idsLow, index) | (BarUnpackByte(idsHigh, index) << 8u);
}

inline float BarBoneWeight(uint weights, uint index)
{
	return float(BarUnpackByte(weights, index)) / 255.0f;
}

inline uint BarTeamIndex(uint instanceInfo) { return instanceInfo & 0xFFu; }
inline uint BarPaletteIndex(uint instanceInfo) { return instanceInfo & 0x7FFu; }
inline uint BarPieceCount(uint instanceInfo) { return instanceInfo >> 16u; }
#endif
