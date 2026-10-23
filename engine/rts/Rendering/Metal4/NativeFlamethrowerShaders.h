/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string_view>
namespace rendering::metal4::flamethrower {
inline constexpr std::string_view SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"metal(
#include <metal_stdlib>
using namespace metal;
struct FlameInstance { float4 worldPos, velLife, sizeData, tintAlpha; };
struct FlameUniform {
    float4x4 cameraViewProjection, cameraViewInverse;
    float4 timeInfo, windVelocity;
    float defines[12];
    uint4 options;
};
struct FlameVertexOutput {
    float4 position [[position]];
    float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];
    float4 particleColor [[RECOIL_SAMPLE_PERSPECTIVE]];
    float animFrame [[RECOIL_SAMPLE_PERSPECTIVE]], rowVariant [[RECOIL_SAMPLE_PERSPECTIVE]];
    float ptype [[flat]];
};
vertex FlameVertexOutput FlamethrowerVertexMain(
    uint vertexID [[vertex_id]], uint instanceID [[instance_id]],
    device const float4* vertices [[buffer(0)]],
    device const FlameInstance* instances [[buffer(1)]],
    constant FlameUniform& u [[buffer(2)]]) {
    const float4 position_xy_uv = vertices[vertexID];
    const FlameInstance source = instances[instanceID];
    const float4 worldPos=source.worldPos, velLife=source.velLife;
    const float4 sizeData=source.sizeData, tintAlpha=source.tintAlpha;
    const float4 timeInfo=u.timeInfo;
    const float3 windVelocity=u.windVelocity.xyz;
    const float4x4 cameraViewInv=u.cameraViewInverse;
    const float4x4 cameraViewProj=u.cameraViewProjection;
    FlameVertexOutput output;

	float currentFrame = timeInfo.x + timeInfo.w;
	float age = currentFrame - worldPos.w;
	float life = velLife.w;
	float t = clamp(age / life, 0.0, 1.0);

	// Cull expired / not yet born
	if (t >= 1.0 || age < 0.0) {
		output.position = float4(-9999.0);
		output.particleColor = float4(0.0);
		output.texCoords = float2(0.0);
		output.animFrame = 0.0;
		output.rowVariant = 0.0;
		output.ptype = 0.0;
		return output;
	}

	float type = sizeData.y;
	float seed = sizeData.z;
	float baseSize = sizeData.x;

	// Position: explicit drag-integrated forward motion
	float drag = exp(age * -0.12);
	float dragFactor = (1.0 - drag) * 8.333; // 1 / 0.12
	float3 pos = worldPos.xyz + velLife.xyz * dragFactor;

	// Per-type vertical buoyancy (none for jet).
	// Flame uses t*t (normalized against per-particle lifetime) so the
	// end-of-life rise is uniform across weapons -- otherwise long-range
	// weapons like cormaw (lifeMult up to 4x) would rise up to 16x more
	// (rise scales with life^2 when integrating constant acceleration).
	// Smoke keeps age*age because smoke lifetime isn't weapon-scaled.
	if (type < 0.5) {
		pos.y += u.defines[0] * t * t;
	} else if (type < 1.5) {
		pos.y += u.defines[1] * age * age;
	}
	// type == 3 (jet): no buoyancy/gravity, follows velocity only -- clean trail

	// Wind drift (smoke much more affected than flame; jet ignores wind)
	if (type < 2.5) {
		float windMult = (type < 1.5) ? u.defines[2] : u.defines[3];
		pos.x += windVelocity.x * age * windMult;
		pos.z += windVelocity.z * age * windMult;
	}

	// Turbulence: grows with age, scaled by type. Jet uses small fixed amplitude.
	float wobAmp;
	if (type > 2.5) {
		wobAmp = u.defines[7];
	} else {
		float wobMax = (type < 0.5) ? u.defines[5]
					 : ((type < 1.5) ? u.defines[6] : u.defines[5] * 0.6);
		wobAmp = mix(u.defines[4], wobMax, t);
	}
	float ph = seed * 6.283185;
	pos.x += sin(age * 0.11 + ph * 2.7) * wobAmp;
	pos.z += cos(age * 0.09 + ph * 3.8) * wobAmp;
	pos.y += sin(age * 0.07 + ph * 1.7) * wobAmp * 0.35;

	// Size growth varies by type
	float growth;
	if (type < 0.5) growth = u.defines[8];
	else if (type < 1.5) growth = u.defines[9];
	else growth = u.defines[10];
	float curve = t * (1.0 + t * 0.6);
	float size = baseSize * (1.0 + curve * growth);

	// Billboard
	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;

	float3 off;
	if (type > 2.5) {
		// Jet: velocity-aligned stretched billboard. The long axis follows the
		// projectile velocity in world space; the short (width) axis is the
		// in-screen perpendicular. This lets one jet quad cover the screen-space
		// span of ~u.defines[11] round particles, so the stream reads as a continuous
		// streak rather than a dotted line and we save on per-frame jet count.
		float3 vDir = velLife.xyz;
		float vLen = length(vDir);
		float3 camFwd = cameraViewInv[2].xyz;
		float3 crossAxis = (vLen > 0.001) ? cross(vDir / vLen, camFwd) : float3(0.0);
		float cLen = length(crossAxis);
		if (vLen > 0.001 && cLen > 0.001) {
			float3 longAxis = vDir / vLen;
			crossAxis = crossAxis / cLen;
			off = longAxis  * position_xy_uv.y * (size * u.defines[11])
			    + crossAxis * position_xy_uv.x * size;
		} else {
			// Velocity ~zero or pointing straight at the camera: fall back to
			// camera-aligned so the particle is still visible (no degenerate quad).
			off = (camRight * position_xy_uv.x + camUp * position_xy_uv.y) * size;
		}
	} else {
		float rotSpeed = (seed - 0.5) * 0.8;
		float rot = sizeData.w + age * rotSpeed * 0.02;
		float cr = cos(rot);
		float sr = sin(rot);
		float3 rR = camRight * cr + camUp * sr;
		float3 rU = -camRight * sr + camUp * cr;
		off = (rR * position_xy_uv.x + rU * position_xy_uv.y) * size;
	}

	output.position = cameraViewProj * float4(pos + off, 1.0);
	output.texCoords = position_xy_uv.zw;
	output.ptype = type;

	// Per-particle color
	float3 col = tintAlpha.rgb;

	// Alpha curve: ease-in (quick rise), long mid, soft fade-out.
	// Jet uses a faster ease-in and earlier fade-out for a crisp leading streak.
	float aIn, aOut;
	if (type > 2.5) {
		aIn  = clamp(t * 12.0, 0.0, 1.0);
		aOut = 1.0 - smoothstep(0.35, 1.0, t);
	} else {
		aIn  = clamp(t * 8.0, 0.0, 1.0);
		aOut = 1.0 - smoothstep(0.55, 1.0, t);
	}
	float a = tintAlpha.a * aIn * aOut;

	output.particleColor = float4(col, a);

	// Animation atlas frame selection
	if (type < 1.5) {
		// fire atlas (16 cols x 6 rows)
		output.animFrame = floor(t * 15.0 + 0.5);
		output.rowVariant = floor(seed * 6.0);
	} else {
		// smoke atlas (8 cols x 8 rows)
		float rawFrame = age * 0.13 + seed * 8.0;
		output.animFrame = (floor(rawFrame) - 8.0 * floor(floor(rawFrame) / 8.0));
		output.rowVariant = floor(seed * 8.0);
	}

    if (u.options.x != 0) output.position.z=(output.position.z+output.position.w)*0.5;
    return output;
}
fragment float4 FlamethrowerFragmentMain(FlameVertexOutput input [[stage_in]],
    texture2d<float> fireTex [[texture(0)]], texture2d<float> smokeTex [[texture(1)]],
    sampler fireSampler [[sampler(0)]], sampler smokeSampler [[sampler(1)]],
    constant FlameUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
    float4 fragColor;

	if (input.particleColor.a <= 0.001) discard_fragment();

	float4 texSample;
	float3 color;
	float a;

	if (input.ptype > 2.5) {
		// Nozzle jet: smooth procedural radial disc, no texture sample.
		// Quadratic falloff with an inner hotspot for a clean glowing streak.
		float2 d = input.texCoords - float2(0.5);
		float r2 = dot(d, d) * 4.0;        // 0 at center, 1 at edge
		float core = exp(-r2 * 4.0);        // tight bright core
		float halo = exp(-r2 * 1.6) * 0.45; // soft halo
		float shape = core + halo;
		// hot white inner hint blends into the configured jet color outward
		float3 hot = mix(input.particleColor.rgb, float3(0.97, 0.99, 1.00), core * 0.65);
		color = hot;
		a = shape * input.particleColor.a;
	} else if (input.ptype < 1.5) {
		// fire / drip
		float2 uv = float2(
			(input.animFrame + input.texCoords.x) * 0.0625,
			(clamp(input.rowVariant, 0.0, 5.0) + input.texCoords.y) * 0.16667
		);
		texSample = fireTex.sample(fireSampler, uv);

		color = input.particleColor.rgb * texSample.rgb;
		// Inner glow boost: brighter core
		float coreBright = texSample.r * texSample.r;
		color += coreBright * input.particleColor.rgb * 0.6;
		a = texSample.a * input.particleColor.a;
	} else {
		float2 uv = float2(
			(input.animFrame + input.texCoords.x) * 0.125,
			(clamp(input.rowVariant, 0.0, 7.0) + input.texCoords.y) * 0.125
		);
		texSample = smokeTex.sample(smokeSampler, uv);
		color = input.particleColor.rgb * texSample.rgb;
		a = texSample.a * input.particleColor.a;
	}

	// Premultiplied additive-friendly output
	fragColor = float4(color * a, a);
	if (fragColor.a < 0.004) discard_fragment();

    if (u.options.y != 0) {
        const float ref=as_type<float>(u.options.w);
        const uint compare=u.options.z;
        const bool passed=compare==0?false:compare==1?fragColor.a<ref:
            compare==2?fragColor.a==ref:compare==3?fragColor.a<=ref:
            compare==4?fragColor.a>ref:compare==5?fragColor.a!=ref:
            compare==6?fragColor.a>=ref:true;
        if (!passed) discard_fragment();
    }
    return fragColor;
}
)metal";
}
