/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering {
struct NativeEnergyUniformSpec {std::string_view name;unsigned offset,count;};
inline constexpr std::array<NativeEnergyUniformSpec,25> NativeEnergyUniforms{{
	{"coreBoost",0,1},
	{"cubeNoise",1,1},
	{"cubeNoiseScale",2,1},
	{"cubeNoiseSpeed",3,1},
	{"cubeShowInside",4,1},
	{"drag",5,1},
	{"drawRadius",6,1},
	{"fadeInFrames",7,1},
	{"glowBreath",8,1},
	{"glowBreathFreq",9,1},
	{"glowBreathFreqVar",10,1},
	{"glowBreathVar",11,1},
	{"glowFalloff",12,1},
	{"glowIntensity",13,1},
	{"glowScale",14,1},
	{"gravity",15,3},
	{"hueJitter",18,1},
	{"u_shape",19,1},
	{"whiteHotspot",20,1},
	{"whiteHotspotThreshold",21,1},
	{"wobbleAmp",22,1},
	{"wobbleFreq",23,1},
	{"wobbleFreqVar",24,1},
	{"wobbleRampFrames",25,1},
	{"wobbleVar",26,1},
}};
inline constexpr unsigned NativeEnergyUniformFloatCount=27;
inline constexpr std::uint64_t NativeEnergyVertexHash=0x273fc6fb3a2f19abULL;
inline constexpr std::uint64_t NativeEnergyFragmentHash=0x382fe7e214f6a517ULL;
}
