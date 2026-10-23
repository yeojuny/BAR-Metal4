/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Generated expression-preserving GLSL interface/type adaptation.
namespace rendering::metal4::projectile {
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
inline float mod(float x,float y){return x-y*floor(x/y); }
struct EffectUniform {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float parameters[42];uint4 options;};
struct BeamVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float coverage [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct BeamVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 position_xy_uv;float4 startPosAndWidth;float4 endPosAndLife;float4 coreColor;float4 edgeColor;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float alpha;float widthPos;float coverage;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Quad vertex: xy = corner position (-1..1), zw = UV


// Per-instance data
   // xyz = beam start, w = beam width (thickness)
       // xyz = beam end,   w = life fraction (0=new, 1=expired)
           // rgb = core color, a = alpha
           // rgb = edge color, a = range falloff factor



void run()
{
	float3 startPos = startPosAndWidth.xyz;
	float beamWidth = startPosAndWidth.w;
	float3 endPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	// Beam direction
	float3 beamDir = endPos - startPos;
	float beamLength = length(beamDir);
	if (beamLength < 0.01) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	float3 forward = beamDir / beamLength;

	// Camera-facing perpendicular for width
	float3 camPos = cameraViewInv[3].xyz;
	float3 toCamera = normalize(camPos - mix(startPos, endPos, 0.5));
	float3 right = cross(forward, toCamera);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fallback = normalize(cross(forward, float3(0.0, 1.0, 0.0)));
		if (length(fallback) < 0.001) {
			fallback = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
		}
		float blend = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fallback, right / max(rightLen, 0.001), blend));
	} else {
		right = right / rightLen;
	}

	// Map vertex x: -1..1 = across beam width
	// Map vertex y: -1..1 = along beam length (0..1 normalized)
	float yNorm = position_xy_uv.y * 0.5 + 0.5;  // 0 = start, 1 = end

	// Animated width: pulse in/out over lifetime
	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { gl_Position = float4(2.0, 2.0, 2.0, 1.0); return; }

	// Width also narrows slightly toward the end of the beam (range falloff)
	float rangeTaper = 1.0 - RANGE_TAPER * yNorm;

	// Slight shimmer
	float phase = startPos.x * 0.7 + startPos.z * 1.1 + lifeFrac * 13.0;
	float shimmer = 1.0 + SHIMMER_AMPLITUDE * sin(timeInfo.z * SHIMMER_SPEED + phase + yNorm * 6.28);

	float width = beamWidth * lifePulse * rangeTaper * shimmer;

	// Minimum screen-space width: prevent the beam from becoming sub-pixel
	// at distance, which causes aliasing/jaggedness. If the beam would be
	// thinner than MIN_PIXEL_WIDTH pixels, expand it and dim alpha to compensate.
	float3 vertPos = mix(startPos, endPos, yNorm);
	// Use beam midpoint for camDist so min-pixel-width is uniform along the
	// entire beam (per-vertex camDist causes start to appear narrower than middle)
	float camDist = length(camPos - mix(startPos, endPos, 0.5));
	float minWidth = camDist * MIN_PIXEL_WIDTH;
	float coverageVal = clamp(width / max(minWidth, 0.001), 0.0, 1.0);
	width = max(width, minWidth);

	float3 vertexWorld = vertPos
		+ right * position_xy_uv.x * width;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	// UV: u = along length, v = across width
	texCoords = float2(yNorm, position_xy_uv.z);

	// Pass raw data to FS for per-pixel core calculation
	vCoreColor = coreColor;
	vEdgeColor = edgeColor;
	widthPos = position_xy_uv.x;  // -1..1
	coverage = coverageVal;

	// Alpha: fade with lifetime and slight range falloff.
	// When the beam is widened to MIN_PIXEL_WIDTH (coverage < 1), we dim alpha to
	// conserve total emitted energy. Linear dim by `coverage` is energy-correct
	// but visually crushes the core at distance, while no dim at all over-bright
	// the inflated quad and produces line-ish jaggies. A sqrt curve is a good
	// middle ground: noticeably brighter than linear at small coverage, still
	// fades out gracefully, and lets fwidth-AA on the full inflated quad keep
	// edges smooth.
	float rangeFalloff = edgeColor.a;
	float alphaFalloff = 1.0 - rangeFalloff * yNorm;
	alpha = coreColor.a * lifePulse * alphaFalloff * sqrt(coverageVal);
}

};
vertex BeamVarying BeamVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){BeamVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.position_xy_uv=mesh[id];p.startPosAndWidth=instances[instance*5];p.endPosAndLife=instances[instance*5+1];p.coreColor=instances[instance*5+2];p.edgeColor=instances[instance*5+3];p.run();BeamVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;out.alpha=p.alpha;out.widthPos=p.widthPos;out.coverage=p.coverage;return out;}
struct BeamFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float alpha;float widthPos;float coverage;float4 fragColor;texture2d<float> beamTex;float4 sample(texture2d<float> t,float2 uv){constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear);return t.sample(s,uv);}





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__







