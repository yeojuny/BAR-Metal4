/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr const char* NativeLegacyWaterBumpShaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BumpLayer { float4 uv; float4 fogColor; float4 fog; };
struct BumpVertex { float4 position [[position]]; float2 uv; };
vertex BumpVertex LegacyBumpVertex(uint id [[vertex_id]], constant BumpLayer& layer [[buffer(0)]]) {
	// Original Zero, Up, Right, XY quad coverage, triangulated as a strip.
	const float2 p[4] = {float2(0,0),float2(0,1),float2(1,0),float2(1,1)};
	BumpVertex out;
	out.position = float4(p[id] * 2.0f - 1.0f, 0.0f, 1.0f);
	out.uv = float2(p[id].x * layer.uv.x, p[id].y * layer.uv.y + layer.uv.z);
	return out;
}
fragment float4 LegacyBumpFragment(BumpVertex in [[stage_in]], texture2d<float> raw [[texture(0)]], sampler linearRepeat [[sampler(0)]], constant BumpLayer& layer [[buffer(0)]]) {
	// glColor3f(0.2,0.2,0.2) retains alpha=1, fixed-function MODULATE.
	float4 color = raw.sample(linearRepeat, in.uv) * float4(0.2f,0.2f,0.2f,1.0f);
	// Every source vertex has identity-modelview eye z=0.
	if (layer.fog.z != 0.0f) color.rgb = mix(layer.fogColor.rgb, color.rgb, clamp(layer.fog.x * layer.fog.y, 0.0f, 1.0f));
	return color;
}
)MSL";
}
