/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBloomData.h"
#include <charconv>
#include <cmath>
#include <string_view>
namespace rendering::bloomProgram {
inline constexpr std::string_view CombineVertex=R"BLOOM(				void main(void)	{
					gl_TexCoord[0] = vec4(gl_Vertex.zwzw);
					#if DOWNSCALE >= 2
						// correct for the rounding pad: only the [0, VSX/(DOWNSCALE*HSX)] sub-region of mip1 covers the screen
						gl_TexCoord[0].xy = vec2(gl_TexCoord[0].xy * vec2(DOWNSCALE * HSX, DOWNSCALE * HSY ) /  vec2(VSX, VSY));
					#endif
					gl_Position    = vec4(gl_Vertex.xy, 0, 1);	}
			)BLOOM";
inline constexpr std::string_view CombineFragment=R"BLOOM(				uniform sampler2D texture0;
				uniform int debugDraw;
				uniform float bloomNorm;

				void main(void) {
					vec4 a = texture2D(texture0, gl_TexCoord[0].st);
					a.rgb *= bloomNorm;
					if (debugDraw == 1) {
						a.a = 1.0;
					}
					gl_FragColor = a;
				}
			)BLOOM";
inline constexpr std::string_view DownsampleVertex=R"BLOOM(			uniform sampler2D source;
			flat out vec2 sourceTexelSize;
			flat out int firstPass;
			void main(void)	{
				ivec2 sourceSize = textureSize(source, 0);
				sourceTexelSize = 1.0 / vec2(sourceSize);
				firstPass = int(sourceSize == ivec2(HSX, HSY));
				gl_TexCoord[0] = vec4(gl_Vertex.zwzw);
				gl_Position    = vec4(gl_Vertex.xy, 0, 1);
			}
		)BLOOM";
inline constexpr std::string_view DownsampleFragment=R"BLOOM(			uniform sampler2D source;
			flat in vec2 sourceTexelSize;
			flat in int firstPass;

			float karisWeight(vec3 c) {
				// Rec.709 luminance, then Karis average weight 1/(1+L)
				float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
				return 1.0 / (1.0 + l * 0.25);
			}

			void main(void) {
				vec2 uv = gl_TexCoord[0].xy;
				vec2 px = sourceTexelSize;

				vec3 a = texture2D(source, uv + px * vec2(-2.0, -2.0)).rgb;
				vec3 b = texture2D(source, uv + px * vec2( 0.0, -2.0)).rgb;
				vec3 c = texture2D(source, uv + px * vec2( 2.0, -2.0)).rgb;
				vec3 d = texture2D(source, uv + px * vec2(-2.0,  0.0)).rgb;
				vec3 e = texture2D(source, uv).rgb;
				vec3 f = texture2D(source, uv + px * vec2( 2.0,  0.0)).rgb;
				vec3 g = texture2D(source, uv + px * vec2(-2.0,  2.0)).rgb;
				vec3 h = texture2D(source, uv + px * vec2( 0.0,  2.0)).rgb;
				vec3 i = texture2D(source, uv + px * vec2( 2.0,  2.0)).rgb;
				vec3 j = texture2D(source, uv + px * vec2(-1.0, -1.0)).rgb;
				vec3 k = texture2D(source, uv + px * vec2( 1.0, -1.0)).rgb;
				vec3 l = texture2D(source, uv + px * vec2(-1.0,  1.0)).rgb;
				vec3 m = texture2D(source, uv + px * vec2( 1.0,  1.0)).rgb;

				// Five 4-tap groups (each samples a 2x2 area)
				vec3 g0 = (a + b + d + e) * 0.25;
				vec3 g1 = (b + c + e + f) * 0.25;
				vec3 g2 = (d + e + g + h) * 0.25;
				vec3 g3 = (e + f + h + i) * 0.25;
				vec3 g4 = (j + k + l + m) * 0.25;

				vec3 result;
				if (firstPass == 1) {
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
				gl_FragColor = vec4(result, 1.0);
			}
		)BLOOM";
inline constexpr std::string_view UpsampleVertex=R"BLOOM(			#version 150 compatibility
			uniform sampler2D source;
			flat out vec2 sourceTexelSize;
			void main(void)	{
				sourceTexelSize = 1.0 / vec2(textureSize(source, 0));
				gl_TexCoord[0] = vec4(gl_Vertex.zwzw);
				gl_Position    = vec4(gl_Vertex.xy, 0, 1);
			}
		)BLOOM";