void run()
{
	float4 texSample = sample(beamTex, texCoords);

	// Per-pixel core factor from widthPos (-1..1), center = core
	// Use fwidth() to ensure the core-to-edge transition always spans at least 1 pixel,
	// preventing jagged/aliased core lines on low-resolution screens or thin beams.
	float edgeDist = abs(widthPos);
	float fw = fwidth(edgeDist);
	float aaStart = CORE_EDGE_START - fw * 0.5;
	float aaEnd   = max(CORE_EDGE_END, CORE_EDGE_START + fw);
	float coreFactor = 1.0 - smoothstep(aaStart, aaEnd, edgeDist);

	// Blend core and edge colors per-pixel
	float3 beamCol = mix(vEdgeColor.rgb, vCoreColor.rgb, coreFactor);

	float3 color = texSample.rgb * beamCol * alpha;

	// Brightness boost
	color *= BRIGHTNESS_MULT;

	// Core gets extra brightness for a hot inner line
	color *= (1.0 + coreFactor * CORE_BRIGHTNESS);

	// Fade beam tip over final few % of length for a soft end instead of hard cutoff
	float tipFade = 1.0 - smoothstep(TIP_FADE_START, 1.0, texCoords.x);
	color *= tipFade;

	// Soft discard: fade out near-black fragments instead of hard discard
	// to avoid aliased edges on the outer boundary of the beam
	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.0005) discard_fragment();
	float edgeSoft = smoothstep(0.0005, 0.003, lum);
	color *= edgeSoft;

	fragColor = float4(color, 0.0);
}

};
fragment float4 BeamFragmentMain(BeamVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));BeamFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.texCoords=input.texCoords;p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.alpha=input.alpha;p.widthPos=input.widthPos;p.coverage=input.coverage;p.beamTex=image;p.run();return p.fragColor;}
struct BeamFlareVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct BeamFlareVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 position_xy_uv;float4 startPosAndWidth;float4 coreColor;float4 edgeColor;float4 flareData;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__



// Per-instance (shared layout with beam VBO)
   // xyz = beam start, w = beam width
           // rgb = core color, a = alpha
           // rgb = edge color, a = unused
           // x = flareSize, yzw = unused



void run()
{
	float3 worldPos = startPosAndWidth.xyz;
	float flareSize = flareData.x;

	// Skip instances with no flare
	if (flareSize <= 0.0) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	// Billboard: camera-facing quad
	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;

	float3 vertexWorld = worldPos
		+ camRight * position_xy_uv.x * flareSize
		+ camUp    * position_xy_uv.y * flareSize;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);
	texCoords = position_xy_uv.zw;
	vCoreColor = coreColor;
	vEdgeColor = edgeColor;
}

};
vertex BeamFlareVarying BeamFlareVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){BeamFlareVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.position_xy_uv=mesh[id];p.startPosAndWidth=instances[instance*5];p.coreColor=instances[instance*5+2];p.edgeColor=instances[instance*5+3];p.flareData=instances[instance*5+4];p.run();BeamFlareVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;return out;}
struct BeamFlareFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float4 fragColor;texture2d<float> flareTex;float4 sample(texture2d<float> t,float2 uv){constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear);return t.sample(s,uv);}





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__







