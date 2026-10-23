/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

namespace rendering {
// Exact BAR current31657 PiP translations; no arbitrary GLSL support is claimed.
inline constexpr char NativePiPMetalPrograms[] = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"PIPMSL(
#include <metal_stdlib>
using namespace metal;
struct PipImmediateVertex { float4 position; float4 color; float4 texCoord; };
struct PipUniformBlock { float4x4 modelViewProjection; float4 values[32]; uint4 flipV; };
inline float PipGLMod(float x, float y) { return x - y * floor(x / y); }
inline float2 PipUV(float2 uv, uint flipV) { return float2(uv.x, flipV ? 1.0f - uv.y : uv.y); }
inline float PipGLRadians(float degrees) { return degrees * 0.017453292519943295f; }
struct PipLOSStage { float4 position [[position]];
	float2 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};
			
			vertex PipLOSStage pipLOSVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipLOSStage output{};
	const PipImmediateVertex input = immediate[vertexID];
				output.texCoord = input.texCoord.xy;
				output.position = input.position;
			
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}
			
			
			
			
			
			fragment float4 pipLOSFragment(PipLOSStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> losTex [[texture(0)]], sampler losTexSampler [[sampler(0)]], texture2d<float> radarTex [[texture(1)]], sampler radarTexSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
	const float losScale = uniforms.values[0].x;
	const float showRadar = uniforms.values[1].x;
				
				float losValue = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : losTex.sample(losTexSampler, PipUV(input.texCoord, uniforms.flipV[0]))).r;
				
				float radarValue = ((uniforms.flipV.w & (1u << 1)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : radarTex.sample(radarTexSampler, PipUV(input.texCoord, uniforms.flipV[1]))).r;

				
				
				
				float darken = (1.0f - losValue) * losScale * 0.28f;

				
				if (showRadar > 0.5f && losValue < 0.2f && radarValue < 0.2f) {
					darken = darken + losScale * 0.1f;
				}

				output = float4(darken, darken, darken, 1.0f);
			
	return output;
}
struct PipDecalBlitStage { float4 position [[position]];
	float2 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};
			
			vertex PipDecalBlitStage pipDecalBlitVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipDecalBlitStage output{};
	const PipImmediateVertex input = immediate[vertexID];
				output.texCoord = input.texCoord.xy;
				output.position = uniforms.modelViewProjection * input.position;
			
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}
			
			
			
			fragment float4 pipDecalBlitFragment(PipDecalBlitStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> decalTex [[texture(0)]], sampler decalTexSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
	const float strength = uniforms.values[0].x;
				float4 tex = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : decalTex.sample(decalTexSampler, PipUV(input.texCoord, uniforms.flipV[0])));
				output = float4(mix(float3(1.0f), tex.rgb, strength), 1.0f);
			
	return output;
}
struct PipMinimapShadingStage { float4 position [[position]];
	float2 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};
			
			vertex PipMinimapShadingStage pipMinimapShadingVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipMinimapShadingStage output{};
	const PipImmediateVertex input = immediate[vertexID];
				output.texCoord = input.texCoord.xy;
				output.position = uniforms.modelViewProjection * input.position;
			
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}
			
			
			
			fragment float4 pipMinimapShadingFragment(PipMinimapShadingStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> minimapTex [[texture(0)]], sampler minimapTexSampler [[sampler(0)]], texture2d<float> shadingTex [[texture(1)]], sampler shadingTexSampler [[sampler(1)]], texture2d<float> infoTex [[texture(2)]], sampler infoTexSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	const float2 infoUvMult = uniforms.values[0].xy;
	const float infoTexMul = uniforms.values[1].x;
	const float4 minimapColor = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : minimapTex.sample(minimapTexSampler, PipUV(input.texCoord, uniforms.flipV[0]), bias(-2.0f)));
	const float4 shadingColor = ((uniforms.flipV.w & (1u << 1)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : shadingTex.sample(shadingTexSampler, PipUV(input.texCoord, uniforms.flipV[1])));
	const float3 infoColor = ((uniforms.flipV.w & (1u << 2)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : infoTex.sample(infoTexSampler, PipUV(input.texCoord * infoUvMult, uniforms.flipV[2]), bias(-2.0f))).rgb - float3(0.5f);
	return float4(minimapColor.rgb * shadingColor.rgb + infoColor * infoTexMul, 1.0f);
}
struct PipDecalStage { float4 position [[position]];
	float2 f_texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
	float f_alpha [[RECOIL_SAMPLE_PERSPECTIVE]];
};


        
     
      
   







vertex PipDecalStage pipDecalVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipDecalStage output{};
	const float gameFrame = uniforms.values[0].x;
	const float2 invMapSize = uniforms.values[1].xy;
	const float2 quadPos = float2(vertices[vertexID * 2], vertices[vertexID * 2 + 1]);
	// Original WG Large/XL records include a fifth parameters float4 (80-byte stride).
	// PiP consumes the first four fields without compacting that shared storage.
	const float4 lengthwidthrotation = instances[instanceID * 5 + 0];
	const float4 uv_atlaspos = instances[instanceID * 5 + 1];
	const float4 alphaParams = instances[instanceID * 5 + 2];
	const float4 worldPos = instances[instanceID * 5 + 3];

	float lifetonow = gameFrame - worldPos.w;
	float currentAlpha = alphaParams.x - lifetonow * alphaParams.y;
	currentAlpha = clamp(currentAlpha, 0.0, lengthwidthrotation.w);
	if (currentAlpha < 0.01) {
		output.f_alpha = 0.0;
		output.position = float4(2.0, 2.0, 0.0, 1.0);
		output.position.z = (output.position.z + output.position.w) * 0.5f; return output;
	}

	output.f_alpha = currentAlpha;
	float2 centerNDC = worldPos.xz * invMapSize - 1.0;

	float ca = cos(lengthwidthrotation.z);
	float sa = sin(lengthwidthrotation.z);
	float2 local = quadPos * (lengthwidthrotation.xy * 0.5);
	float2 rotatedWorld = float2(local.x * ca - local.y * sa, local.x * sa + local.y * ca);
	float2 ndcOff = rotatedWorld * invMapSize;
	output.position = float4(centerNDC + ndcOff, 0.0, 1.0);

	float2 t = quadPos * 0.5 + 0.5;
	output.f_texCoord = float2(mix(uv_atlaspos.x, uv_atlaspos.y, t.x), mix(uv_atlaspos.z, uv_atlaspos.w, t.y));

	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}






