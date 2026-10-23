/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// Exact original test31657 programs; paired native consumer only.
#include "NativeLightningEffectData.h"
#include <string_view>
namespace rendering {
inline constexpr std::array<std::string_view,21> NativeLightningParameterNames={"ANIMATE_RATE","BRIGHTNESS_MULT","BRUSH_MAX_JITTER_FRAC","CORE_BRIGHTNESS","CORE_EDGE_END","CORE_EDGE_START","ENV_LIGHTNING_ANIMATE","FADE_IN_END","FADE_OUT_START","FLICKER_AMPLITUDE","GLOW_FALLOFF_POWER","GLOW_MIN_PIXEL_WIDTH","GLOW_WIDTH_MULT","IMPACT_BRIGHTNESS","JITTER_MAX_BOLT_FRAC","LENGTH_FALLOFF","MIN_PIXEL_WIDTH","ROOT_TAPER_END","SEGMENT_LENGTH_VAR","THICKNESS_VARIATION","TIP_TAPER_START"};
struct NativeLightningProgramSpec {std::string_view name;std::uint64_t vertexHash,fragmentHash;std::string_view texture;};
inline constexpr std::array<NativeLightningProgramSpec,5> NativeLightningPrograms={{
{"LightningCannonBoltGL4",0x902fded01e9562fdULL,0x43f4226f6c73a540ULL,""},
{"LightningCannonGlowGL4",0xbb4d46c4957dd5cbULL,0x92bf05925adc2106ULL,""},
{"LightningCannonImpactGL4",0xef434d7cf0401b9dULL,0x24becff48c26eef1ULL,"bitmaps/projectiletextures/flare2.tga"},
{"EnvLightningBoltGL4",0x711f20c1863512b2ULL,0x685113760ed865beULL,""},
{"EnvLightningGlowGL4",0x877db90f3773e9adULL,0x5347bed4ce97589eULL,""},
}};
inline constexpr std::array<std::string_view,5> NativeLightningSamplerNames={"","","impactTex","",""};
inline constexpr std::array<std::array<bool,21>,5> NativeLightningRequiredParameters={{
{{false,true,true,true,true,true,false,true,true,true,false,false,false,false,true,false,true,false,true,true,false}},
{{false,false,false,false,false,false,false,true,true,true,true,true,true,false,false,false,false,false,false,false,false}},
{{false,true,false,false,false,false,false,true,true,true,false,false,false,true,false,false,false,false,false,false,false}},
{{true,true,true,true,true,true,true,false,false,true,false,false,false,false,true,true,true,true,true,true,true}},
{{true,false,false,false,false,false,true,false,false,true,true,true,true,false,true,true,false,false,false,false,false}},
}};
}