void run()
{
	float4 texSample = sample(flareTex, texCoords);
	float shape = max(texSample.a, dot(texSample.rgb, float3(0.299, 0.587, 0.114)));

	if (shape < 0.001) discard_fragment();

	// Radial core factor: same logic as beam but using radial distance
	float2 centered = texCoords * 2.0 - 1.0;
	float dist = length(centered);
	float coreFactor = 1.0 - smoothstep(CORE_EDGE_START, CORE_EDGE_END, dist);

	// Blend core and edge colors per-pixel (same as beam)
	float3 flareCol = mix(vEdgeColor.rgb, vCoreColor.rgb, coreFactor);

	float3 color = flareCol * shape * BRIGHTNESS_MULT;

	// Core gets extra brightness for a hot center (same as beam)
	color *= (1.0 + coreFactor * CORE_BRIGHTNESS);

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();

	// Additive blending (GL_ONE, GL_ONE): alpha channel unused
	fragColor = float4(color, 0.0);
}

};
fragment float4 BeamFlareFragmentMain(BeamFlareVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));BeamFlareFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.texCoords=input.texCoords;p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.flareTex=image;p.run();return p.fragColor;}
struct BeamGlowVarying {float4 position [[position]];float2 localPos [[RECOIL_SAMPLE_PERSPECTIVE]];float3 glowColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float glowHalfWidth [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct BeamGlowVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 position_xy_uv;float4 startPosAndWidth;float4 endPosAndLife;float4 coreColor;float4 edgeColor;float2 localPos;float3 glowColor;float alpha;float glowHalfWidth;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__










void run()
{
	float3 startPos = startPosAndWidth.xyz;
	float beamWidth = startPosAndWidth.w;
	float3 endPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	float3 beamDir = endPos - startPos;
	float beamLength = length(beamDir);
	if (beamLength < 0.01) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	float3 forward = beamDir / beamLength;

	float3 camPos = cameraViewInv[3].xyz;
	float3 toCamera = normalize(camPos - mix(startPos, endPos, 0.5));
	float3 right = cross(forward, toCamera);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fallback = normalize(cross(forward, float3(0.0, 1.0, 0.0)));
		if (length(fallback) < 0.001) {
			fallback = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
		}
		float blend = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fallback, right / max(rightLen, 0.001), blend));
	} else {
		right = right / rightLen;
	}

	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { gl_Position = float4(2.0, 2.0, 2.0, 1.0); return; }

	float phase = startPos.x * 0.7 + startPos.z * 1.1 + lifeFrac * 13.0;
	float yNorm = position_xy_uv.y * 0.5 + 0.5;
	float shimmer = 1.0 + SHIMMER_AMPLITUDE * sin(timeInfo.z * SHIMMER_SPEED + phase + yNorm * 6.28);

	float glowWorldWidth = beamWidth * GLOW_WIDTH_MULT * lifePulse * shimmer;

	// No min-pixel-width clamping for glow: it is a soft additive halo that degrades
	// gracefully at distance. Inflating it at zoom-out made it disproportionately
	// large relative to the beam; removing inflation keeps proportions consistent.

	// Extend quad past beam endpoints by glowWorldWidth along forward direction
	// This creates the capsule-like rounded ends
	float extension = glowWorldWidth / max(beamLength, 0.01);
	float yExtended = mix(-extension, 1.0 + extension, yNorm);

	float3 vertPos = startPos + forward * (yExtended * beamLength)
		+ right * position_xy_uv.x * glowWorldWidth;

	gl_Position = cameraViewProj * float4(vertPos, 1.0);

	// Pass local coordinates: x = -1..1 across width, y = extended along length
	localPos = float2(position_xy_uv.x, yExtended);

	// Ratio of glow width to beam length (for capsule distance in FS)
	glowHalfWidth = glowWorldWidth / max(beamLength, 0.01);

	glowColor = edgeColor.rgb;

	// Scale glow brightness by beam thickness: thin beams get dimmer glow
	float glowScale = mix(float(GLOW_DIM_FACTOR), 1.0, smoothstep(float(GLOW_WIDTH_DIM), float(GLOW_WIDTH_FULL), beamWidth));

	float rangeFalloff = edgeColor.a;
	float yBeamClamped = clamp(yExtended, 0.0, 1.0);
	float alphaFalloff = 1.0 - rangeFalloff * yBeamClamped;
	alpha = coreColor.a * lifePulse * alphaFalloff * glowScale;
}

};
vertex BeamGlowVarying BeamGlowVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){BeamGlowVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.position_xy_uv=mesh[id];p.startPosAndWidth=instances[instance*5];p.endPosAndLife=instances[instance*5+1];p.coreColor=instances[instance*5+2];p.edgeColor=instances[instance*5+3];p.run();BeamGlowVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.localPos=p.localPos;out.glowColor=p.glowColor;out.alpha=p.alpha;out.glowHalfWidth=p.glowHalfWidth;return out;}
struct BeamGlowFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 localPos;float3 glowColor;float alpha;float glowHalfWidth;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	// Capsule-shaped distance: find closest point on beam axis (0..1 segment),
	// then compute normalized distance from that point
	float yOnBeam = clamp(localPos.y, 0.0, 1.0);
	float dy = localPos.y - yOnBeam;  // overshoot past beam ends
	float dx = localPos.x;            // -1..1 across width

	// Normalize both axes to glow radius units
	// dx is already -1..1 (= full glow width), dy needs scaling relative to width
	float dyNorm = dy / max(glowHalfWidth, 0.001);

	// 2D distance from beam axis (capsule shape)
	float distSq = dx * dx + dyNorm * dyNorm;
	if (distSq >= 1.0) discard_fragment();
	float dist = sqrt(distSq);

	// Radial falloff with configurable power curve
	float falloff = pow(1.0 - dist, GLOW_FALLOFF_POWER);

	float3 color = glowColor * (falloff * alpha * GLOW_BRIGHTNESS);

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.0003) discard_fragment();

	fragColor = float4(color, 0.0);
}

};
fragment float4 BeamGlowFragmentMain(BeamGlowVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));BeamGlowFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.localPos=input.localPos;p.glowColor=input.glowColor;p.alpha=input.alpha;p.glowHalfWidth=input.glowHalfWidth;p.run();return p.fragColor;}
struct BeamPulseVarying {float4 position [[position]];float yWorld [[RECOIL_SAMPLE_PERSPECTIVE]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float3 pulseColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float phase [[RECOIL_SAMPLE_PERSPECTIVE]];float beamLen [[RECOIL_SAMPLE_PERSPECTIVE]];float isParalyzer [[RECOIL_SAMPLE_PERSPECTIVE]];float coverage [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct BeamPulseVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 position_xy_uv;float4 startPosAndWidth;float4 endPosAndLife;float4 coreColor;float4 edgeColor;float4 flareData;float yWorld;float widthPos;float3 pulseColor;float alpha;float phase;float beamLen;float isParalyzer;float coverage;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__







           // y = isParalyzer flag (0 or 1)



void run()
{
	float3 startPos = startPosAndWidth.xyz;
	float beamWidth = startPosAndWidth.w;
	float3 endPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	float3 beamDir = endPos - startPos;
	float beamLength = length(beamDir);
	if (beamLength < 0.01) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}
	float3 forward = beamDir / beamLength;

	float3 camPos = cameraViewInv[3].xyz;
	float3 toCamera = normalize(camPos - mix(startPos, endPos, 0.5));
	float3 right = cross(forward, toCamera);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fallback = normalize(cross(forward, float3(0.0, 1.0, 0.0)));
		if (length(fallback) < 0.001) {
			fallback = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
		}
		float blend = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fallback, right / max(rightLen, 0.001), blend));
	} else {
		right = right / rightLen;
	}

	float yNorm = position_xy_uv.y * 0.5 + 0.5;

	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { gl_Position = float4(2.0, 2.0, 2.0, 1.0); return; }

	float paraFlag = flareData.y;  // 1.0 for paralyzer beams, 0.0 otherwise

	float pulseWidth = beamWidth * PULSE_WIDTH_MULT * lifePulse;

	float3 vertPos = mix(startPos, endPos, yNorm);
	// Use beam midpoint for camDist so min-pixel-width is uniform along beam
	float camDist = length(camPos - mix(startPos, endPos, 0.5));
	// Track the beam body's effective width so pulse stays proportional at all distances
	float baseWidth = beamWidth * lifePulse;
	float minWidth = camDist * MIN_PIXEL_WIDTH;
	float coverageVal = clamp(baseWidth / max(minWidth, 0.001), 0.0, 1.0);
	// Per-beam width multiplier: paralyzer uses PULSE_PARA_WIDTH_MULT
	float widthMult = mix(float(PULSE_WIDTH_MULT), float(PULSE_PARA_WIDTH_MULT), paraFlag);
	// Lerp width multiplier toward 0.3 at distance so pulse shrinks below beam width
	// when both are at sub-pixel sizes (avoids pulse dominating a thin beam)
	float effectiveMult = 0.3 + (widthMult - 0.3) * coverageVal;
	pulseWidth = max(baseWidth, minWidth) * effectiveMult;

	float3 vertexWorld = vertPos + right * position_xy_uv.x * pulseWidth;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	widthPos = position_xy_uv.x;
	yWorld = yNorm * beamLength;
	beamLen = beamLength;

	// Use edge (weapon) color for pulse to avoid color shift from CORE_COLOR_ADD
	pulseColor = edgeColor.rgb;

	// Per-beam unique phase derived from start position
	phase = startPos.x * 0.31 + startPos.y * 0.17 + startPos.z * 0.43;

	isParalyzer = paraFlag;

	coverage = coverageVal;

	float rangeFalloff = edgeColor.a;
	float alphaFalloff = 1.0 - rangeFalloff * yNorm;
	// coverage dims pulse at distance so it doesn't dominate a sub-pixel beam
	alpha = coreColor.a * lifePulse * lifePulse * alphaFalloff * coverageVal;
}

};
vertex BeamPulseVarying BeamPulseVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){BeamPulseVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.position_xy_uv=mesh[id];p.startPosAndWidth=instances[instance*5];p.endPosAndLife=instances[instance*5+1];p.coreColor=instances[instance*5+2];p.edgeColor=instances[instance*5+3];p.flareData=instances[instance*5+4];p.run();BeamPulseVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.yWorld=p.yWorld;out.widthPos=p.widthPos;out.pulseColor=p.pulseColor;out.alpha=p.alpha;out.phase=p.phase;out.beamLen=p.beamLen;out.isParalyzer=p.isParalyzer;out.coverage=p.coverage;return out;}
struct BeamPulseFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float yWorld;float widthPos;float3 pulseColor;float alpha;float phase;float beamLen;float isParalyzer;float coverage;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	// Select pulse parameters: paralyzer beams get faster, brighter, tighter pulses
	// float() casts needed because Lua tostring() strips ".0" from whole numbers,
	// making #defines integer literals which break mix() overload resolution.
	float pulseSpeed   = mix(float(PULSE_SPEED),      float(PULSE_PARA_SPEED),      isParalyzer);
	float pulseSpacing = mix(float(PULSE_SPACING),     float(PULSE_PARA_SPACING),    isParalyzer);
	float pulseSigma   = mix(float(PULSE_SIGMA),       float(PULSE_PARA_SIGMA),      isParalyzer);
	float pulseBright  = mix(float(PULSE_BRIGHTNESS),  float(PULSE_PARA_BRIGHTNESS), isParalyzer);

	// Radial falloff across beam width
	float edgeDist = abs(widthPos);
	float radial = 1.0 - smoothstep(PULSE_CORE_FRAC, 1.0, edgeDist);
	if (radial < 0.001) discard_fragment();

	// World-space distance to nearest pulse center (repeating pattern)
	float scrolledY = yWorld - timeInfo.z * pulseSpeed - phase * 100.0;
	float halfSpacing = pulseSpacing * 0.5;

	// Per-pulse random offset: hash the pulse index for organic irregularity
	float pulseIndex = floor(scrolledY / pulseSpacing + 0.5);
	float jitter = fract(sin(pulseIndex * 127.1 + phase * 311.7) * 43758.5453) - 0.5;
	float jitteredY = scrolledY + jitter * pulseSpacing * 0.3;

	float distToPulse = abs(mod(jitteredY + halfSpacing, pulseSpacing) - halfSpacing);

	// Gaussian falloff in world units
	float invSigmaSq = 1.0 / (2.0 * pulseSigma * pulseSigma);
	float pulseVal = exp(-distToPulse * distToPulse * invSigmaSq);

	// Fade at beam tip so pulses disappear smoothly (no fade at start — flare covers origin)
	float fadeDist = pulseSigma * 2.0;
	float edgeFade = 1.0 - smoothstep(beamLen - fadeDist, beamLen, yWorld);

	float3 color = pulseColor * (pulseVal * radial * edgeFade * alpha * pulseBright);

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.0005) discard_fragment();

	fragColor = float4(color, 0.0);
}

};
fragment float4 BeamPulseFragmentMain(BeamPulseVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));BeamPulseFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.yWorld=input.yWorld;p.widthPos=input.widthPos;p.pulseColor=input.pulseColor;p.alpha=input.alpha;p.phase=input.phase;p.beamLen=input.beamLen;p.isParalyzer=input.isParalyzer;p.coverage=input.coverage;p.run();return p.fragColor;}
struct PlasmaVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];float noiseSeed [[RECOIL_SAMPLE_PERSPECTIVE]];float2 swirlCosSin [[flat]];};
struct PlasmaVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 quadVertex;float4 posAndSize;float4 coreColor;float4 edgeColorAndSeed;float4 velocity;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float noiseSeed;float2 swirlCosSin;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Quad vertex: xy = corner position (-1..1), z = 1 on the second (crossed) plasma plane