fragment float4 pipDecalFragment(PipDecalStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> atlasTex [[texture(0)]], sampler atlasTexSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
	float4 tex = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : atlasTex.sample(atlasTexSampler, PipUV(input.f_texCoord, uniforms.flipV[0])));
	float a = tex.a * tex.a * 0.999f * input.f_alpha;
	float3 scarColor = mix(tex.rgb, float3(0.5f), 0.35f);
	float3 result = mix(float3(1.0f), scarColor, a);
	output = float4(result, 1.0f);

	return output;
}
struct PipWaterStage { float4 position [[position]];
	float2 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};
			
			vertex PipWaterStage pipWaterVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipWaterStage output{};
	const PipImmediateVertex input = immediate[vertexID];
				output.texCoord = input.texCoord.xy;
				output.position = uniforms.modelViewProjection * input.position;
			
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}
			
			
			
			
			
			
			
			
			    
			
			 
			    
			
			    
			  
			   
			       
			        
			   
			   
			
			      
			     
			    
			      
			       
			   
			   
			   
			     
			    
			 
			

			float PipWaterrand(float2 co) {
				return fract(sin(dot(co, float2(12.9898f, 78.233f))) * 43758.5453f);
			}
			float PipWatervnoise(float2 p) {
				float2 i = floor(p);
				float2 f = fract(p);
				f = f * f * (3.0f - 2.0f * f);
				return mix(
					mix(PipWaterrand(i), PipWaterrand(i + float2(1.0f, 0.0f)), f.x),
					mix(PipWaterrand(i + float2(0.0f, 1.0f)), PipWaterrand(i + float2(1.0f, 1.0f)), f.x),
					f.y);
			}

			fragment float4 pipWaterFragment(PipWaterStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> heightTex [[texture(0)]], sampler heightTexSampler [[sampler(0)]], texture2d<float> lavaDiffuseTex [[texture(1)]], sampler lavaDiffuseTexSampler [[sampler(1)]], texture2d<float> lavaDistortTex [[texture(2)]], sampler lavaDistortTexSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
	const float4 waterColor = uniforms.values[0];
	const float waterLevel = uniforms.values[1].x;
	const float isLava = uniforms.values[2].x;
	const float hasLavaTex = uniforms.values[3].x;
	const float hasDistortTex = uniforms.values[4].x;
	const float gameFrames = uniforms.values[5].x;
	const float3 lavaCoastColor = uniforms.values[6].xyz;
	const float3 lavaHighlightColor = uniforms.values[7].xyz;
	const float3 colorCorrection = uniforms.values[8].xyz;
	const float lavaCoastWidth = uniforms.values[9].x;
	const float lavaUvScale = uniforms.values[10].x;
	const float mapRatio = uniforms.values[13].x;
	const float sunDirY = uniforms.values[14].x;
	const float heatDistortX = uniforms.values[15].x;
	const float heatDistortZ = uniforms.values[16].x;
	const float3 wSurfColor = uniforms.values[17].xyz;
	const float wSurfAlpha = uniforms.values[18].x;
	const float3 wAbsorbColor = uniforms.values[19].xyz;
	const float3 wBaseColor = uniforms.values[20].xyz;
	const float3 wMinColor = uniforms.values[21].xyz;
	const float wCausticsStr = uniforms.values[22].x;
	const float wPerlinStart = uniforms.values[23].x;
	const float wPerlinLacun = uniforms.values[24].x;
	const float wPerlinAmp = uniforms.values[25].x;
	const float wFresnelMin = uniforms.values[26].x;
	const float wDiffuseFactor = uniforms.values[27].x;
				float height = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : heightTex.sample(heightTexSampler, PipUV(input.texCoord, uniforms.flipV[0]))).r;
				float depth = waterLevel - height;

				if (isLava > 0.5f) {
					float lavaAlpha = clamp(depth / 5.0f, 0.0f, 1.0f);

					
					float coastfactor = clamp((height - waterLevel + lavaCoastWidth + 2.0f) / lavaCoastWidth, 0.0f, 1.0f);
					if (coastfactor > 0.90f) {
						coastfactor = 9.0f * (1.0f - coastfactor);
					} else {
						coastfactor = pow(coastfactor / 0.9f, 3.0f);
					}

					
					float2 worldUV = input.texCoord;
					worldUV.y *= mapRatio;

					
					
					worldUV += float2(sin(gameFrames * 0.0001f), cos(gameFrames * 0.0001f)) * 0.05f;

					
					float2 camshift = float2(heatDistortX, heatDistortZ) * 0.001f;

					
					
					float4 nodiffuseEmit = float4(0.5f);
					if (hasLavaTex > 0.5f) {
						nodiffuseEmit = ((uniforms.flipV.w & (1u << 1)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : lavaDiffuseTex.sample(lavaDiffuseTexSampler, PipUV(worldUV * lavaUvScale, uniforms.flipV[1])));
					}

					
					
					
					
					float flowTime = gameFrames * 0.001f;
					float2 flowUV = worldUV * 3.0f;

					
					float flow1x = PipWatervnoise(flowUV + float2(flowTime * 0.7f, flowTime * 0.3f)) - 0.5f;
					float flow1y = PipWatervnoise(flowUV + float2(-flowTime * 0.5f, flowTime * 0.6f) + float2(7.3f, 3.1f)) - 0.5f;

					
					float flow2x = PipWatervnoise(flowUV * 2.5f + float2(flowTime * 1.1f, -flowTime * 0.8f) + float2(13.7f, 5.9f)) - 0.5f;
					float flow2y = PipWatervnoise(flowUV * 2.5f + float2(-flowTime * 0.9f, flowTime * 1.3f) + float2(2.1f, 11.4f)) - 0.5f;

					float2 swirl = float2(flow1x + flow2x * 0.5f, flow1y + flow2y * 0.5f) * 0.06f;

					
					
					
					
					float2 distortion;
					if (hasDistortTex > 0.5f) {
						float4 distTex = ((uniforms.flipV.w & (1u << 2)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : lavaDistortTex.sample(lavaDistortTexSampler, PipUV((worldUV + camshift) * 8.0f, uniforms.flipV[2])));
						distortion = distTex.xy * 0.2f * 0.15f;
					} else {
						float dn1 = PipWatervnoise((worldUV + camshift) * 8.0f);
						float dn2 = PipWatervnoise((worldUV + camshift) * 8.0f * 1.7f + float2(3.7f, 1.2f));
						distortion = float2(dn1, dn2) * 0.03f;
					}
					distortion *= clamp(nodiffuseEmit.a * 0.5f + coastfactor, 0.2f, 2.0f);

					
					
					
					float2 finalUV = worldUV * lavaUvScale + distortion + swirl;

					float3 baseColor;
					float emissive;
					if (hasLavaTex > 0.5f) {
						float4 lavaSample = ((uniforms.flipV.w & (1u << 1)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : lavaDiffuseTex.sample(lavaDiffuseTexSampler, PipUV(finalUV, uniforms.flipV[1])));
						baseColor = lavaSample.rgb;
						emissive = lavaSample.a;
					} else {
						float n1 = PipWatervnoise(finalUV * 12.0f);
						float n2 = PipWatervnoise(finalUV * 28.0f);
						float nv = n1 * 0.65f + n2 * 0.35f;
						baseColor = mix(waterColor.rgb, waterColor.rgb * 2.5f + lavaHighlightColor, nv * 0.4f);
						emissive = nv;
					}

					
					
					
					float lightamount = mix(1.0f, clamp(sunDirY, 0.2f, 1.0f), 0.5f);
					baseColor *= lightamount;

					
					baseColor += lavaCoastColor * coastfactor;

					
					
					
					baseColor += baseColor * (emissive * distortion.y * 140.0f);

					
					baseColor *= colorCorrection;

					float alpha = max(lavaAlpha, coastfactor * 0.85f);
					output = float4(baseColor, alpha);
				} else {
					
					
					
					float waterAmount = smoothstep(0.0f, 35.0f, depth) * 0.58f;

					if (waterColor.a > 0.95f) {
						
						output = float4(waterColor.rgb, waterAmount);
					} else {
						
						float2 worldUV = input.texCoord;
						worldUV.y *= mapRatio;

						
						
						float depthFactor = min(depth * 0.04f, 1.0f);
						float3 depthColor = wBaseColor - wAbsorbColor * depthFactor;
						depthColor = max(depthColor, wMinColor);
						
						
						float blueShift = max(depthColor.g - depthColor.b, 0.0f) * 0.8f;
						depthColor.g -= blueShift;
						depthColor.b += blueShift * 0.5f;
						
						
						
						
						
						float reflectBright = 1.0f + (wFresnelMin - 0.2f) * 1.0f;
						
						float diffuseBright = 1.0f + (wDiffuseFactor - 1.0f) * 0.15f;
						float mapBrightness = clamp(reflectBright * diffuseBright, 0.6f, 2.0f);

						
						depthColor *= 0.45f * mapBrightness;
						
						float deepDarken = mix(1.0f, 0.6f, smoothstep(10.0f, 80.0f, depth));
						depthColor *= deepDarken;

						
						float3 surfColor = wSurfColor * 0.25f * mapBrightness;
						
						float surfBlueShift = max(surfColor.g - surfColor.b, 0.0f) * 0.8f;
						surfColor.g -= surfBlueShift;
						surfColor.b += surfBlueShift * 0.5f;

						
						float waveTime = gameFrames * 0.0005f;
						float freq1 = wPerlinStart;
						float freq2 = freq1 * wPerlinLacun;
						float freq3 = freq2 * wPerlinLacun;
						float wave1 = PipWatervnoise(worldUV * freq1 + float2(waveTime * 0.3f, waveTime * 0.2f));
						float wave2 = PipWatervnoise(worldUV * freq2 + float2(-waveTime * 0.4f, waveTime * 0.35f) + float2(3.7f, 1.2f));
						float wave3 = PipWatervnoise(worldUV * freq3 + float2(waveTime * 0.5f, -waveTime * 0.45f) + float2(7.3f, 5.9f));
						float ampTotal = 1.0f + wPerlinAmp + wPerlinAmp * wPerlinAmp;
						float waveNoise = (wave1 + wave2 * wPerlinAmp + wave3 * wPerlinAmp * wPerlinAmp) / ampTotal;

						
						float surfaceVar = mix(0.92f, 1.08f, waveNoise);

						
						float causticsDepthScale = smoothstep(0.0f, 5.0f, depth) * smoothstep(40.0f, 15.0f, depth);
						float c1 = PipWatervnoise(worldUV * 20.0f + float2(waveTime * 0.6f, -waveTime * 0.3f));
						float c2 = PipWatervnoise(worldUV * 35.0f + float2(-waveTime * 0.4f, waveTime * 0.7f) + float2(5.3f, 8.7f));
						float caustics = pow(c1 * 0.5f + c2 * 0.5f, 2.0f) * wCausticsStr * 6.0f * causticsDepthScale;

						
						float shoreFactor = smoothstep(3.0f, 0.0f, depth) * smoothstep(-0.5f, 0.5f, depth);
						float foamNoise = PipWatervnoise(worldUV * 40.0f + float2(waveTime * 2.0f, -waveTime * 1.5f));
						float foam = shoreFactor * foamNoise * 0.4f;

						
						
						float3 waterResult = mix(depthColor, surfColor, wSurfAlpha * 0.3f) * surfaceVar;
						waterResult += float3(caustics);
						waterResult += float3(foam);

						
						float lightamount = mix(1.0f, clamp(sunDirY, 0.2f, 1.0f), 0.5f);
						waterResult *= lightamount;

						output = float4(waterResult, waterAmount);
					}
				}
			
	return output;
}
struct PipIconStage { float4 position [[position]];
	float2 f_texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 f_color [[RECOIL_SAMPLE_PERSPECTIVE]];
	float f_flash [[flat]];
};

         
   
         
      
















vertex PipIconStage pipIconVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipIconStage output{};
	const float2 wtp_scale = uniforms.values[0].xy;
	const float2 wtp_offset = uniforms.values[1].xy;
	const float2 ndcScale = uniforms.values[2].xy;
	const float2 rotSC = uniforms.values[3].xy;
	const float2 rotCenter = uniforms.values[4].xy;
	const float iconBaseSize = uniforms.values[5].x;
	const float gameTime = uniforms.values[6].x;
	const float wallClockTime = uniforms.values[7].x;
	const float outlinePass = uniforms.values[8].x;
	const float healthDarkenMax = uniforms.values[9].x;
	const float2 quadPos = float2(vertices[vertexID * 2], vertices[vertexID * 2 + 1]);
	const float4 worldPos_size = instances[instanceID * 3 + 0];
	const float4 atlasUV = instances[instanceID * 3 + 1];
	const float4 colorFlags = instances[instanceID * 3 + 2];
	float flags = worldPos_size.w;
	float bitFlags = PipGLMod(flags, 32.0f);
	float healthPct = floor(flags / 32.0f);
	float healthFrac = clamp(healthPct / 100.0f, 0.0f, 1.0f);
	float isRadar    = PipGLMod(floor(bitFlags      ), 2.0f);
	float isTakeable = PipGLMod(floor(bitFlags / 2.0f ), 2.0f);
	float isStunned  = PipGLMod(floor(bitFlags / 4.0f ), 2.0f);
	float isTracked  = PipGLMod(floor(bitFlags / 8.0f ), 2.0f);
	float isSelfD    = PipGLMod(floor(bitFlags / 16.0f), 2.0f);

	float packedW = colorFlags.w;
	float flashFactor = floor(packedW / 7.0f) / 100.0f;
	float phase = PipGLMod(packedW, 7.0f);

	float2 pipPos;
	pipPos.x = wtp_offset.x + worldPos_size.x * wtp_scale.x;
	pipPos.y = wtp_offset.y + worldPos_size.y * wtp_scale.y;

	float wobbleAmp = iconBaseSize * sqrt(abs(wtp_scale.x)) * 0.03f * isRadar;
	pipPos.x += sin(gameTime * 3.0f + phase) * wobbleAmp;
	pipPos.y += cos(gameTime * 2.7f + phase * 1.3f) * wobbleAmp;

	float2 d = pipPos - rotCenter;
	pipPos = rotCenter + float2(
		d.x * rotSC.y - d.y * rotSC.x,
		d.x * rotSC.x + d.y * rotSC.y
	);

	float2 centerNDC = pipPos * ndcScale - 1.0f;

	float3 col = colorFlags.rgb;
	float alpha = 1.0f - 0.25f * isRadar;

	float takeableBlink = step(0.0f, sin(wallClockTime * 9.42f));
	alpha *= mix(1.0f, mix(0.3f, 1.0f, takeableBlink), isTakeable);

	float damage = 1.0f - healthFrac;
	col = max(col - float3(damage * healthDarkenMax), float3(0.0f));

	float stunnedPulse = 0.5f + 0.5f * sin(wallClockTime * 12.57f);
	col = mix(col, float3(0.82f, 0.85f, 1.0f), 0.45f * stunnedPulse * isStunned);

	float selfDPulse = 0.55f + 0.45f * sin(wallClockTime * 18.85f);
	col = mix(col, float3(1.0f, 0.25f, 0.0f), 0.7f * selfDPulse * isSelfD);

	if (outlinePass > 0.5f) {
		float hasOutline = max(isTracked, isSelfD);
		if (hasOutline < 0.5f) {
			alpha = 0.0f;
		} else if (isSelfD > 0.5f) {
			float selfDOutlinePulse = 0.5f + 0.5f * sin(wallClockTime * 18.85f);
			col = float3(1.0f, 0.3f, 0.0f);
			alpha = 0.4f + 0.4f * selfDOutlinePulse;
		} else {
			col = float3(1.0f, 1.0f, 1.0f);
			alpha = 0.5f;
		}
	}

	float hasOutline = max(isTracked, isSelfD);
	float sizeExpand = 1.0f + 0.12f * outlinePass * hasOutline;
	float2 halfSizeNDC = float2(iconBaseSize * worldPos_size.z * sizeExpand) * ndcScale;

	output.position = float4(centerNDC + quadPos * halfSizeNDC, 0.0f, 1.0f);
	float2 t = quadPos * 0.5f + 0.5f;
	output.f_texCoord = float2(mix(atlasUV.x, atlasUV.z, t.x), mix(atlasUV.y, atlasUV.w, t.y));
	output.f_color = float4(col, alpha);
	output.f_flash = flashFactor;

	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}

		
		
		
		
		

		fragment float4 pipIconFragment(PipIconStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> iconAtlas [[texture(0)]], sampler iconAtlasSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
			float4 texColor = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : iconAtlas.sample(iconAtlasSampler, PipUV(input.f_texCoord, uniforms.flipV[0])));
			float4 result = texColor * input.f_color;
			result.rgb += input.f_color.rgb * input.f_flash * (1.0f - texColor.rgb);
			if (result.a < 0.01f) discard_fragment();
			output = result;
		
	return output;
}
struct PipCircleStage { float4 position [[position]];
	float2 f_localCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 f_coreColor [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 f_edgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];
	float f_blendMode [[flat]];
};

		
		
		
		

		
		
		
		
		

		
		
		
		

		vertex PipCircleStage pipCircleVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipCircleStage output{};
	const float2 wtp_scale = uniforms.values[0].xy;
	const float2 wtp_offset = uniforms.values[1].xy;
	const float2 ndcScale = uniforms.values[2].xy;
	const float2 rotSC = uniforms.values[3].xy;
	const float2 rotCenter = uniforms.values[4].xy;
	const float2 quadPos = float2(vertices[vertexID * 2], vertices[vertexID * 2 + 1]);
	const float4 posRadius = instances[instanceID * 3 + 0];
	const float4 coreColor = instances[instanceID * 3 + 1];
	const float4 edgeColor = instances[instanceID * 3 + 2];
			float2 pipPos = wtp_offset + posRadius.xy * wtp_scale;
			float2 d = pipPos - rotCenter;
			pipPos = rotCenter + float2(d.x*rotSC.y - d.y*rotSC.x, d.x*rotSC.x + d.y*rotSC.y);
			float2 centerNDC = pipPos * ndcScale - 1.0f;

			float radiusPIP = posRadius.z * abs(wtp_scale.x);
			float2 halfSizeNDC = float2(radiusPIP) * ndcScale;

			output.position = float4(centerNDC + quadPos * halfSizeNDC, 0.0f, 1.0f);
			output.f_localCoord = quadPos;
			output.f_coreColor = float4(coreColor.rgb, posRadius.w);
			output.f_edgeColor = float4(edgeColor.rgb, coreColor.w);
			output.f_blendMode = edgeColor.w;
		
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}

		
		
		
		
		

		fragment float4 pipCircleFragment(PipCircleStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
			float dist = length(input.f_localCoord);
			if (dist > 1.0f) discard_fragment();
			float t = smoothstep(0.0f, 1.0f, dist);
			output = mix(input.f_coreColor, input.f_edgeColor, t);
		
	return output;
}
struct PipQuadStage { float4 position [[position]];
	float4 f_color [[RECOIL_SAMPLE_PERSPECTIVE]];
	float2 f_texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};

		
		
		
		

		
		
		
		
		

		

		vertex PipQuadStage pipQuadVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipQuadStage output{};
	const float2 wtp_scale = uniforms.values[0].xy;
	const float2 wtp_offset = uniforms.values[1].xy;
	const float2 ndcScale = uniforms.values[2].xy;
	const float2 rotSC = uniforms.values[3].xy;
	const float2 rotCenter = uniforms.values[4].xy;
	const float2 quadPos = float2(vertices[vertexID * 2], vertices[vertexID * 2 + 1]);
	const float4 posSizeIn = instances[instanceID * 4 + 0];
	const float4 colorIn = instances[instanceID * 4 + 1];
	const float4 angleFlags = instances[instanceID * 4 + 2];
	const float4 uvRectIn = instances[instanceID * 4 + 3];

			float2 pipPos = wtp_offset + posSizeIn.xy * wtp_scale;
			float2 d = pipPos - rotCenter;
			pipPos = rotCenter + float2(d.x*rotSC.y - d.y*rotSC.x, d.x*rotSC.x + d.y*rotSC.y);
			float2 centerNDC = pipPos * ndcScale - 1.0;

			float scalePIP = abs(wtp_scale.x);
			float2 hs = float2(posSizeIn.z * scalePIP, posSizeIn.w * scalePIP);
			float a = PipGLRadians(angleFlags.x);
			float sa = sin(a), ca = cos(a);

			float sT = sa * rotSC.y + ca * rotSC.x;
			float cT = ca * rotSC.y - sa * rotSC.x;

			float2 localPx = quadPos * hs;
			float2 rotatedPx = float2(
				localPx.x * cT - localPx.y * sT,
				localPx.x * sT + localPx.y * cT
			);
			output.position = float4(centerNDC + rotatedPx * ndcScale, 0.0, 1.0);
			output.f_color = colorIn;
			float2 uvMix = quadPos * 0.5 + 0.5;
			output.f_texCoord = mix(uvRectIn.xy, uvRectIn.zw, uvMix);
		
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}

		
		
		fragment float4 pipQuadFragment(PipQuadStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> atlasTex [[texture(0)]], sampler atlasTexSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	const float4 texel = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : atlasTex.sample(atlasTexSampler, PipUV(input.f_texCoord, uniforms.flipV[0])));
	return texel * input.f_color;
}
struct PipLineStage { float4 position [[position]];
	float4 f_color [[RECOIL_SAMPLE_PERSPECTIVE]];
};

		
		

		
		
		
		
		

		

		vertex PipLineStage pipLineVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipLineStage output{};
	const float2 wtp_scale = uniforms.values[0].xy;
	const float2 wtp_offset = uniforms.values[1].xy;
	const float2 ndcScale = uniforms.values[2].xy;
	const float2 rotSC = uniforms.values[3].xy;
	const float2 rotCenter = uniforms.values[4].xy;
	const float4 colorMul = uniforms.values[5].xyzw;
	const float3 mapLineFade = uniforms.values[6].xyz;
	const float2 worldPos = float2(vertices[vertexID * 6], vertices[vertexID * 6 + 1]);
	const float4 vertColor = float4(vertices[vertexID * 6 + 2], vertices[vertexID * 6 + 3], vertices[vertexID * 6 + 4], vertices[vertexID * 6 + 5]);

			float2 pipPos = wtp_offset + worldPos * wtp_scale;
			float2 d = pipPos - rotCenter;
			pipPos = rotCenter + float2(d.x*rotSC.y - d.y*rotSC.x, d.x*rotSC.x + d.y*rotSC.y);
			output.position = float4(pipPos * ndcScale - 1.0, 0.0, 1.0);
			float alpha = vertColor.a;
			if (mapLineFade.x > 0.5) {
				float fadeDuration = min(1.0, mapLineFade.z);
				float remaining = mapLineFade.z - (mapLineFade.y - vertColor.a);
				alpha = 0.85 * clamp(remaining / fadeDuration, 0.0, 1.0);
			}
			output.f_color = float4(vertColor.rgb, alpha) * colorMul;
		
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}

		
		
		fragment float4 pipLineFragment(PipLineStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float4 output{};
			output = input.f_color;
		
	return output;
}