inline constexpr std::string_view UpsampleFragment=R"BLOOM(			uniform sampler2D source;
			flat in vec2 sourceTexelSize;
			uniform float filterRadius;

			void main(void) {
				vec2 uv = gl_TexCoord[0].xy;
				float x = sourceTexelSize.x * filterRadius;
				float y = sourceTexelSize.y * filterRadius;

				vec3 a = texture2D(source, uv + vec2(-x, -y)).rgb;
				vec3 b = texture2D(source, uv + vec2( 0, -y)).rgb;
				vec3 c = texture2D(source, uv + vec2( x, -y)).rgb;
				vec3 d = texture2D(source, uv + vec2(-x,  0)).rgb;
				vec3 e = texture2D(source, uv).rgb;
				vec3 f = texture2D(source, uv + vec2( x,  0)).rgb;
				vec3 g = texture2D(source, uv + vec2(-x,  y)).rgb;
				vec3 h = texture2D(source, uv + vec2( 0,  y)).rgb;
				vec3 i = texture2D(source, uv + vec2( x,  y)).rgb;

				// 3x3 tent: center 4, edges 2, corners 1 -> divide by 16
				vec3 result = e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i);
				result *= (1.0 / 16.0);
				gl_FragColor = vec4(result, 1.0);
			}
		)BLOOM";
inline constexpr std::string_view FinalUpsampleVertex=R"BLOOM(			#version 150 compatibility
			void main(void)	{
				gl_TexCoord[0] = vec4(gl_Vertex.zwzw);
				gl_Position    = vec4(gl_Vertex.xy, 0, 1);
			}
		)BLOOM";
inline constexpr std::string_view FinalUpsampleFragment=R"BLOOM(			uniform sampler2D source;
			uniform sampler2D brightTex;
			uniform sampler2D historyTex;
			uniform vec2 sourceTexelSize;
			uniform float filterRadius;
			uniform float historyMix;

			void main(void) {
				vec2 uv = gl_TexCoord[0].xy;
				float x = sourceTexelSize.x * filterRadius;
				float y = sourceTexelSize.y * filterRadius;

				vec3 a = texture2D(source, uv + vec2(-x, -y)).rgb;
				vec3 b = texture2D(source, uv + vec2( 0, -y)).rgb;
				vec3 c = texture2D(source, uv + vec2( x, -y)).rgb;
				vec3 d = texture2D(source, uv + vec2(-x,  0)).rgb;
				vec3 e = texture2D(source, uv).rgb;
				vec3 f = texture2D(source, uv + vec2( x,  0)).rgb;
				vec3 g = texture2D(source, uv + vec2(-x,  y)).rgb;
				vec3 h = texture2D(source, uv + vec2( 0,  y)).rgb;
				vec3 i = texture2D(source, uv + vec2( x,  y)).rgb;

				// 3x3 tent: center 4, edges 2, corners 1 -> divide by 16
				vec3 tent = e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i);
				vec3 cur = texture2D(brightTex, uv).rgb + tent * (1.0 / 16.0);
				vec3 hist = texture2D(historyTex, uv).rgb;
				gl_FragColor = vec4(mix(cur, hist, historyMix), 1.0);
			}
		)BLOOM";
inline constexpr std::string_view BrightVertex=R"BLOOM(			#version 150 compatibility
			void main(void)	{
				gl_TexCoord[0] = vec4(gl_Vertex.zwzw);
				gl_Position    = vec4(gl_Vertex.xy, 0, 1);	}
		)BLOOM";
inline constexpr std::string_view BrightFragment=R"BLOOM(
			uniform sampler2D modelDiffuseTex;
			uniform sampler2D modelEmitTex;

			uniform sampler2D modelDepthTex;
			uniform sampler2D mapDepthTex;

			uniform float illuminationThreshold;
			uniform float kneeWidth;
			uniform float fragGlowAmplifier;
			uniform float maxBrightContribution;

			void main(void) {
				// Center texture coordinates correctly (rounding pad correction)
				vec2 texCoors = vec2(gl_TexCoord[0].xy * vec2(VSX, VSY) / vec2(DOWNSCALE * HSX, DOWNSCALE * HSY ));
				#if DOWNSCALE <= 2
					float modelDepth = texture2D(modelDepthTex, texCoors).r;

					// Bail early if this is not a model fragment
					if (modelDepth > 0.9999) {
						gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
						return;
					}

					// Bail before the color fetches if the model is occluded by the map
					float mapDepth = texture2D(mapDepthTex, texCoors).r;
					if (modelDepth >= mapDepth) {
						gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
						return;
					}

					vec4 color = texture2D(modelDiffuseTex, texCoors);
					vec4 colorEmit = texture2D(modelEmitTex, texCoors);

				#else
					// downscale by 3 case
					vec2 offset = vec2(1.0/VSX, 1.0/VSY) * 0.56;
					float modelDepth1 = texture2D(modelDepthTex, texCoors + offset).r;
					float modelDepth2 = texture2D(modelDepthTex, texCoors - offset).r;

					if ((modelDepth1 + modelDepth2) > 1.9999) {
						gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
						return;
					}

					// Bail before the color fetches if the model is occluded by the map
					float mapDepth = texture2D(mapDepthTex, texCoors).r;
					if ((modelDepth1 + modelDepth2) * 0.5 >= mapDepth) {
						gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
						return;
					}

					vec4 color = vec4(texture2D(modelDiffuseTex, texCoors+ offset));
						 color += vec4(texture2D(modelDiffuseTex, texCoors- offset));
						 color *= 0.5;
					vec4 colorEmit = texture2D(modelEmitTex, texCoors+ offset);
						 colorEmit *=2;
						 colorEmit += texture2D(modelEmitTex, texCoors- offset);

				#endif


				// Handle transparency in color.a
				color.rgb = color.rgb * color.a;

				// Add the emit color
				color.rgb += colorEmit.rgb;

				// Proper Rec.709 luminance
				float illum = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

				// Soft-knee threshold: smoothstep from (T - knee) to (T + knee) instead of a hard cutoff.
				// This greatly reduces "popping" of pixels in/out of bloom and removes binary fireflies.
				float kneeLow  = illuminationThreshold - kneeWidth * 0.5;
				float kneeHigh = illuminationThreshold + kneeWidth * 0.5;
				float kneeMul  = smoothstep(kneeLow, kneeHigh, illum);

				// Standard "subtract threshold" bright pass (no extra (illum-threshold) gain).
				// In an HDR (FP16) pipeline that gain term is no longer clamped at 1.0, so it
				// would explode bright emissive blinks. Soft-cap the per-pixel contribution.
				vec3 excess = max(color.rgb - vec3(illuminationThreshold), vec3(0.0));
				vec3 brightOutput = excess * fragGlowAmplifier * kneeMul;
				brightOutput = min(brightOutput, vec3(maxBrightContribution));

				gl_FragColor = vec4(brightOutput, 1.0);
			}
		)BLOOM";