// Per-instance data
       // xyz = sim frame position, w = cross-section size
        // rgb = core color, a = alpha
 // rgb = edge color, a = noise seed
         // xyz = velocity (elmos/frame)

// Drawn position between sim frames, flight direction (normalized) and speed (elmos/frame)
void plasmaMotion(thread float3& worldPos, thread float3& velDir, thread float& speed)
{
	float3 vel = velocity.xyz;
	speed = length(vel);
	worldPos = posAndSize.xyz;
	velDir = float3(0.0, 1.0, 0.0);
	if (speed > 0.001) {
		velDir = vel / speed;
		worldPos += vel * (timeInfo.w - 1.0);
	}
}




void run()
{
	float size = posAndSize.w;

	if (size <= 0.0) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	float3 worldPos, velDir;
	float speed;
	plasmaMotion(worldPos, velDir, speed);

	// Fixed world-derived perpendicular axis (does not rotate with camera).
	// The second plane uses the other perpendicular — together they form a
	// stable cross shape visible from all angles.
	float3 axis1 = cross(velDir, float3(0.0, 1.0, 0.0));
	float axis1Len = length(axis1);
	if (axis1Len < 0.001) {
		axis1 = normalize(cross(velDir, float3(1.0, 0.0, 0.0)));
	} else {
		axis1 = axis1 / axis1Len;
	}
	float3 axis = (quadVertex.z > 0.5) ? cross(axis1, velDir) : axis1;

	// Vertex x: across width (-1..1), vertex y: along velocity (-1..1)
	// Elongate along velocity direction, scaled by speed
	float speedFrac = clamp(speed / float(ELONGATION_SPEED_REF), 0.0, 1.0);
	float elongation = mix(float(ELONGATION_MIN), float(ELONGATION_MAX), speedFrac);
	float halfWidth  = size;
	float halfLength = size * elongation;

	// No center shift here — the FS handles TRAIL_SHIFT in UV space.
	// The quad must be large enough to contain the shifted shape at any TRAIL_SHIFT.
	// We expand the quad along velocity to guarantee no clipping.
	float paddedHalfLength = halfLength * (1.0 + abs(TRAIL_SHIFT));

	float3 vertexWorld = worldPos
		+ axis   * quadVertex.x * halfWidth
		+ velDir * quadVertex.y * paddedHalfLength;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	// UV: 0..1
	texCoords = quadVertex.xy * 0.5 + 0.5;
	vCoreColor = coreColor;
	vEdgeColor = edgeColorAndSeed;
	noiseSeed = edgeColorAndSeed.a;

	float swirlAngle = timeInfo.z * SWIRL_SPEED + noiseSeed * 6.28;
	swirlCosSin = float2(cos(swirlAngle), sin(swirlAngle));
}

};
vertex PlasmaVarying PlasmaVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){PlasmaVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.quadVertex=mesh[id];p.posAndSize=instances[instance*4];p.coreColor=instances[instance*4+1];p.edgeColorAndSeed=instances[instance*4+2];p.velocity=instances[instance*4+3];p.run();PlasmaVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;out.noiseSeed=p.noiseSeed;out.swirlCosSin=p.swirlCosSin;return out;}
struct PlasmaFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float noiseSeed;float2 swirlCosSin;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