// Original non-smooth GL line coverage is expanded by NativeScreenLine after
// the original line vertex expression. These are its actual pixel triangles.
vertex PipLineStage pipLineCoverageVertex(uint vertexID [[vertex_id]],
    device const PipImmediateVertex* immediate [[buffer(0)]]) {
    PipLineStage output;
    output.position = immediate[vertexID].position;
    output.f_color = immediate[vertexID].color;
    return output;
}

struct PipNanoStreamStage { float4 position [[position]];
	float4 f_color [[RECOIL_SAMPLE_PERSPECTIVE]];
	float2 f_texCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex PipNanoStreamStage pipNanoStreamVertex(uint vertexID [[vertex_id]], uint instanceID [[instance_id]], device const PipImmediateVertex* immediate [[buffer(0)]], device const float* vertices [[buffer(1)]], device const float4* instances [[buffer(2)]], constant PipUniformBlock& uniforms [[buffer(3)]]) {
	PipNanoStreamStage output{};
	const float2 wtp_scale = uniforms.values[0].xy;
	const float2 wtp_offset = uniforms.values[1].xy;
	const float2 ndcScale = uniforms.values[2].xy;
	const float2 rotSC = uniforms.values[3].xy;
	const float2 rotCenter = uniforms.values[4].xy;
	const float gameTime = uniforms.values[5].x;
	const float uiScale = uniforms.values[6].x;
	const float cameraZoom = uniforms.values[7].x;
	const float2 quadPos = float2(vertices[vertexID * 2], vertices[vertexID * 2 + 1]);
	const float4 endpoints = instances[instanceID * 2 + 0];
	const float4 colorUsage = instances[instanceID * 2 + 1];

			float2 startPos = wtp_offset + endpoints.xy * wtp_scale;
			float2 endPos = wtp_offset + endpoints.zw * wtp_scale;
			float2 startDelta = startPos - rotCenter;
			float2 endDelta = endPos - rotCenter;
			startPos = rotCenter + float2(startDelta.x * rotSC.y - startDelta.y * rotSC.x, startDelta.x * rotSC.x + startDelta.y * rotSC.y);
			endPos = rotCenter + float2(endDelta.x * rotSC.y - endDelta.y * rotSC.x, endDelta.x * rotSC.x + endDelta.y * rotSC.y);

			float2 delta = endPos - startPos;
			float lineLength = max(length(delta), 1.0);
			float2 perpendicular = float2(-delta.y, delta.x) / lineLength;
			float along = quadPos.y * 0.5 + 0.5;
			float packedUsage = colorUsage.a;
			float flowDirection = 1.0;
			float usage = packedUsage;
			if (packedUsage >= 1.5) {
				usage = packedUsage - 2.0;
				flowDirection = -1.0;
			} else if (packedUsage <= -1.5) {
				usage = -1.0;
				flowDirection = -1.0;
			}
			float strength = usage < 0.0 ? 0.65 : sqrt(clamp(usage, 0.0, 1.0));
			float widthZoom = clamp(sqrt(max(cameraZoom, 0.01) / 0.25), 0.75, 2.0);
			float halfWidth = (0.65 + strength * 0.7) * widthZoom * uiScale * 2.0;
			float2 pipPos = mix(startPos, endPos, along) + perpendicular * quadPos.x * halfWidth;

			output.position = float4(pipPos * ndcScale - 1.0, 0.0, 1.0);
			output.f_color = float4(colorUsage.rgb, usage < 0.0 ? 0.48 : 0.12 + strength * 0.5);
			float repeatZoomCompensation = 0.55 / max(cameraZoom, 0.01);
			float repeats = max(lineLength * repeatZoomCompensation / (14.0 * uiScale), 2.0);
			output.f_texCoord = float2(along * repeats - gameTime * 2.4 * flowDirection, quadPos.x * 0.5 + 0.5);
		
	output.position.z = (output.position.z + output.position.w) * 0.5f;
	return output;
}
fragment float4 pipNanoStreamFragment(PipNanoStreamStage input [[stage_in]], constant PipUniformBlock& uniforms [[buffer(3)]], texture2d<float> nanoTex [[texture(0)]], sampler nanoTexSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	const float4 texel = ((uniforms.flipV.w & (1u << 0)) ? float4(0.0f, 0.0f, 0.0f, 1.0f) : nanoTex.sample(nanoTexSampler, PipUV(float2(fract(input.f_texCoord.x), input.f_texCoord.y), uniforms.flipV[0])));
	return float4(input.f_color.rgb * texel.rgb, input.f_color.a * texel.a);
}
)PIPMSL";
}
