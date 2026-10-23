/* Complete exact30770 Fire GL4 VS/FS expressions; original instancing, no GS. */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string_view>
namespace rendering::metal4::firegl4 {
inline constexpr std::string_view SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"metal(
#include <metal_stdlib>
using namespace metal;
struct FireInstance { float4 worldPos,velLife,sizeData,tintAlpha; };
struct FireUniform { float4x4 cameraViewProjection,cameraViewInverse;float4 timeInfo,windVelocity;float defines[13];uint4 options; };
struct FireOutput { float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 particleColor [[RECOIL_SAMPLE_PERSPECTIVE]];float animFrame [[RECOIL_SAMPLE_PERSPECTIVE]],rowVariant [[RECOIL_SAMPLE_PERSPECTIVE]];float ptype [[flat]]; };
vertex FireOutput FireGL4VertexMain(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],device const float4*vertices [[buffer(0)]],device const FireInstance*instances [[buffer(1)]],constant FireUniform&u [[buffer(2)]]){
 const float4 position_xy_uv=vertices[vertexID];const auto source=instances[instanceID];
 const float4 worldPos=source.worldPos,velLife=source.velLife,sizeData=source.sizeData,tintAlpha=source.tintAlpha,timeInfo=u.timeInfo;
 const float3 windVelocity=u.windVelocity.xyz;const float4x4 cameraViewInv=u.cameraViewInverse,cameraViewProj=u.cameraViewProjection;FireOutput output;

	float currentFrame = timeInfo.x + timeInfo.w;
	float age = currentFrame - worldPos.w;
	float life = velLife.w;
	float t = clamp(age / life, 0.0, 1.0);

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

	// Drag-integrated forward motion
	float drag = exp(age * -0.10);
	float dragFactor = (1.0 - drag) * 10.0; // 1 / 0.10
	float3 pos = worldPos.xyz + velLife.xyz * dragFactor;

	// Per-type vertical buoyancy
	if (type < 0.5) {
		pos.y += u.defines[0] * t * t;
	} else if (type < 1.5) {
		pos.y += u.defines[1] * age * age;
	} else {
		pos.y += u.defines[2] * age * age;
	}

	// Wind drift
	float windMult = (type < 0.5) ? u.defines[3]
				   : ((type < 1.5) ? u.defines[4] : u.defines[5]);
	pos.x += windVelocity.x * age * windMult;
	pos.z += windVelocity.z * age * windMult;

	// Turbulence
	float wobAmp;
	if (type < 0.5) {
		wobAmp = mix(u.defines[6], u.defines[7], t);
	} else if (type < 1.5) {
		wobAmp = u.defines[8] * t;
	} else {
		wobAmp = u.defines[9];
	}
	float ph = seed * 6.283185;
	float aw = (type > 1.5) ? 0.30 : 0.11; // embers wobble faster (flaky)
	pos.x += sin(age * aw + ph * 2.7) * wobAmp;
	pos.z += cos(age * (aw * 0.85) + ph * 3.8) * wobAmp;
	pos.y += sin(age * (aw * 0.65) + ph * 1.7) * wobAmp * 0.35;

	// Size growth varies by type
	float growth;
	if (type < 0.5) growth = u.defines[10];
	else if (type < 1.5) growth = u.defines[11];
	else growth = u.defines[12];
	float curve = t * (1.0 + t * 0.6);
	float size = baseSize * (1.0 + curve * growth);
	if (type > 1.5) size = baseSize * (1.0 - t * 0.45); // embers shrink as they cool

	// Billboard (camera-aligned, with rotation)
	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;
	float rotSpeed = (seed - 0.5) * 0.8;
	float rot = sizeData.w + age * rotSpeed * 0.02;
	float cr = cos(rot);
	float sr = sin(rot);
	float3 rR =  camRight * cr + camUp * sr;
	float3 rU = -camRight * sr + camUp * cr;
	float3 off = (rR * position_xy_uv.x + rU * position_xy_uv.y) * size;

	output.position = cameraViewProj * float4(pos + off, 1.0);
	output.texCoords = position_xy_uv.zw;
	output.ptype = type;

	float3 col = tintAlpha.rgb;

	// Alpha curve: ease-in, long mid, soft fade-out.
	float aIn  = clamp(t * 8.0, 0.0, 1.0);
	float aOut = 1.0 - smoothstep(0.6, 1.0, t);
	float a = tintAlpha.a * aIn * aOut;

	// Ember flaky flicker
	if (type > 1.5) {
		float fl = 0.55 + 0.45 * sin(age * 0.9 + ph * 5.0);
		a *= clamp(fl, 0.0, 1.0);
	}

	output.particleColor = float4(col, a);

	// Animation atlas frame selection
	if (type < 0.5) {
		// fire atlas (16 cols x 6 rows)
		output.animFrame = floor(t * 15.0 + 0.5);
		output.rowVariant = floor(seed * 6.0);
	} else {
		// smoke atlas (8 cols x 8 rows)
		float rawFrame = age * 0.13 + seed * 8.0;
		output.animFrame = (floor(rawFrame) - 8.0 * floor(floor(rawFrame) / 8.0));
		output.rowVariant = floor(seed * 8.0);
	}

 if(u.options.x!=0)output.position.z=(output.position.z+output.position.w)*0.5;
 return output;
}
fragment float4 FireGL4FragmentMain(FireOutput input [[stage_in]],texture2d<float>fireTex [[texture(0)]],texture2d<float>smokeTex [[texture(1)]],sampler fireSampler [[sampler(0)]],sampler smokeSampler [[sampler(1)]],constant FireUniform&u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));float4 fragColor;

	if (input.particleColor.a <= 0.001) discard_fragment();

	float3 color;
	float a;

	if (input.ptype > 1.5) {
		// Ember: procedural glowing radial disc, no texture sample.
		float2 d = input.texCoords - float2(0.5);
		float r2 = dot(d, d) * 4.0;
		float core = exp(-r2 * 4.0);
		float halo = exp(-r2 * 1.6) * 0.4;
		float shape = core + halo;
		color = mix(input.particleColor.rgb, float3(1.0, 0.95, 0.7), core * 0.5);
		a = shape * input.particleColor.a;
	} else if (input.ptype < 0.5) {
		// fire
		float2 uv = float2(
			(input.animFrame + input.texCoords.x) * 0.0625,
			(clamp(input.rowVariant, 0.0, 5.0) + input.texCoords.y) * 0.16667
		);
		float4 texSample = fireTex.sample(fireSampler, uv);
		color = input.particleColor.rgb * texSample.rgb;
		float coreBright = texSample.r * texSample.r;
		color += coreBright * input.particleColor.rgb * 0.6;
		a = texSample.a * input.particleColor.a;
	} else {
		// smoke
		float2 uv = float2(
			(input.animFrame + input.texCoords.x) * 0.125,
			(clamp(input.rowVariant, 0.0, 7.0) + input.texCoords.y) * 0.125
		);
		float4 texSample = smokeTex.sample(smokeSampler, uv);
		color = input.particleColor.rgb * texSample.rgb;
		a = texSample.a * input.particleColor.a;
	}

	// Premultiplied additive-friendly output
	fragColor = float4(color * a, a);
	if (fragColor.a < 0.004) discard_fragment();

 if(u.options.y!=0){const float ref=as_type<float>(u.options.w);const uint compare=u.options.z;
 const bool passed=compare==0?false:compare==1?fragColor.a<ref:compare==2?fragColor.a==ref:compare==3?fragColor.a<=ref:compare==4?fragColor.a>ref:compare==5?fragColor.a!=ref:compare==6?fragColor.a>=ref:true;if(!passed)discard_fragment();}
 return fragColor;
}
)metal";
}
