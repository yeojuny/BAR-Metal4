/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::bloom {inline constexpr char SHADER[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"BLOOM(
#include <metal_stdlib>
using namespace metal;
struct BloomUniform {float4 source,bright,temporal,alphaConfig;uint4 flip,swizzle[4],depthMode;};
struct BloomVarying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex BloomVarying BloomVertex(uint id [[vertex_id]],constant float4* vertices [[buffer(1)]],constant BloomUniform&u [[buffer(0)]]){
 float4 p=vertices[id];BloomVarying v;v.position=float4(p.xy,u.temporal.w>0?0:.5,1);v.uv=p.zw;
#ifdef BLOOM_COMBINE
 #if DOWNSCALE >= 2
 v.uv=v.uv*float2(DOWNSCALE*HSX,DOWNSCALE*HSY)/float2(VSX,VSY);
 #endif
#endif
 return v;
}
#if DEPTH_MASK & 1
using Texture0=depth2d<float>;
#else
using Texture0=texture2d<float>;
#endif
#if DEPTH_MASK & 2
using Texture1=depth2d<float>;
#else
using Texture1=texture2d<float>;
#endif
#if DEPTH_MASK & 4
using Texture2=depth2d<float>;
#else
using Texture2=texture2d<float>;
#endif
#if DEPTH_MASK & 8
using Texture3=depth2d<float>;
#else
using Texture3=texture2d<float>;
#endif
float4 LogicalRGBA(float4 v,uint unit,constant BloomUniform&u){return v;}
float4 LogicalRGBA(float d,uint unit,constant BloomUniform&u){uint mode=u.depthMode[unit];return mode==0x1906u?float4(1,1,1,d):mode==0x8049u?float4(d):float4(d,d,d,1);}
float Channel(float4 v,uint c){switch(c){case 0:return 0;case 1:return 1;case 0x1903:return v.r;case 0x1904:return v.g;case 0x1905:return v.b;case 0x1906:return v.a;default:return 0;}}
template<class T>float4 ReadColor(T image,sampler filter,float2 uv,uint unit,constant BloomUniform&u){if(u.flip.x&(1u<<unit))uv.y=1-uv.y;float4 v=LogicalRGBA(image.sample(filter,uv),unit,u);uint4 s=u.swizzle[unit];return float4(Channel(v,s.x),Channel(v,s.y),Channel(v,s.z),Channel(v,s.w));}
bool AlphaPass(float a,constant BloomUniform&u){if(u.alphaConfig.x==0)return true;float r=u.alphaConfig.y;switch(uint(u.alphaConfig.z)){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;default:return true;}}
fragment float4 BloomCombine(BloomVarying in [[stage_in]],constant BloomUniform&u [[buffer(0)]],Texture0 texture0 [[texture(0)]],sampler texture0Sampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

					float4 a = ReadColor(texture0,texture0Sampler,(in.uv),0,u);
					a.rgb *= u.temporal.y;
					if (int(u.temporal.z) == 1) {
						a.a = 1.0;
					}
					fragColor = a;
				
if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
float karisWeight(float3 c) {
				// Rec.709 luminance, then Karis average weight 1/(1+L)
				float l = dot(c, float3(0.2126, 0.7152, 0.0722));
				return 1.0 / (1.0 + l * 0.25);
			}

			
fragment float4 BloomDownsample(BloomVarying in [[stage_in]],constant BloomUniform&u [[buffer(0)]],Texture0 source [[texture(0)]],sampler sourceSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

				float2 uv = in.uv;
				float2 px = u.source.xy;

				float3 a = ReadColor(source,sourceSampler,(uv + px * float2(-2.0, -2.0)),0,u).rgb;
				float3 b = ReadColor(source,sourceSampler,(uv + px * float2( 0.0, -2.0)),0,u).rgb;
				float3 c = ReadColor(source,sourceSampler,(uv + px * float2( 2.0, -2.0)),0,u).rgb;
				float3 d = ReadColor(source,sourceSampler,(uv + px * float2(-2.0,  0.0)),0,u).rgb;
				float3 e = ReadColor(source,sourceSampler,(uv),0,u).rgb;
				float3 f = ReadColor(source,sourceSampler,(uv + px * float2( 2.0,  0.0)),0,u).rgb;
				float3 g = ReadColor(source,sourceSampler,(uv + px * float2(-2.0,  2.0)),0,u).rgb;
				float3 h = ReadColor(source,sourceSampler,(uv + px * float2( 0.0,  2.0)),0,u).rgb;
				float3 i = ReadColor(source,sourceSampler,(uv + px * float2( 2.0,  2.0)),0,u).rgb;
				float3 j = ReadColor(source,sourceSampler,(uv + px * float2(-1.0, -1.0)),0,u).rgb;
				float3 k = ReadColor(source,sourceSampler,(uv + px * float2( 1.0, -1.0)),0,u).rgb;
				float3 l = ReadColor(source,sourceSampler,(uv + px * float2(-1.0,  1.0)),0,u).rgb;
				float3 m = ReadColor(source,sourceSampler,(uv + px * float2( 1.0,  1.0)),0,u).rgb;

				// Five 4-tap groups (each samples a 2x2 area)
				float3 g0 = (a + b + d + e) * 0.25;
				float3 g1 = (b + c + e + f) * 0.25;
				float3 g2 = (d + e + g + h) * 0.25;
				float3 g3 = (e + f + h + i) * 0.25;
				float3 g4 = (j + k + l + m) * 0.25;

				float3 result;
				if (int(u.source.w) == 1) {
					float w0 = karisWeight(g0);
					float w1 = karisWeight(g1);
					float w2 = karisWeight(g2);
					float w3 = karisWeight(g3);
					float w4 = karisWeight(g4);
					float wt = w0 + w1 + w2 + w3 + w4;
					result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / wt;
				} else {
					// Standard Jimenez weights: center 0.5, four outer corners 0.125 each
					result = g4 * 0.5 + (g0 + g1 + g2 + g3) * 0.125;
				}
				fragColor = float4(result, 1.0);
			
if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
fragment float4 BloomUpsample(BloomVarying in [[stage_in]],constant BloomUniform&u [[buffer(0)]],Texture0 source [[texture(0)]],sampler sourceSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

				float2 uv = in.uv;
				float x = u.source.xy.x * u.source.z;
				float y = u.source.xy.y * u.source.z;

				float3 a = ReadColor(source,sourceSampler,(uv + float2(-x, -y)),0,u).rgb;
				float3 b = ReadColor(source,sourceSampler,(uv + float2( 0, -y)),0,u).rgb;
				float3 c = ReadColor(source,sourceSampler,(uv + float2( x, -y)),0,u).rgb;
				float3 d = ReadColor(source,sourceSampler,(uv + float2(-x,  0)),0,u).rgb;
				float3 e = ReadColor(source,sourceSampler,(uv),0,u).rgb;
				float3 f = ReadColor(source,sourceSampler,(uv + float2( x,  0)),0,u).rgb;
				float3 g = ReadColor(source,sourceSampler,(uv + float2(-x,  y)),0,u).rgb;
				float3 h = ReadColor(source,sourceSampler,(uv + float2( 0,  y)),0,u).rgb;
				float3 i = ReadColor(source,sourceSampler,(uv + float2( x,  y)),0,u).rgb;

				// 3x3 tent: center 4, edges 2, corners 1 -> divide by 16
				float3 result = e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i);
				result *= (1.0 / 16.0);
				fragColor = float4(result, 1.0);
			
if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
fragment float4 BloomFinalUpsample(BloomVarying in [[stage_in]],constant BloomUniform&u [[buffer(0)]],Texture0 source [[texture(0)]],sampler sourceSampler [[sampler(0)]],Texture1 brightTex [[texture(1)]],sampler brightTexSampler [[sampler(1)]],Texture2 historyTex [[texture(2)]],sampler historyTexSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

				float2 uv = in.uv;
				float x = u.source.xy.x * u.source.z;
				float y = u.source.xy.y * u.source.z;

				float3 a = ReadColor(source,sourceSampler,(uv + float2(-x, -y)),0,u).rgb;
				float3 b = ReadColor(source,sourceSampler,(uv + float2( 0, -y)),0,u).rgb;
				float3 c = ReadColor(source,sourceSampler,(uv + float2( x, -y)),0,u).rgb;
				float3 d = ReadColor(source,sourceSampler,(uv + float2(-x,  0)),0,u).rgb;
				float3 e = ReadColor(source,sourceSampler,(uv),0,u).rgb;
				float3 f = ReadColor(source,sourceSampler,(uv + float2( x,  0)),0,u).rgb;
				float3 g = ReadColor(source,sourceSampler,(uv + float2(-x,  y)),0,u).rgb;
				float3 h = ReadColor(source,sourceSampler,(uv + float2( 0,  y)),0,u).rgb;
				float3 i = ReadColor(source,sourceSampler,(uv + float2( x,  y)),0,u).rgb;

				// 3x3 tent: center 4, edges 2, corners 1 -> divide by 16
				float3 tent = e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i);
				float3 cur = ReadColor(brightTex,brightTexSampler,(uv),1,u).rgb + tent * (1.0 / 16.0);
				float3 hist = ReadColor(historyTex,historyTexSampler,(uv),2,u).rgb;
				fragColor = float4(mix(cur, hist, u.temporal.x), 1.0);
			
if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
fragment float4 BloomBright(BloomVarying in [[stage_in]],constant BloomUniform&u [[buffer(0)]],Texture0 modelDiffuseTex [[texture(0)]],sampler modelDiffuseTexSampler [[sampler(0)]],Texture1 modelEmitTex [[texture(1)]],sampler modelEmitTexSampler [[sampler(1)]],Texture2 modelDepthTex [[texture(2)]],sampler modelDepthTexSampler [[sampler(2)]],Texture3 mapDepthTex [[texture(3)]],sampler mapDepthTexSampler [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
float4 fragColor;

				// Center texture coordinates correctly (rounding pad correction)
				float2 texCoors = float2(in.uv * float2(VSX, VSY) / float2(DOWNSCALE * HSX, DOWNSCALE * HSY ));
				#if DOWNSCALE <= 2
					float modelDepth = ReadColor(modelDepthTex,modelDepthTexSampler,(texCoors),2,u).r;

					// Bail early if this is not a model fragment
					if (modelDepth > 0.9999) {
						fragColor = float4(0.0, 0.0, 0.0, 1.0);
						if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
					}

					// Bail before the color fetches if the model is occluded by the map
					float mapDepth = ReadColor(mapDepthTex,mapDepthTexSampler,(texCoors),3,u).r;
					if (modelDepth >= mapDepth) {
						fragColor = float4(0.0, 0.0, 0.0, 1.0);
						if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
					}

					float4 color = ReadColor(modelDiffuseTex,modelDiffuseTexSampler,(texCoors),0,u);
					float4 colorEmit = ReadColor(modelEmitTex,modelEmitTexSampler,(texCoors),1,u);

				#else
					// downscale by 3 case
					float2 offset = float2(1.0/VSX, 1.0/VSY) * 0.56;
					float modelDepth1 = ReadColor(modelDepthTex,modelDepthTexSampler,(texCoors + offset),2,u).r;
					float modelDepth2 = ReadColor(modelDepthTex,modelDepthTexSampler,(texCoors - offset),2,u).r;

					if ((modelDepth1 + modelDepth2) > 1.9999) {
						fragColor = float4(0.0, 0.0, 0.0, 1.0);
						if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
					}

					// Bail before the color fetches if the model is occluded by the map
					float mapDepth = ReadColor(mapDepthTex,mapDepthTexSampler,(texCoors),3,u).r;
					if ((modelDepth1 + modelDepth2) * 0.5 >= mapDepth) {
						fragColor = float4(0.0, 0.0, 0.0, 1.0);
						if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
					}

					float4 color = float4(ReadColor(modelDiffuseTex,modelDiffuseTexSampler,(texCoors+ offset),0,u));
						 color += float4(ReadColor(modelDiffuseTex,modelDiffuseTexSampler,(texCoors- offset),0,u));
						 color *= 0.5;
					float4 colorEmit = ReadColor(modelEmitTex,modelEmitTexSampler,(texCoors+ offset),1,u);
						 colorEmit *=2;
						 colorEmit += ReadColor(modelEmitTex,modelEmitTexSampler,(texCoors- offset),1,u);

				#endif


				// Handle transparency in color.a
				color.rgb = color.rgb * color.a;

				// Add the emit color
				color.rgb += colorEmit.rgb;

				// Proper Rec.709 luminance
				float illum = dot(color.rgb, float3(0.2126, 0.7152, 0.0722));

				// Soft-knee threshold: smoothstep from (T - knee) to (T + knee) instead of a hard cutoff.
				// This greatly reduces "popping" of pixels in/out of bloom and removes binary fireflies.
				float kneeLow  = u.bright.x - u.bright.y * 0.5;
				float kneeHigh = u.bright.x + u.bright.y * 0.5;
				float kneeMul  = smoothstep(kneeLow, kneeHigh, illum);

				// Standard "subtract threshold" bright pass (no extra (illum-threshold) gain).
				// In an HDR (FP16) pipeline that gain term is no longer clamped at 1.0, so it
				// would explode bright emissive blinks. Soft-cap the per-pixel contribution.
				float3 excess = max(color.rgb - float3(u.bright.x), float3(0.0));
				float3 brightOutput = excess * u.bright.z * kneeMul;
				brightOutput = min(brightOutput, float3(u.bright.w));

				fragColor = float4(brightOutput, 1.0);
			
if(!AlphaPass(fragColor.a,u))discard_fragment();return fragColor;
}
)BLOOM";}