inline NativeBloomDefines ParseDefines(std::string_view source){
 NativeBloomDefines values;
 while(!source.empty()){
  const auto end=source.find('\n');if(end==source.npos)throw std::runtime_error("Original Bloom define has no newline");auto row=source.substr(0,end);source.remove_prefix(end+1);
  if(row.empty())continue;if(!row.starts_with("#define "))throw std::runtime_error("Original Bloom unexpected shader prefix");row.remove_prefix(8);auto split=row.find_first_of(" \t");if(split==row.npos)throw std::runtime_error("Original Bloom define missing numeric token");std::string key(row.substr(0,split));row.remove_prefix(split);while(!row.empty()&&(row.front()==' '||row.front()=='\t'))row.remove_prefix(1);
  float value=0;auto parsed=std::from_chars(row.data(),row.data()+row.size(),value,std::chars_format::general);
  if(parsed.ec!=std::errc{}||parsed.ptr!=row.data()+row.size()||!std::isfinite(value)||!values.emplace(std::move(key),value).second)throw std::runtime_error("Original Bloom invalid/duplicate numeric define");
 }
 ValidateNativeBloomDefines(values);return values;
}
inline NativeBloomDefines Transport(std::string_view source,std::string_view body,bool bright){
 const std::string_view version=bright?"#version 150 compatibility \n":"#version 150 compatibility\n";
 if(!source.starts_with(version)||!source.ends_with(body))throw std::runtime_error("Original Bloom inline shader identity differs");source.remove_prefix(version.size());source.remove_suffix(body.size());return ParseDefines(source);
}
inline std::pair<NativeBloomProgram,NativeBloomDefines> Parse(const std::string&name,const std::string&vertex,const std::string&fragment){
 if(name=="Bloom Combine Shader"){auto defines=Transport(fragment,CombineFragment,false);if(Transport(vertex,CombineVertex,false)!=defines)throw std::runtime_error("Original31657 Bloom vertex/define identity differs");return{NativeBloomProgram::Combine,std::move(defines)};}
 if(name=="Bloom Downsample Shader"){auto defines=Transport(fragment,DownsampleFragment,false);if(Transport(vertex,DownsampleVertex,false)!=defines)throw std::runtime_error("Original31657 Bloom vertex/define identity differs");return{NativeBloomProgram::Downsample,std::move(defines)};}
 if(name=="Bloom Upsample Shader"){auto defines=Transport(fragment,UpsampleFragment,false);if(vertex!=UpsampleVertex)throw std::runtime_error("Original31657 Bloom vertex/define identity differs");return{NativeBloomProgram::Upsample,std::move(defines)};}
 if(name=="Bloom Final Upsample Shader"){auto defines=Transport(fragment,FinalUpsampleFragment,false);if(vertex!=FinalUpsampleVertex)throw std::runtime_error("Original31657 Bloom vertex/define identity differs");return{NativeBloomProgram::FinalUpsample,std::move(defines)};}
 if(name=="Bloom Bright Shader"){auto defines=Transport(fragment,BrightFragment,true);if(vertex!=BrightVertex)throw std::runtime_error("Original31657 Bloom vertex/define identity differs");return{NativeBloomProgram::Bright,std::move(defines)};}
 throw std::runtime_error("Unknown original31657 Bloom program name");
}
inline constexpr uint64_t WidgetHash=0x5912701fb1d1a5b0ULL;
inline constexpr uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr uint64_t GeometryHelperHash=0x8cef105d28038367ULL;
}