// Hash-based gradient noise (no texture lookup needed)
float3 hash33(float3 p) {
	p = float3(dot(p, float3(127.1, 311.7, 74.7)),
			  dot(p, float3(269.5, 183.3, 246.1)),
			  dot(p, float3(113.5, 271.9, 124.6)));
	return fract(sin(p) * 43758.5453123) * 2.0 - 1.0;
}

float noise3D(float3 p) {
	float3 i = floor(p);
	float3 f = fract(p);
	float3 u = f * f * (3.0 - 2.0 * f);

	return mix(mix(mix(dot(hash33(i + float3(0,0,0)), f - float3(0,0,0)),
					   dot(hash33(i + float3(1,0,0)), f - float3(1,0,0)), u.x),
				   mix(dot(hash33(i + float3(0,1,0)), f - float3(0,1,0)),
					   dot(hash33(i + float3(1,1,0)), f - float3(1,1,0)), u.x), u.y),
			   mix(mix(dot(hash33(i + float3(0,0,1)), f - float3(0,0,1)),
					   dot(hash33(i + float3(1,0,1)), f - float3(1,0,1)), u.x),
				   mix(dot(hash33(i + float3(0,1,1)), f - float3(0,1,1)),
					   dot(hash33(i + float3(1,1,1)), f - float3(1,1,1)), u.x), u.y), u.z);
}

// Fractal Brownian Motion - 2 octaves for performance
float fbm(float3 p) {
	return 0.5 * noise3D(p) + 0.225 * noise3D(p * 2.2);
}

