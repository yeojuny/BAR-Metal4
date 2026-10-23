/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "BarMaterialSemantics.h"

#include <cstdlib>
#include <iostream>

using namespace spring::metal4::bar;

namespace {

int checks = 0;

void Check(bool condition, const char* label)
{
	++checks;
	if (!condition) {
		std::cerr << "FAIL: " << label << '\n';
		std::exit(EXIT_FAILURE);
	}
}

void Near(float actual, float expected, const char* label)
{
	Check(std::isfinite(actual) && std::abs(actual - expected) < 2e-6f, label);
}

} // namespace

int main()
{
	// Independently calculated values from the pinned GLSL equations.
	const Rgba texture1{0.2f, 0.4f, 0.6f, 0.25f};
	const Rgb team{1.0f, 0.0f, 0.25f};
	const Rgba texture2{0.8f, 0.3f, 0.02f, 0.499f};
	const auto base = DecodeMaterial(texture1, texture2, team);
	Near(base.albedo[0], 0.4f, "team mixing precedes gamma, red");
	Near(base.albedo[1], 0.3f, "team mixing precedes gamma, green");
	Near(base.albedo[2], 0.5125f, "team mixing precedes gamma, blue");
	Near(base.emissiveness, 0.8f, "texture2 red emissiveness");
	Near(base.metalness, 0.3f, "texture2 green metalness");
	Near(base.roughness, 0.04f, "texture2 blue lower roughness bound");
	Near(base.alpha, 0.499f, "forward alpha remains continuous");
	Near(base.alphaBin, 0.0f, "deferred binary alpha below threshold");
	Check(!AlphaShadowDiscard(base, false), "opaque material has no global alpha clip");
	Check(AlphaShadowDiscard(base, true), "alpha-shadow specialization clips");

	const auto gamma = DecodeMaterial(texture1, texture2, team, 1.5f, 2.0f);
	Near(gamma.albedo[0], 0.16f, "power gamma after mixing, red");
	Near(gamma.albedo[1], 0.09f, "power gamma after mixing, green");
	Near(gamma.albedo[2], 0.26265625f, "power gamma after mixing, blue");
	Near(gamma.emissiveness, 1.0f, "self illumination clamped after multiplication");

	const auto bounds = DecodeMaterial(texture1, {-1.0f, 2.0f, 2.0f, 0.5f}, team);
	Near(bounds.emissiveness, 0.0f, "emission lower clamp");
	Near(bounds.metalness, 1.0f, "metalness upper clamp");
	Near(bounds.roughness, 1.0f, "roughness upper clamp");
	Near(bounds.alphaBin, 1.0f, "threshold equality is opaque");
	Check(!AlphaShadowDiscard(bounds, true), "threshold equality not discarded");
	const auto negativeMetal = DecodeMaterial(texture1, {0.0f, -0.1f, 0.5f, 1.0f}, team);
	Near(negativeMetal.metalness, 0.0f, "metalness lower clamp");

	const auto dielectricF0 = ReflectanceF0(negativeMetal);
	for (float channel: dielectricF0)
		Near(channel, 0.04f, "dielectric reflectance");
	const auto metallicF0 = ReflectanceF0(bounds);
	for (std::size_t i = 0; i < metallicF0.size(); ++i)
		Near(metallicF0[i], bounds.albedo[i], "fully metallic reflectance equals albedo");
	Near(ReflectanceF90({0.001f, 0.002f, 0.003f}), 0.15f, "low reflectance f90");
	Near(ReflectanceF90(dielectricF0), 1.0f, "f90 upper clamp");
	const auto fresnel = FresnelSchlick(dielectricF0, {1.0f, 1.0f, 1.0f}, 0.5f);
	for (float channel: fresnel)
		Near(channel, 0.07f, "Schlick midpoint");
	Near(FresnelSchlick(dielectricF0, {1.0f, 1.0f, 1.0f}, -1.0f)[0], 1.0f, "Schlick lower domain bound");
	Near(FresnelSchlick(dielectricF0, {1.0f, 1.0f, 1.0f}, 2.0f)[0], 0.04f, "Schlick upper domain bound");
	Near(VisibilityOcclusionFast(0.6f, 0.8f, 0.25f), 0.5f / 1.07f, "BAR fast Smith GGX visibility");
	Near(VisibilityOcclusionFast(0.0f, 0.0f, 0.25f), 0.0f, "zero visibility denominator is finite");
	Near(MicrofacetDistribution(0.5f, 0.0625f), 256.0f / 2401.0f, "BAR distribution cancels PI");

	const auto tone = CustomTonemap({0.0f, 0.5f, 1.0f});
	Near(tone[0], 0.0f, "tonemap black");
	Near(tone[1], 1.5625f / 2.3f, "custom tonemap half radiance");
	Near(tone[2], 110.0f / 107.0f, "custom tonemap does not insert clamp");
	Near(CustomTonemap({1.0f, 0.0f, 0.0f}, 2.0f)[0], std::sqrt(110.0f / 107.0f), "tonemap gamma follows rational function");

	constexpr auto info = PackInstanceInfo(0x3FFu, 0xABCDu);
	Check(info == 0xABCD03FFu, "instance words follow Recoil order");
	Check(PaletteIndex(info) == 0x3FFu, "Recoil palette index is wider than BAR team index");
	Check(BarTeamIndex(info) == 0xFFu, "BAR team index uses low byte");
	Check(PieceCount(info) == 0xABCDu, "piece count uses full high halfword");
	Check(BoneId(0x04030201u, 0x000100FFu, 0) == 0xFF01u, "bone low/high bytes lane zero");
	Check(BoneId(0x04030201u, 0x000100FFu, 2) == 0x0103u, "bone low/high bytes lane two");
	Check(BoneId(0xFFFFFFFFu, 0xFFFFFFFFu, 3) == 0xFFFFu, "invalid bone sentinel");
	Near(BoneWeight(0x008000FFu, 0), 1.0f, "weight 255");
	Near(BoneWeight(0x008000FFu, 2), 128.0f / 255.0f, "quantized bone weight");

	std::cout << "PASS: " << checks << " BAR material / vertex contract checks\n";
	return EXIT_SUCCESS;
}
