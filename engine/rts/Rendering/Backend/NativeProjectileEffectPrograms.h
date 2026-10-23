/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// Exact original test31657 programs; paired native consumer only.
#include "NativeProjectileEffectData.h"
#include <string_view>
namespace rendering {
inline constexpr std::array<std::string_view,42> NativeProjectileParameterNames={"BRIGHTNESS_MULT","CORE_BRIGHTNESS","CORE_EDGE_END","CORE_EDGE_START","CROSS_SECTION_BRIGHTNESS","EDGE_SOFTNESS","ELONGATION_MAX","ELONGATION_MIN","ELONGATION_SPEED_REF","FADE_IN_END","FADE_OUT_START","GLOW_BRIGHTNESS","GLOW_DIM_FACTOR","GLOW_FALLOFF_POWER","GLOW_REF_SIZE","GLOW_SIZE_MULT","GLOW_WIDTH_DIM","GLOW_WIDTH_FULL","GLOW_WIDTH_MULT","MIN_PIXEL_WIDTH","NOISE_SCALE","NOISE_SPEED","NOISE_STRENGTH","PULSE_BRIGHTNESS","PULSE_CORE_FRAC","PULSE_PARA_BRIGHTNESS","PULSE_PARA_SIGMA","PULSE_PARA_SPACING","PULSE_PARA_SPEED","PULSE_PARA_WIDTH_MULT","PULSE_SIGMA","PULSE_SPACING","PULSE_SPEED","PULSE_WIDTH_MULT","RANGE_TAPER","SHIMMER_AMPLITUDE","SHIMMER_SPEED","SWIRL_SPEED","SWIRL_STRENGTH","TIP_FADE_START","TRAIL_FALLOFF","TRAIL_SHIFT"};
struct NativeProjectileProgramSpec {std::string_view name;std::uint64_t vertexHash,fragmentHash;std::string_view texture;};
inline constexpr std::array<NativeProjectileProgramSpec,7> NativeProjectilePrograms={{
{"BeamLaserGL4",0xee40fb11e08fd71aULL,0x2a7b9c39ebc258cULL,"bitmaps/projectiletextures/largebeam.tga"},
{"BeamLaserFlareGL4",0x58e2e7f9b8525e8cULL,0x288260bd945a8cdaULL,"bitmaps/projectiletextures/flare2.tga"},
{"BeamLaserGlowGL4",0x14a44593baf88350ULL,0x71d92b02ecacbc8cULL,""},
{"BeamLaserPulseGL4",0x1b27105d56a517ffULL,0x180271d94944908ULL,""},
{"PlasmaCannonGL4",0x53b66743c1b76c1dULL,0xf88b285f18d5407ULL,""},
{"PlasmaCannonCrossSectionGL4",0xd009b96b31ab91d0ULL,0xf0a0c4f5ad4abe96ULL,""},
{"PlasmaCannonGlowGL4",0xc1ebf7a182087a53ULL,0xde814c08639df811ULL,"bitmaps/projectiletextures/glow2.tga"},
}};
inline constexpr std::array<std::string_view,7> NativeProjectileSamplerNames={"beamTex","flareTex","","","","","glowTex"};
inline constexpr std::array<std::array<bool,42>,7> NativeProjectileRequiredParameters={{
{{true,true,true,true,false,false,false,false,false,true,true,false,false,false,false,false,false,false,false,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,true,true,false,false,true,false,false}},
{{true,true,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false}},
{{false,false,false,false,false,false,false,false,false,true,true,true,true,true,false,false,true,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,true,false,false,false,false,false}},
{{false,false,false,false,false,false,false,false,false,true,true,false,false,false,false,false,false,false,false,true,false,false,false,true,true,true,true,true,true,true,true,true,true,true,false,false,false,false,false,false,false,false}},
{{true,true,true,true,false,true,true,true,true,false,false,false,false,false,false,false,false,false,false,false,true,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,true,false,true,true}},
{{false,true,true,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true,false,false,false,false}},
{{false,false,false,false,false,false,true,true,true,false,false,true,false,false,true,true,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,true}},
}};
}