void run()
{
	// Centered UV: -1..1 (x = across width, y = along velocity)
	float2 centered = texCoords * 2.0 - 1.0;
	// Scale Y to match the padded quad (VS expands by 1+TRAIL_SHIFT)
	centered.y *= (1.0 + abs(TRAIL_SHIFT));
	float dx = centered.x;
	float dy = centered.y;

	// Trail shape: front is a tight ball, back stretches into a tail
	float yShifted = dy + TRAIL_SHIFT;
	float backStretch = 1.0 + max(0.0, yShifted) * TRAIL_FALLOFF;
	float frontTighten = 1.0 + max(0.0, -yShifted) * 0.3;
	float dist = length(float2(dx * frontTighten, yShifted * backStretch));

	if (dist > 1.6) discard_fragment();

	float time = timeInfo.z;
	float seed = noiseSeed;

	// Swirl: rotate UV around center over time for spinning energy look (angle per instance, from the VS)
	float cosA = swirlCosSin.x;
	float sinA = swirlCosSin.y;
	float2 swirled = float2(
		centered.x * cosA - centered.y * sinA,
		centered.x * sinA + centered.y * cosA
	) * SWIRL_STRENGTH + centered * (1.0 - SWIRL_STRENGTH);

	// Domain warping: use one noise to offset the input of another for organic energy look
	float3 noisePos1 = float3(swirled * NOISE_SCALE, time * NOISE_SPEED + seed * 100.0);
	float warpX = noise3D(noisePos1 + float3(5.2, 1.3, 0.0));
	float warpY = noise3D(noisePos1 + float3(1.7, 9.2, 0.0));
	float3 noisePos = noisePos1 + float3(warpX, warpY, 0.0) * 0.5;
	float displacement = fbm(noisePos);

	// Displace the radial distance - creates blobby, shifting edges
	float noisedDist = dist + displacement * NOISE_STRENGTH;

	// Outer edge mask - noise makes it blobby (tested before the tendril noise, which it does not need)
	float outerEdge = 1.0 - smoothstep(0.5 - EDGE_SOFTNESS, 0.5 + EDGE_SOFTNESS, noisedDist);
	if (outerEdge < 0.001) discard_fragment();

	// Additional high-freq noise layer for energy tendrils inside the blob
	float tendrilNoise = noise3D(noisePos * 3.5 + float3(0.0, 0.0, time * NOISE_SPEED * 1.3));
	float tendrils = smoothstep(0.1, 0.5, abs(tendrilNoise)) * 0.4;

	// Trail fade: back of the blob fades out
	float trailFade = 1.0 - smoothstep(0.0, 1.0, max(0.0, yShifted) * TRAIL_FALLOFF * 0.7);
	outerEdge *= trailFade;

	// Core factor: bright center with noise-modulated boundary
	float coreFactor = 1.0 - smoothstep(CORE_EDGE_START, CORE_EDGE_END, noisedDist);

	// Tendrils brighten the inside and create visible energy structure
	coreFactor = clamp(coreFactor + tendrils * (1.0 - dist * 0.8), 0.0, 1.0);

	// Blend core and edge colors
	float3 plasmaCol = mix(vEdgeColor.rgb, vCoreColor.rgb, coreFactor);

	// Alpha from outer edge mask
	float alpha = vCoreColor.a * outerEdge;

	float3 color = plasmaCol * alpha * BRIGHTNESS_MULT;

	// Core brightness boost - hot center
	color *= (1.0 + coreFactor * coreFactor * CORE_BRIGHTNESS);

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();

	// Additive blending output
	fragColor = float4(color, 0.0);
}

};
fragment float4 PlasmaFragmentMain(PlasmaVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));PlasmaFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.texCoords=input.texCoords;p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.noiseSeed=input.noiseSeed;p.swirlCosSin=input.swirlCosSin;p.run();return p.fragColor;}
struct PlasmaCrossSectionVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float4 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];float noiseSeed [[RECOIL_SAMPLE_PERSPECTIVE]];float headOnFactor [[RECOIL_SAMPLE_PERSPECTIVE]];float2 swirlCosSin [[flat]];};
struct PlasmaCrossSectionVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 quadVertex;float4 posAndSize;float4 coreColor;float4 edgeColorAndSeed;float4 velocity;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float noiseSeed;float headOnFactor;float2 swirlCosSin;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Quad vertex: xy = corner position (-1..1), z = 1 on the second (crossed) plasma plane


// Per-instance data
       // xyz = sim frame position, w = cross-section size
        // rgb = core color, a = alpha
 // rgb = edge color, a = noise seed
         // xyz = velocity (elmos/frame)

// Drawn position between sim frames, flight direction (normalized) and speed (elmos/frame)
void plasmaMotion(thread float3& worldPos, thread float3& velDir, thread float& speed)
{
	float3 vel = velocity.xyz;
	speed = length(vel);
	worldPos = posAndSize.xyz;
	velDir = float3(0.0, 1.0, 0.0);
	if (speed > 0.001) {
		velDir = vel / speed;
		worldPos += vel * (timeInfo.w - 1.0);
	}
}




void run()
{
	float size = posAndSize.w;

	if (size <= 0.0) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	float3 worldPos, velDir;
	float speed;
	plasmaMotion(worldPos, velDir, speed);

	// How head-on is the camera view? (1 = looking along velocity, 0 = side view)
	float3 camPos = cameraViewInv[3].xyz;
	float3 toCamera = normalize(camPos - worldPos);
	float headOn = abs(dot(velDir, toCamera));

	// Only visible when looking along velocity; fade out from side view
	if (headOn < 0.3) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	// Camera-facing billboard, circular (same size in both axes)
	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;

	float3 vertexWorld = worldPos
		+ camRight * quadVertex.x * size
		+ camUp    * quadVertex.y * size;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	texCoords = quadVertex.xy * 0.5 + 0.5;
	vCoreColor = coreColor;
	vEdgeColor = edgeColorAndSeed;
	noiseSeed = edgeColorAndSeed.a;
	headOnFactor = smoothstep(0.3, 0.7, headOn);

	float swirlAngle = timeInfo.z * SWIRL_SPEED + noiseSeed * 6.28;
	swirlCosSin = float2(cos(swirlAngle), sin(swirlAngle));
}

};
vertex PlasmaCrossSectionVarying PlasmaCrossSectionVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){PlasmaCrossSectionVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.quadVertex=mesh[id];p.posAndSize=instances[instance*4];p.coreColor=instances[instance*4+1];p.edgeColorAndSeed=instances[instance*4+2];p.velocity=instances[instance*4+3];p.run();PlasmaCrossSectionVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;out.noiseSeed=p.noiseSeed;out.headOnFactor=p.headOnFactor;out.swirlCosSin=p.swirlCosSin;return out;}
struct PlasmaCrossSectionFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 texCoords;float4 vCoreColor;float4 vEdgeColor;float noiseSeed;float headOnFactor;float2 swirlCosSin;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





