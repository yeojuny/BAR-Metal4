/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string_view>
namespace rendering::metal4::napalmPreview {
inline constexpr std::string_view SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"metal(
#include <metal_stdlib>
using namespace metal;
struct NapalmUniform {
 float4x4 modelViewProjection;
 float4 color;
 float4 params; // original time, center.x, center.y, inherited alpha reference
 uint4 options; // actual clip convention, alpha enabled, alpha compare
};
struct NapalmVertexOutput {
 float4 position [[position]];
 float4 worldPos [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 vColor [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex NapalmVertexOutput NapalmVertexMain(uint vertexID [[vertex_id]],
 device const float4* vertices [[buffer(0)]],constant NapalmUniform& u [[buffer(1)]]) {
 NapalmVertexOutput output;
 output.worldPos=vertices[vertexID];
 output.vColor=u.color;
 output.position=u.modelViewProjection*vertices[vertexID];
 if(u.options.x!=0)output.position.z=(output.position.z+output.position.w)*0.5;
 return output;
}
// =======================================================================
// [CONFIGURATION]
// =======================================================================

// 1. SIZE: Lower = Bigger clouds
constant float NOISE_SCALE = 0.005;

// 2. SPEED: [UPDATED] Very slow drift now (Fire from above doesn't "run" away)
constant float2 SCROLL_SPEED = float2(0.1, -0.1);

// 3. CONTRAST: 1.0 = Blurry/Flat. 0.0 = Sharp edges.
constant float SHARPNESS = 0.2;

// 4. VISIBILITY:
constant float OPACITY_MIN = 0.35;
constant float OPACITY_MAX = 0.5;

// 5. DETAIL: 0.0 to 1.0
constant float DETAIL_STRENGTH = 0.4;

// 6. ACCENT COLOR (The 2nd color - "hot" parts).
constant float3 ACCENT_COLOR = float3(1.0, 0.91, 0.29);

// 7. COLOR MIX STRENGTH
constant float COLOR_MIX_AMOUNT = 0.1;

// 8. WIND WOBBLE (Randomness)
constant float WOBBLE_STRENGTH = 0.1;
constant float WOBBLE_FREQ = 0.5;

// How fast does it grow/shrink?
constant float PULSE_SPEED = 1;

// How much does the size change? (0.0 = None, 0.5 = Massive pulsing)
constant float PULSE_AMPLITUDE = 0.001;

// =======================================================================

float NapalmRandom(float2 p) {
    return fract(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float NapalmNoise(float2 p) {
    float2 i = floor(p);
    float2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = NapalmRandom(i);
    float b = NapalmRandom(i + float2(1.0, 0.0));
    float c = NapalmRandom(i + float2(0.0, 1.0));
    float d = NapalmRandom(i + float2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}


fragment float4 NapalmFragmentMain(NapalmVertexOutput input [[stage_in]],constant NapalmUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 const float4 worldPos=input.worldPos,vColor=input.vColor;
 const float time=u.params.x;
 const float2 center=u.params.yz;
 float4 result;

    // 1. Setup Coordinates & Pulse
    float2 uv = (worldPos.xz - center) * NOISE_SCALE;

    // Pulse effect (zoom in/out slightly)
    float pulse = 1.0 + sin(time * PULSE_SPEED) * PULSE_AMPLITUDE;
    uv *= pulse;

    // 2. Movement Logic
    float2 baseFlow = time * SCROLL_SPEED;
    float2 wander   = float2(sin(time * WOBBLE_FREQ), cos(time * WOBBLE_FREQ * 2.0));
    float2 flow     = baseFlow + (wander * WOBBLE_STRENGTH);

    // 3. Noise Generation
    float n1 = NapalmNoise(uv - flow);
    float n2 = NapalmNoise(uv * 2.0 + flow * 1.5);
    float n3 = NapalmNoise(uv * 4.0 + flow * 3.0);

    float weightedNoise = (n1 * 0.5) + (n2 * 0.3) + (n3 * DETAIL_STRENGTH);
    weightedNoise /= (0.8 + DETAIL_STRENGTH);

    // 4. Sharpness & Alpha
    float density = smoothstep(0.5 - (0.5 * SHARPNESS), 0.5 + (0.5 * SHARPNESS), weightedNoise);
    float finalAlpha = mix(OPACITY_MIN, OPACITY_MAX, density);

    // 5. Final Color
    float3 mixedColor = mix(vColor.rgb, ACCENT_COLOR, density * COLOR_MIX_AMOUNT);

    result = float4(mixedColor, vColor.a * finalAlpha);

 if(u.options.y!=0){
  const float ref=u.params.w;const uint compare=u.options.z;
  const bool passed=compare==0?false:compare==1?result.a<ref:compare==2?result.a==ref:
   compare==3?result.a<=ref:compare==4?result.a>ref:compare==5?result.a!=ref:compare==6?result.a>=ref:true;
  if(!passed)discard_fragment();
 }
 return result;
}
)metal";
}
