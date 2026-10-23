/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::fireSmoke {
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
// GLSL mod, including the original floor behavior for negative inputs.
float mod(float x,float y) { return x-y*floor(x/y); }
struct FireSmokeUniform {
 float4x4 cameraViewProjection,cameraViewInverse;
 float4 timeInfo,windVelocity,highlightPass;
 float defines[8];
 uint4 options;
};
struct FireSmokeInstance { float4 worldPos,velocity,sizeAndType,colorTint; };
struct FireSmokeVarying {
 float4 position [[position]];
 float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 particleColor [[RECOIL_SAMPLE_PERSPECTIVE]];
 float animFrame [[RECOIL_SAMPLE_PERSPECTIVE]],rowVariant [[RECOIL_SAMPLE_PERSPECTIVE]];
 float isFireParticle [[flat]];
};
vertex FireSmokeVarying FireSmokeVertexMain(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device FireSmokeInstance* instances [[buffer(1)]],
 constant FireSmokeUniform& u [[buffer(2)]])
{
 FireSmokeVarying out{};
 const float4 position_xy_uv=vertices[vertexId];
 const float4 worldPos=instances[instanceId].worldPos,velocity=instances[instanceId].velocity;
 const float4 sizeAndType=instances[instanceId].sizeAndType,colorTint=instances[instanceId].colorTint;

	float currentFrame = u.timeInfo.x + u.timeInfo.w;
	float ageFrames = currentFrame - worldPos.w;
	float lifetime = velocity.w;
	float normalizedAge = clamp(ageFrames / lifetime, 0.0, 1.0);

	// Highlight pass: skip fire particles, shorten effective lifetime
	if (u.highlightPass.x > 0.5) {
		if (sizeAndType.y > 0.5) {
			out.position = float4(-9999.0, -9999.0, -9999.0, 1.0);
			out.particleColor = float4(0.0);
			out.texCoords = float2(0.0);
			out.animFrame = 0.0;
			out.rowVariant = 0.0;
			out.isFireParticle = 0.0;
			return out;
		}
		normalizedAge = clamp(normalizedAge * u.highlightPass.w, 0.0, 1.0);
	}

	// Kill expired particles
	if (normalizedAge >= 1.0 || ageFrames < 0.0) {
		out.position = float4(-9999.0, -9999.0, -9999.0, 1.0);
		out.particleColor = float4(0.0);
		out.texCoords = float2(0.0);
		out.animFrame = 0.0;
		out.rowVariant = 0.0;
		out.isFireParticle = 0.0;
		return out;
	}

	float seed = sizeAndType.z;

	// Velocity with air drag (0.85 per frame)
	float dragPow = exp(ageFrames * -0.16252);  // ln(0.85)
	float dragFactor = (1.0 - dragPow) * 6.6667;  // 1/(1-0.85)

	float3 pos = worldPos.xyz;
	pos += velocity.xyz * dragFactor;

	// Buoyancy
	pos.y += 0.004 * ageFrames * ageFrames;

	// Highlight pass: Y offset (applied before wind/wobble)
	pos.y += u.highlightPass.z;

	// Wind drift: accumulates over particle age, smoke drifts more than fire
	float windMult = mix(u.defines[5], u.defines[5] * u.defines[6], step(0.5, sizeAndType.y));
	pos.x += u.windVelocity.xyz.x * ageFrames * windMult;
	pos.z += u.windVelocity.xyz.z * ageFrames * windMult;

	// Size
	float baseSize = sizeAndType.x;

	// Turbulence: grows with age, scaled by particle size
	float seedPhase = seed * 6.283;
	float wobble = u.defines[2] + normalizedAge * u.defines[3] * baseSize + ageFrames * u.defines[4];
	pos.x += sin(ageFrames * 0.07 + seedPhase * 2.7) * wobble;
	pos.z += cos(ageFrames * 0.09 + seedPhase * 3.8) * wobble;
	pos.y += sin(ageFrames * 0.05 + seedPhase * 1.8) * wobble * 0.25;

	// Growth: lifetime-based curve + time-based linear rate (smoke only)
	float isFire = step(0.5, sizeAndType.y);
	float growCurve = normalizedAge * (1.0 + normalizedAge);
	float lifetimeGrowth = baseSize * (1.0 + growCurve * u.defines[0]);
	float timeGrowth = ageFrames * u.defines[1];
	float smokeGrowth = lifetimeGrowth + timeGrowth;
	float sizeGrowth = mix(smokeGrowth, baseSize, isFire);

	// Highlight pass: smaller size, Y offset
	if (u.highlightPass.x > 0.5) {
		sizeGrowth *= u.highlightPass.y;
	}

	// Billboard
	float3 camRight = u.cameraViewInverse[0].xyz;
	float3 camUp    = u.cameraViewInverse[1].xyz;

	// Rotation
	float rotSpeed = (seed - 0.5) * 0.6;
	float rot = sizeAndType.w + normalizedAge * rotSpeed;
	float cr = cos(rot);
	float sr = sin(rot);
	float3 rotRight = camRight * cr + camUp * sr;
	float3 rotUp    = -camRight * sr + camUp * cr;

	float3 vertexOffset = (rotRight * position_xy_uv.x + rotUp * position_xy_uv.y) * sizeGrowth;
	float4 worldPosition = float4(pos + vertexOffset, 1.0);

	out.position = u.cameraViewProjection * worldPosition;
	out.texCoords = position_xy_uv.zw;

	// Particle type (reuse isFire from growth section)
	out.isFireParticle = isFire;
	int cmapVariant = int(out.isFireParticle);

	// Colormaps: 0 = smoke (dark grey), 1 = fire (orange→black)
	const float4 cmaps[16] = {
		// smoke
		float4(0.20, 0.15, 0.10, 0.75),
		float4(0.22, 0.18, 0.16, 0.7),
		float4(0.22, 0.19, 0.18, 0.65),
		float4(0.28, 0.27, 0.26, 0.55),
		float4(0.30, 0.29, 0.28, 0.42),
		float4(0.27, 0.26, 0.25, 0.28),
		float4(0.2, 0.2, 0.2, 0.14),
		float4(0.12, 0.12, 0.12, 0.01),
		// fire
		float4(1.0, 0.7, 0.15, 1.0),
		float4(0.88, 0.4, 0.1, 0.9),
		float4(0.5, 0.15, 0.07, 0.75),
		float4(0.3, 0.1, 0.04, 0.55),
		float4(0.1, 0.06, 0.02, 0.35),
		float4(0.07, 0.02, 0.01, 0.18),
		float4(0.02, 0.012, 0.005, 0.06),
		float4(0.0, 0.0, 0.0, 0.01)
	};
	int cmapBase = cmapVariant * 8;
	float t = normalizedAge * 7.0;
	int idx = int(clamp(t, 0.0, 6.0));
	float4 cmapColor = mix(cmaps[cmapBase + idx], cmaps[cmapBase + idx + 1], fract(t));

	// Per-particle brightness variation for smoke
	if (out.isFireParticle < 0.5) {
		float brightnessVar = 0.5 + seed * 0.5;
		float hBright = (u.highlightPass.x > 0.5) ? u.defines[7] : 1.0;
		cmapColor.rgb *= brightnessVar * colorTint.rgb * hBright;
	}

	cmapColor.a *= colorTint.a;
	out.particleColor = cmapColor;

	// Animation
	if (out.isFireParticle > 0.5) {
		out.animFrame = floor(normalizedAge * 15.0 + 0.5);
		out.rowVariant = floor(seed * 6.0);
	} else {
		float smokeAnimSpeed = 0.13;
		float rawFrame = ageFrames * smokeAnimSpeed + seed * 8.0;
		out.animFrame = mod(floor(rawFrame), 8.0);
		out.rowVariant = floor(seed * 8.0);
	}

 if(u.options.x) out.position.z=(out.position.z+out.position.w)*0.5;
 return out;
}
fragment float4 FireSmokeFragmentMain(FireSmokeVarying in [[stage_in]],
 texture2d<float> fireTex [[texture(0)]],texture2d<float> smokeTex [[texture(1)]],sampler filter [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;

	if (in.particleColor.a <= 0.001) discard_fragment();

	float4 texSample;
	if (in.isFireParticle > 0.5) {
		float2 atlasUV = float2(
			(in.animFrame + in.texCoords.x) * 0.0625,
			(clamp(in.rowVariant, 0.0, 5.0) + in.texCoords.y) * 0.16667
		);
		texSample = fireTex.sample(filter,atlasUV);

		float3 color = in.particleColor.rgb * texSample.rgb;
		float coreBright3 = texSample.r * texSample.r * texSample.r;
		float glowScale = coreBright3;
		color += glowScale * float3(0.4, 0.32, 0.16);

		float a = texSample.a * in.particleColor.a;
		float3 premul = color * (a + glowScale * 0.2);
		fragColor = float4(premul, a);
	} else {
		float2 atlasUV = float2(
			(in.animFrame + in.texCoords.x) * 0.125,
			(clamp(in.rowVariant, 0.0, 7.0) + in.texCoords.y) * 0.125
		);
		texSample = smokeTex.sample(filter,atlasUV);

		float3 color = in.particleColor.rgb * texSample.rgb;
		float a = texSample.a * in.particleColor.a;
		fragColor = float4(color * a, a);
	}

	if (fragColor.a < 0.005) discard_fragment();

 return fragColor;
}
)MSL";
}