float3 hash33(float3 p) {
	p = float3(dot(p, float3(127.1, 311.7, 74.7)),
			  dot(p, float3(269.5, 183.3, 246.1)),
			  dot(p, float3(113.5, 271.9, 124.6)));
	return fract(sin(p) * 43758.5453123) * 2.0 - 1.0;
}

float noise3D(float3 p) {
	float3 i = floor(p);
	float3 f = fract(p);
	float3 u = f * f * (3.0 - 2.0 * f);

	return mix(mix(mix(dot(hash33(i + float3(0,0,0)), f - float3(0,0,0)),
					   dot(hash33(i + float3(1,0,0)), f - float3(1,0,0)), u.x),
				   mix(dot(hash33(i + float3(0,1,0)), f - float3(0,1,0)),
					   dot(hash33(i + float3(1,1,0)), f - float3(1,1,0)), u.x), u.y),
			   mix(mix(dot(hash33(i + float3(0,0,1)), f - float3(0,0,1)),
					   dot(hash33(i + float3(1,0,1)), f - float3(1,0,1)), u.x),
				   mix(dot(hash33(i + float3(0,1,1)), f - float3(0,1,1)),
					   dot(hash33(i + float3(1,1,1)), f - float3(1,1,1)), u.x), u.y), u.z);
}

void run()
{
	float2 centered = texCoords * 2.0 - 1.0;
	float dist = length(centered);

	if (dist > 1.0) discard_fragment();

	float time = timeInfo.z;
	float seed = noiseSeed;

	// Swirling noise for blobby circular shape (angle per instance, from the VS)
	float cosA = swirlCosSin.x;
	float sinA = swirlCosSin.y;
	float2 swirled = float2(
		centered.x * cosA - centered.y * sinA,
		centered.x * sinA + centered.y * cosA
	);

	float3 noisePos = float3(swirled * NOISE_SCALE, time * NOISE_SPEED + seed * 100.0);
	float displacement = noise3D(noisePos);
	float noisedDist = dist + displacement * NOISE_STRENGTH * 0.5;

	// Soft circular edge
	float outerEdge = 1.0 - smoothstep(0.4, 0.7, noisedDist);
	if (outerEdge < 0.001) discard_fragment();

	// Core factor
	float coreFactor = 1.0 - smoothstep(CORE_EDGE_START, CORE_EDGE_END, noisedDist);

	float3 plasmaCol = mix(vEdgeColor.rgb, vCoreColor.rgb, coreFactor);

	float alpha = vCoreColor.a * outerEdge * headOnFactor * CROSS_SECTION_BRIGHTNESS;
	float3 color = plasmaCol * alpha;
	color *= (1.0 + coreFactor * coreFactor * CORE_BRIGHTNESS);

	float lum = dot(color, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();

	fragColor = float4(color, 0.0);
}

};
fragment float4 PlasmaCrossSectionFragmentMain(PlasmaCrossSectionVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));PlasmaCrossSectionFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.texCoords=input.texCoords;p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.noiseSeed=input.noiseSeed;p.headOnFactor=input.headOnFactor;p.swirlCosSin=input.swirlCosSin;p.run();return p.fragColor;}
struct PlasmaGlowVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float3 glowColor [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct PlasmaGlowVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float4 quadVertex;float4 posAndSize;float4 coreColor;float4 edgeColorAndSeed;float4 velocity;float2 texCoords;float3 glowColor;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Quad vertex: xy = corner position (-1..1), z = 1 on the second (crossed) plasma plane


// Per-instance data
       // xyz = sim frame position, w = cross-section size
        // rgb = core color, a = alpha
 // rgb = edge color, a = noise seed
         // xyz = velocity (elmos/frame)

// Drawn position between sim frames, flight direction (normalized) and speed (elmos/frame)
void plasmaMotion(thread float3& worldPos, thread float3& velDir, thread float& speed)
{
	float3 vel = velocity.xyz;
	speed = length(vel);
	worldPos = posAndSize.xyz;
	velDir = float3(0.0, 1.0, 0.0);
	if (speed > 0.001) {
		velDir = vel / speed;
		worldPos += vel * (timeInfo.w - 1.0);
	}
}




void run()
{
	float size = posAndSize.w;

	if (size <= 0.0) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		return;
	}

	float3 worldPos, velDir;
	float speed;
	plasmaMotion(worldPos, velDir, speed);

	// Offset glow center to match the visual bright center of the plasma shape,
	// which is shifted backward along velocity by TRAIL_SHIFT in UV space.
	float speedFrac = clamp(speed / float(ELONGATION_SPEED_REF), 0.0, 1.0);
	float elongation = mix(float(ELONGATION_MIN), float(ELONGATION_MAX), speedFrac);
	float3 glowCenter = worldPos - velDir * (size * elongation * float(TRAIL_SHIFT));

	float glowSize = size * GLOW_SIZE_MULT;

	// Camera-facing billboard
	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;

	float3 vertexWorld = glowCenter
		+ camRight * quadVertex.x * glowSize
		+ camUp    * quadVertex.y * glowSize;

	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);
	texCoords = quadVertex.xy * 0.5 + 0.5;
	float glowBright = GLOW_BRIGHTNESS * clamp(size / GLOW_REF_SIZE, 0.0, 1.0);
	glowColor = edgeColorAndSeed.rgb * glowBright;
}

};
vertex PlasmaGlowVarying PlasmaGlowVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){PlasmaGlowVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.quadVertex=mesh[id];p.posAndSize=instances[instance*4];p.coreColor=instances[instance*4+1];p.edgeColorAndSeed=instances[instance*4+2];p.velocity=instances[instance*4+3];p.run();PlasmaGlowVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.glowColor=p.glowColor;return out;}
struct PlasmaGlowFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float BRIGHTNESS_MULT;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float CROSS_SECTION_BRIGHTNESS;float EDGE_SOFTNESS;float ELONGATION_MAX;float ELONGATION_MIN;float ELONGATION_SPEED_REF;float FADE_IN_END;float FADE_OUT_START;float GLOW_BRIGHTNESS;float GLOW_DIM_FACTOR;float GLOW_FALLOFF_POWER;float GLOW_REF_SIZE;float GLOW_SIZE_MULT;float GLOW_WIDTH_DIM;float GLOW_WIDTH_FULL;float GLOW_WIDTH_MULT;float MIN_PIXEL_WIDTH;float NOISE_SCALE;float NOISE_SPEED;float NOISE_STRENGTH;float PULSE_BRIGHTNESS;float PULSE_CORE_FRAC;float PULSE_PARA_BRIGHTNESS;float PULSE_PARA_SIGMA;float PULSE_PARA_SPACING;float PULSE_PARA_SPEED;float PULSE_PARA_WIDTH_MULT;float PULSE_SIGMA;float PULSE_SPACING;float PULSE_SPEED;float PULSE_WIDTH_MULT;float RANGE_TAPER;float SHIMMER_AMPLITUDE;float SHIMMER_SPEED;float SWIRL_SPEED;float SWIRL_STRENGTH;float TIP_FADE_START;float TRAIL_FALLOFF;float TRAIL_SHIFT;float2 texCoords;float3 glowColor;float4 fragColor;texture2d<float> glowTex;float4 sample(texture2d<float> t,float2 uv){constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear);return t.sample(s,uv);}





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__







void run()
{
	float4 texSample = sample(glowTex, texCoords);
	float3 col = glowColor * texSample.rgb;
	float a = texSample.a;

	if (a < 0.002) discard_fragment();

	fragColor = float4(col * a, 0.0);
}

};
fragment float4 PlasmaGlowFragmentMain(PlasmaGlowVarying input [[stage_in]],constant EffectUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));PlasmaGlowFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.BRIGHTNESS_MULT=u.parameters[0];p.CORE_BRIGHTNESS=u.parameters[1];p.CORE_EDGE_END=u.parameters[2];p.CORE_EDGE_START=u.parameters[3];p.CROSS_SECTION_BRIGHTNESS=u.parameters[4];p.EDGE_SOFTNESS=u.parameters[5];p.ELONGATION_MAX=u.parameters[6];p.ELONGATION_MIN=u.parameters[7];p.ELONGATION_SPEED_REF=u.parameters[8];p.FADE_IN_END=u.parameters[9];p.FADE_OUT_START=u.parameters[10];p.GLOW_BRIGHTNESS=u.parameters[11];p.GLOW_DIM_FACTOR=u.parameters[12];p.GLOW_FALLOFF_POWER=u.parameters[13];p.GLOW_REF_SIZE=u.parameters[14];p.GLOW_SIZE_MULT=u.parameters[15];p.GLOW_WIDTH_DIM=u.parameters[16];p.GLOW_WIDTH_FULL=u.parameters[17];p.GLOW_WIDTH_MULT=u.parameters[18];p.MIN_PIXEL_WIDTH=u.parameters[19];p.NOISE_SCALE=u.parameters[20];p.NOISE_SPEED=u.parameters[21];p.NOISE_STRENGTH=u.parameters[22];p.PULSE_BRIGHTNESS=u.parameters[23];p.PULSE_CORE_FRAC=u.parameters[24];p.PULSE_PARA_BRIGHTNESS=u.parameters[25];p.PULSE_PARA_SIGMA=u.parameters[26];p.PULSE_PARA_SPACING=u.parameters[27];p.PULSE_PARA_SPEED=u.parameters[28];p.PULSE_PARA_WIDTH_MULT=u.parameters[29];p.PULSE_SIGMA=u.parameters[30];p.PULSE_SPACING=u.parameters[31];p.PULSE_SPEED=u.parameters[32];p.PULSE_WIDTH_MULT=u.parameters[33];p.RANGE_TAPER=u.parameters[34];p.SHIMMER_AMPLITUDE=u.parameters[35];p.SHIMMER_SPEED=u.parameters[36];p.SWIRL_SPEED=u.parameters[37];p.SWIRL_STRENGTH=u.parameters[38];p.TIP_FADE_START=u.parameters[39];p.TRAIL_FALLOFF=u.parameters[40];p.TRAIL_SHIFT=u.parameters[41];p.texCoords=input.texCoords;p.glowColor=input.glowColor;p.glowTex=image;p.run();return p.fragColor;}
)MSL";
}
