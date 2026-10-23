/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Source-consuming Jammer translation; exact original source provenance is in SOURCE-INPUTS.json.
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr char NativeJammerPreviewShaderSource[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(// SPDX-License-Identifier: GPL-2.0-or-later
// Original Jammer bodies from pinned BAR31657; shared radar stages retain their MIT notices.
// Mechanically translated from pinned game31657 GLSL; no manually recreated coverage algorithm.
#include <metal_stdlib>
using namespace metal;

// The immutable draw snapshot specializes the actual resource contract.
// Original runtime guards and arithmetic remain unchanged. No placeholder bindings.
constant bool PreviewTexture0 [[function_constant(0)]];
constant bool PreviewTexture1 [[function_constant(1)]];
constant bool PreviewTexture2 [[function_constant(2)]];
constant bool PreviewTexture3 [[function_constant(3)]];
constant bool PreviewTexture4 [[function_constant(4)]];
constant bool PreviewTexture5 [[function_constant(5)]];
constant bool PreviewTexture6 [[function_constant(6)]];
constant bool PreviewStorage [[function_constant(7)]];

// The caller supplies the ACTUAL LuaShader-expanded defines before this source.
// No fallback style/configuration or model-depth setting is invented here.
#ifndef MODEL_DEPTH_TEST
#error Actual original Jammer define MODEL_DEPTH_TEST is required
#endif
#ifndef RADAR_CELL_SIZE
#error Actual original Jammer define RADAR_CELL_SIZE is required
#endif
#ifndef MAP_CELLS_X
#error Actual original Jammer define MAP_CELLS_X is required
#endif
#ifndef MAP_CELLS_Z
#error Actual original Jammer define MAP_CELLS_Z is required
#endif
#ifndef SHEET_COLOR
#error Actual original Jammer define SHEET_COLOR is required
#endif
#ifndef ALLIED_COLOR
#error Actual original Jammer define ALLIED_COLOR is required
#endif
#ifndef ALLIED_ALPHA
#error Actual original Jammer define ALLIED_ALPHA is required
#endif
#ifndef OUTLINE_COLOR
#error Actual original Jammer define OUTLINE_COLOR is required
#endif
#ifndef SHEET_ALPHA
#error Actual original Jammer define SHEET_ALPHA is required
#endif
#ifndef SHEET_OUTLINE_ALPHA
#error Actual original Jammer define SHEET_OUTLINE_ALPHA is required
#endif
#ifndef OUTLINE_GAP_ALPHA
#error Actual original Jammer define OUTLINE_GAP_ALPHA is required
#endif
#ifndef OUTLINE_WIDTH
#error Actual original Jammer define OUTLINE_WIDTH is required
#endif
#ifndef OUTLINE_WORLD_WIDTH
#error Actual original Jammer define OUTLINE_WORLD_WIDTH is required
#endif
#ifndef OUTLINE_THIN_START
#error Actual original Jammer define OUTLINE_THIN_START is required
#endif
#ifndef OUTLINE_THIN_END
#error Actual original Jammer define OUTLINE_THIN_END is required
#endif
#ifndef OUTLINE_THIN_SCALE
#error Actual original Jammer define OUTLINE_THIN_SCALE is required
#endif
#ifndef OUTLINE_MIN_WIDTH
#error Actual original Jammer define OUTLINE_MIN_WIDTH is required
#endif
#ifndef OFFMAP_OUTLINE_FADE
#error Actual original Jammer define OFFMAP_OUTLINE_FADE is required
#endif
#ifndef OUTLINE_DASHES
#error Actual original Jammer define OUTLINE_DASHES is required
#endif
#ifndef MINIMAP_ALPHA
#error Actual original Jammer define MINIMAP_ALPHA is required
#endif
#ifndef MINIMAP_OUTLINE_ALPHA
#error Actual original Jammer define MINIMAP_OUTLINE_ALPHA is required
#endif
#ifndef MINIMAP_ALLIED_OUTLINE_ALPHA
#error Actual original Jammer define MINIMAP_ALLIED_OUTLINE_ALPHA is required
#endif
#ifndef MINIMAP_OUTLINE_WIDTH
#error Actual original Jammer define MINIMAP_OUTLINE_WIDTH is required
#endif
#ifndef INACTIVE_ALPHA
#error Actual original Jammer define INACTIVE_ALPHA is required
#endif
#ifndef INACTIVE_OUTLINE_ALPHA
#error Actual original Jammer define INACTIVE_OUTLINE_ALPHA is required
#endif
#ifndef MIN_COVERAGE
#error Actual original Jammer define MIN_COVERAGE is required
#endif
#ifndef SPAWN_SPEED
#error Actual original Jammer define SPAWN_SPEED is required
#endif
#ifndef MAX_ALLIED_CIRCLES
#error Actual original Jammer define MAX_ALLIED_CIRCLES is required
#endif
#ifndef CIRCLE_SDF_RANGE
#error Actual original Jammer define CIRCLE_SDF_RANGE is required
#endif
#ifndef CIRCLE_SDF_SLACK
#error Actual original Jammer define CIRCLE_SDF_SLACK is required
#endif
#ifndef PREVIEW_MAP_DEPTH_IS_DEPTH
#error Original map-depth native sampled type must be supplied
#endif
#if MODEL_DEPTH_TEST && !defined(PREVIEW_MODEL_DEPTH_IS_DEPTH)
#error Original model-depth native sampled type must be supplied
#endif
#if PREVIEW_MAP_DEPTH_IS_DEPTH
#define PreviewMapDepthTexture depth2d<float>
#else
#define PreviewMapDepthTexture texture2d<float>
#endif
#if MODEL_DEPTH_TEST
#if PREVIEW_MODEL_DEPTH_IS_DEPTH
#define PreviewModelDepthTexture depth2d<float>
#else
#define PreviewModelDepthTexture texture2d<float>
#endif
#endif

// Standalone source uniforms only. The engine UBO0/UBO1 remain separate original raw publications.
struct PreviewUniforms {
 float4 passRect,mipParams,losParams,coverageFlags,alliedCircle,smoothParams;
 float4 radarcenter_range,lookupParams,animParams,circleParams,discParams,inactiveParams,mapParams;
 float4 fragmentTransform; // GL.xy = nativeFragment.xy * .xy + .zw; actual pass/viewport mapping
 uint4 imageOptions; // x: flipV bits; y: original zero-to-one clip control; z: alpha compare|enabled<<8; w: reference float bits
 float4 previewParams,jammerFlags,emitterAllied;
 float jammerCircles[192];
};
struct PreviewVarying {
 float4 position [[position]];
 float2 minimapUV [[user(locn0)]];
 float clipDistance [[clip_distance]] [4];
};
struct PreviewFragmentInput {
 float4 position [[position]];
 float2 minimapUV [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
};
inline int2 PreviewSize(texture2d<float> image) {return int2(image.get_width(),image.get_height());}
inline float4 PreviewFetch(texture2d<float> image,int2 p,uint slot,constant PreviewUniforms& u) {
 if(u.imageOptions.x & (1u<<slot)) p.y=int(image.get_height())-1-p.y;
 return image.read(uint2(p),0);
}
inline float4 PreviewSample(texture2d<float> image,sampler imageSampler,float2 uv,uint slot,constant PreviewUniforms& u) {
 if(u.imageOptions.x & (1u<<slot))uv.y=1.0f-uv.y;
 return image.sample(imageSampler,uv);
}
inline float4 PreviewSample(depth2d<float> image,sampler imageSampler,float2 uv,uint slot,constant PreviewUniforms& u) {
 if(u.imageOptions.x & (1u<<slot))uv.y=1.0f-uv.y;
 return float4(image.sample(imageSampler,uv),0.0f,0.0f,1.0f);
}
inline bool2 lessThan(int2 a,int2 b){return a<b;}
inline bool2 lessThan(float2 a,float2 b){return a<b;}
inline bool2 greaterThanEqual(int2 a,int2 b){return a>=b;}
inline bool2 greaterThan(int2 a,int2 b){return a>b;}
inline bool2 greaterThan(float2 a,float2 b){return a>b;}
inline bool3 greaterThan(float3 a,float3 b){return a>b;}
// Compatibility alpha test follows the original fragment result and precedes
// attachment/stencil writes. Source shader calculations/discards stay intact.
inline bool PreviewAlphaPass(float alpha,constant PreviewUniforms& u) {
 const uint packed=u.imageOptions.z;
 if((packed&256u)==0u)return true;
 const float reference=as_type<float>(u.imageOptions.w);
 switch(packed&255u){
  case 0u:return false;
  case 1u:return alpha<reference;
  case 2u:return alpha==reference;
  case 3u:return alpha<=reference;
  case 4u:return alpha>reference;
  case 5u:return alpha!=reference;
  case 6u:return alpha>=reference;
  default:return true;
 }
}
vertex PreviewVarying OriginalPreviewPassVertex(uint vertexIndex [[vertex_id]], const device float4* positions [[buffer(2)]], constant PreviewUniforms& u [[buffer(3)]]) {
 PreviewVarying out;out.minimapUV=float2(0.0);const float4 pos=positions[vertexIndex];
	out.position = float4(mix(u.passRect.xy, u.passRect.zw, pos.xy * 0.5 + 0.5), 0.0, 1.0);
	// Offscreen passes and the projected sheet do not use the engine's fixed-function clip planes.
	out.clipDistance[0] = 1.0;
	out.clipDistance[1] = 1.0;
	out.clipDistance[2] = 1.0;
	out.clipDistance[3] = 1.0;

 if(u.imageOptions.y==0u)out.position.z=(out.position.z+out.position.w)*0.5f;
 return out;
}
vertex PreviewVarying OriginalPreviewMinimapVertex(uint vertexIndex [[vertex_id]], const device float4* positions [[buffer(2)]], constant PreviewUniforms& u [[buffer(3)]]) {
 PreviewVarying out;out.minimapUV=float2(0.0);const float4 pos=positions[vertexIndex];
	out.position = float4(pos.xy, 0.0, 1.0);
	out.minimapUV = pos.xy * 0.5 + 0.5;
	// The engine enables four user clip planes around the Lua minimap draw (CMiniMap::SetClipPlanes), set up
	// for its fixed-function matrices. A custom vertex shader that leaves them alone gets clipped against
	// out.position instead, which cuts the quad down to one quadrant, so declare every vertex inside.
	out.clipDistance[0] = 1.0;
	out.clipDistance[1] = 1.0;
	out.clipDistance[2] = 1.0;
	out.clipDistance[3] = 1.0;

 if(u.imageOptions.y==0u)out.position.z=(out.position.z+out.position.w)*0.5f;
 return out;
}

// Original LuaUI/Shaders/sensor_ranges_jammer_preview_disc.frag.glsl SHA512 bdb7558fd4762a3738f9877e00e469d82d184f150a27a2ac2cc2ff96080225b4c70ceed1e95a5603264de67756c2d54c11041d0af6795ff5c961ed070ef6bb60

// Jammer preview disc pass: the engine's jammer coverage of one emitter, one texel per radar cell
// (8 << radarMipLevel elmos). Terrain plays no part: CLosMap::AddCircle (rts/Sim/Misc/LosMap.cpp) fills a midpoint
// circle line by line around the emitter's cell, clipped to the map. Uncovered texels are discarded, so the target
// is cleared first and several emitters can be drawn into one map-sized target.
// Circle mode (SensorPreviewCircles) draws an allied jammer's circle instead: its distance field over the whole map,
// MAX-blended into the union of all of them.

//__DEFINES__

// emitter cell x, emitter cell z, radius in cells, 1 = the target is the whole map (texel = radar cell), 0 = the
// (2 * radius + 1)^2 disc around the emitter; or, with w = 2, circle center x, z and radius in elmos


// widest half line MidpointCircleAlgoPerLine fills at a row (0..radius) of a filled circle, -1 for none
inline int JammerDisc_halfWidth(int radius, int row, constant PreviewUniforms& u) {
	int x = radius;
	int y = 0;
	int decisionOver2 = 1 - radius;
	int width = -1;
	while (x >= y) {
		if (y == row) {
			width = max(width, x); // func(x, y)
		}
		if (decisionOver2 <= 0) {
			y++;
			decisionOver2 += 2 * y + 1;
		} else {
			if (x != y && x == row) {
				width = max(width, y); // func(y, x)
			}
			y++;
			x--;
			decisionOver2 += 2 * (y - x) + 1;
		}
	}
	return width;
}

fragment float4 OriginalPreviewJammerDiscFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	if (u.discParams.w > 1.5) {
		// elmos inside (+) or outside (-) the circle's edge at the texel's center, clamped and stored as 0..1
		float range = float(CIRCLE_SDF_RANGE);
		float sdf = clamp(u.discParams.z - distance(sourceFragment * float(RADAR_CELL_SIZE), u.discParams.xy), -range, range);
		fragColor = float4(sdf / (2.0 * range) + 0.5, 0.0, 0.0, 1.0);
		if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
	}
	int radius = int(u.discParams.z);
	int2 base = int2(u.discParams.xy);
	int2 texel = int2(sourceFragment);
	int2 off = texel - ((u.discParams.w > 0.5) ? base : int2(radius));
	int2 cell = base + off;

	bool onMap = all(greaterThanEqual(cell, int2(0))) && all(lessThan(cell, int2(MAP_CELLS_X, MAP_CELLS_Z)));
	int row = abs(off.y);
	if (!onMap || row > radius || abs(off.x) > JammerDisc_halfWidth(radius, row, u)) {
		discard_fragment();
	}
	fragColor = float4(1.0, 0.0, 0.0, 1.0);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original LuaUI/Shaders/sensor_ranges_radar_preview_smooth.frag.glsl SHA512 5f6fecc723655aaca4531e99cac9fb5b2bac735912bc441a4a890b12ec5995f1a256f449b25e3719e5d9a42274d657016273918916b65351528692c28ab8202e

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview smoothing pass. Eases the displayed coverage towards the freshly computed coverage
// every frame (ping-ponged between two textures), so the sheet fades smoothly while the radar
// is dragged around instead of popping in and out.
// The previous state is read with a texel shift, because when the emitter moves to another cell the
// same texel index refers to a different spot in the world.

//__DEFINES__




fragment float4 OriginalPreviewSmoothFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 texture2d<float> prevTex [[texture(0), function_constant(PreviewTexture0)]],
 texture2d<float> targetTex [[texture(1), function_constant(PreviewTexture1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	int2 texel = int2(sourceFragment);
	float target = PreviewFetch(targetTex,texel,1u,u).r;

	int2 prevTexel = texel + int2(u.smoothParams.xy);
	int2 prevSize = PreviewSize(prevTex);
	float prev = 0.0;
	if (u.smoothParams.w < 0.5 && all(greaterThanEqual(prevTexel, int2(0))) && all(lessThan(prevTexel, prevSize))) {
		prev = PreviewFetch(prevTex,prevTexel,0u,u).r;
	}

	fragColor = float4(mix(prev, target, u.smoothParams.z), 0.0, 0.0, 1.0);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original LuaUI/Shaders/sensor_ranges_jammer_preview.frag.glsl SHA512 b10db7ac9db0b8a58ade0bc399a361842d050e0ce2ecd780c4ad7b3e53003f0f7c6bbc26459e155cbf117dc31a59923ea9163b1ed89a6026762846a1e3a408b0

// Jammer preview, projected onto the terrain: drawn over the screen area of the coverage, each pixel rebuilds its
// world position from the map g-buffer depth and takes the coverage of its radar cell, outlined along the border
// with uncovered radar cells. Being the rendered terrain itself, it covers steep cliffs a sheet of per-cell quads
// cuts through. Over water it lies on the water surface; units and features in front of the terrain stay untinted.
// Past the map's edge there is no fill, only the outline carrying on a little and fading out.

//__DEFINES__

#if MODEL_DEPTH_TEST
#endif


// circle mode (SensorPreviewCircles): coverage as circles at the jammers' ranges instead of the engine's radar cells

//__ENGINEUNIFORMBUFFERDEFS__



constant float JammerSheet_cellSize = float(RADAR_CELL_SIZE);
constant float3 JammerSheet_sheetColor = SHEET_COLOR;
constant float3 JammerSheet_alliedColor = ALLIED_COLOR;
constant float3 JammerSheet_outlineColor = OUTLINE_COLOR;
constant float JammerSheet_sheetAlpha = float(SHEET_ALPHA);
constant float JammerSheet_sheetOutlineAlpha = float(SHEET_OUTLINE_ALPHA);
constant float JammerSheet_outlineGapAlpha = float(OUTLINE_GAP_ALPHA);
constant float JammerSheet_outlineWidth = float(OUTLINE_WIDTH); // pixels at 1080p, scaled with the vertical resolution
constant float JammerSheet_outlineWorldWidth = float(OUTLINE_WORLD_WIDTH); // elmos
constant float JammerSheet_outlineThinStart = float(OUTLINE_THIN_START); // elmos from the camera
constant float JammerSheet_outlineThinScale = float(OUTLINE_THIN_SCALE);
constant float JammerSheet_outlineMinWidth = float(OUTLINE_MIN_WIDTH); // screen pixels
constant float JammerSheet_outlineDashes = float(OUTLINE_DASHES); // per radar cell side
constant float JammerSheet_alliedAlpha = float(ALLIED_ALPHA);
constant float JammerSheet_inactiveAlpha = float(INACTIVE_ALPHA);
constant float JammerSheet_inactiveOutlineAlpha = float(INACTIVE_OUTLINE_ALPHA);
constant float JammerSheet_minCoverage = float(MIN_COVERAGE);
constant float JammerSheet_spawnSpeed = float(SPAWN_SPEED);
constant float JammerSheet_sdfRange = float(CIRCLE_SDF_RANGE);
constant float JammerSheet_sdfSlack = float(CIRCLE_SDF_SLACK); // elmos a mobile jammer's circle moves before the field is re-rendered
constant float JammerSheet_PI = 3.14159265;

// texel of the previewed jammer's disc textures for a radar cell, (-1, -1) outside its disc
inline int2 JammerSheet_discTexel(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	int radius = int(u.previewParams.z);
	int2 texel = cell - int2(u.previewParams.xy) + int2(radius);
	if (radius < 0 || any(lessThan(texel, int2(0))) || any(greaterThanEqual(texel, int2(2 * radius + 1)))) {
		return int2(-1);
	}
	return texel;
}

// smoothed coverage of the previewed jammer at a radar cell, 0 outside its disc
inline float JammerSheet_previewCoverageAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	int2 texel = JammerSheet_discTexel(cell, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
	return (texel.x < 0) ? 0.0 : PreviewFetch(coverageTex,texel,2u,u).r;
}

// exact engine coverage of the previewed jammer at a radar cell (0/1)
inline float JammerSheet_previewTargetAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	int2 texel = JammerSheet_discTexel(cell, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
	return (texel.x < 0) ? 0.0 : step(0.5, PreviewFetch(targetTex,texel,4u,u).r);
}

// 1 where an allied jammer covers the radar cell, 0 with allied coverage off
inline float JammerSheet_alliedAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	if (u.emitterAllied.zw.x < 0.5 || any(lessThan(cell, int2(0))) || any(greaterThanEqual(cell, PreviewSize(alliedTex)))) {
		return 0.0;
	}
	return step(0.5, PreviewFetch(alliedTex,cell,3u,u).r);
}

// Opacity of the stipple along a border at `coord` (radar cells), `rate` cells per pixel along it: OUTLINE_DASHES
// dashes per cell side, centred on the cell corners so the steps of the border stay readable. Anti-aliased, and
// blurred into an even line once the dashes shrink below a few pixels.
inline float JammerSheet_stipple(float coord, float rate, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	float periodsPerPixel = max(rate * JammerSheet_outlineDashes, 1e-4);
	float dashes = smoothstep(0.5 - periodsPerPixel, 0.5 + periodsPerPixel, abs(fract(coord * JammerSheet_outlineDashes) - 0.5) * 2.0);
	dashes = mix(dashes, 0.5, smoothstep(0.25, 0.5, periodsPerPixel));
	return mix(JammerSheet_outlineGapAlpha, 1.0, dashes);
}

// Stipples of the cell's near x and z side lines and of their corner, travelled `offset` cells clockwise around the
// coverage (x sides run along z, z sides along x). The pattern is symmetric around the corners, so it flows round them.
inline void JammerSheet_stipplesAt(float2 cellCoord, float2 nearSide, float2 pixelCells, float offset, thread float2& sides, thread float& corner, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	sides = float2(
		JammerSheet_stipple(cellCoord.y - (nearSide.x * 2.0 - 1.0) * offset, pixelCells.y, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams),
		JammerSheet_stipple(cellCoord.x + (nearSide.y * 2.0 - 1.0) * offset, pixelCells.x, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams));
	corner = JammerSheet_stipple(offset, max(pixelCells.x, pixelCells.y), u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
}

// A border's line in a cell: along its near x and z sides that face outside (`sides`), plus the square where those
// sides meet. That square belongs to both lines at a convex corner and to neither at a concave one (only the diagonal
// cell outside), so it is drawn once there, with the stipple of the corner itself.
inline float JammerSheet_borderLine(float2 sides, float diagonalOut, float2 edge, float2 stipples, float cornerStipple, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	float line = max(sides.x * edge.x * stipples.x, sides.y * edge.y * stipples.y);
	float corner = min(edge.x, edge.y) * max(sides.x * sides.y, (1.0 - sides.x) * (1.0 - sides.y) * diagonalOut);
	return mix(line, cornerStipple, corner);
}

// 1 for a radar cell past the map's edge, 0 on the map
inline float JammerSheet_cellOffMap(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	return float(any(lessThan(cell, int2(0))) || any(greaterThanEqual(cell, int2(MAP_CELLS_X, MAP_CELLS_Z))));
}

// An outline width (`pixel`: one screen pixel in its unit) thinned by `thin` (OUTLINE_THIN_SCALE once far from the
// camera), though not below OUTLINE_MIN_WIDTH pixels unless it was thinner to begin with
inline float2 JammerSheet_thinOutline(float2 width, float2 pixel, float thin, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	return max(width * thin, min(width, pixel * JammerSheet_outlineMinWidth));
}

// circle mode: elmos inside (+) or outside (-) the allied circles' union, clamped to CIRCLE_SDF_RANGE. Off the map
// less the distance to it, or its edge texels would smear outward (the circle list is exact there)
inline float JammerSheet_alliedCircleSdf(float2 w, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	if (u.emitterAllied.zw.x < 0.5) {
		return -JammerSheet_sdfRange;
	}
	float offMap = distance(w, clamp(w, float2(0.0), engineParams[3].xy)); // engineParams[3].xy: the map in elmos
	return (PreviewSample(alliedSdfTex,sampler5,w / engineParams[3].xy,5u,u).r - 0.5) * 2.0 * JammerSheet_sdfRange - offMap;
}

// circle mode: the stipple along a circle (center x, z, radius), travelled `offset` cells clockwise. A whole number of
// dash periods goes round it, so the pattern closes on itself.
inline float JammerSheet_arcStipple(float2 w, float3 circle, float offset, float rate, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	float periods = max(1.0, floor(2.0 * JammerSheet_PI * circle.z * JammerSheet_outlineDashes / JammerSheet_cellSize + 0.5));
	float turns = atan2(w.y - circle.y, w.x - circle.x) / (2.0 * JammerSheet_PI);
	return JammerSheet_stipple(turns * periods / JammerSheet_outlineDashes - offset, rate, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
}

// circle mode: elmos inside the allied circles from their list, exact where the distance field (one texel per radar
// cell) would bend their edges into polygons, and the circle whose edge runs nearest: the one w lies deepest inside
inline float JammerSheet_alliedExactSdf(float2 w, thread float3& owner, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	owner = float3(0.0, 0.0, -1.0);
	float best = -JammerSheet_sdfRange;
	int count = int(u.jammerFlags.w);
	for (int i = 0; i < count; i++) {
		float3 circle = float3(u.jammerCircles[i * 3], u.jammerCircles[i * 3 + 1], u.jammerCircles[i * 3 + 2]);
		float inside = circle.z - distance(w, circle.xy);
		if (inside > best) {
			best = inside;
			owner = circle;
		}
	}
	return best;
}

// circle mode: a line along the inside of an edge at signed distance `sdf`, `width` elmos wide, anti-aliased over
// `px` elmos on both sides
inline float JammerSheet_edgeBand(float sdf, float width, float px, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> alliedTex, texture2d<float> targetTex, texture2d<float> alliedSdfTex, sampler sampler5, constant float4* engineParams) {
	return smoothstep(-px, 0.0, sdf) * (1.0 - smoothstep(width - px, width, sdf));
}

fragment float4 OriginalPreviewJammerSheetFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 PreviewMapDepthTexture mapDepths [[texture(0), function_constant(PreviewTexture0)]]
#if MODEL_DEPTH_TEST
,
 PreviewModelDepthTexture modelDepths [[texture(1), function_constant(PreviewTexture1)]]
#endif
,
 texture2d<float> coverageTex [[texture(2), function_constant(PreviewTexture2)]],
 texture2d<float> alliedTex [[texture(3), function_constant(PreviewTexture3)]],
 texture2d<float> targetTex [[texture(4), function_constant(PreviewTexture4)]],
 texture2d<float> alliedSdfTex [[texture(5), function_constant(PreviewTexture5)]],
 sampler sampler0 [[sampler(0)]]
#if MODEL_DEPTH_TEST
 ,sampler sampler1 [[sampler(1)]]
#endif
,
 constant float4x4* engineMatrices [[buffer(0)]], constant float4* engineParams [[buffer(1)]], sampler sampler5 [[sampler(5), function_constant(PreviewTexture5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
const float JammerSheet_outlineThinEnd = max(float(OUTLINE_THIN_END), JammerSheet_outlineThinStart + 1.0);
const float JammerSheet_offMapOutlineFade = max(float(OFFMAP_OUTLINE_FADE), 1.0); // elmos past the map's edge

	float2 screenUV = (sourceFragment - engineParams[2].zw) / engineParams[2].xy;
	float mapDepth = PreviewSample(mapDepths,sampler0,screenUV,0u,u).x;
	float4 world = engineMatrices[9] * float4(screenUV * 2.0 - 1.0, mapDepth, 1.0);
	float3 worldPos = world.xyz / world.w;
	// over water the coverage lies on the water surface, where the view ray crosses y = 0
	float3 camPos = engineMatrices[7][3].xyz;
	if (worldPos.y < 0.0 && camPos.y > 0.0) {
		worldPos = mix(camPos, worldPos, camPos.y / (camPos.y - worldPos.y));
	}
	float2 cellCoord = worldPos.xz / JammerSheet_cellSize; // position in radar cells
	float2 cellPixels = fwidth(cellCoord); // radar cells per pixel, taken before any discard
	// radar cells per pixel across x and across z: the width of the borders running along z and x, and the stipple rate
	// of the ones running along x and z
	float2 xGrad = float2(dfdx(cellCoord.x), (-dfdy(cellCoord.x)));
	float2 zGrad = float2(dfdx(cellCoord.y), (-dfdy(cellCoord.y)));
	float2 pixelCells = max(float2(length(xGrad), length(zGrad)), float2(1e-5));

	if (mapDepth >= 0.999999) {
		discard_fragment(); // sky
	}

	int2 cell = int2(floor(cellCoord));
	float inactive = u.jammerFlags.x;
	bool circles = u.jammerFlags.z > 0.5;
	float2 w = worldPos.xz;
	float px = max(pixelCells.x, pixelCells.y) * JammerSheet_cellSize; // elmos per pixel
	// the outline's width factor: 1 up close, OUTLINE_THIN_SCALE from OUTLINE_THIN_END elmos from the camera on
	float thin = mix(1.0, JammerSheet_outlineThinScale, smoothstep(JammerSheet_outlineThinStart, JammerSheet_outlineThinEnd, distance(camPos, worldPos)));
	// past the map's edge there is no fill, and the outline fades out over OFFMAP_OUTLINE_FADE elmos
	float offMapDist = distance(w, clamp(w, float2(0.0), engineParams[3].xy)); // engineParams[3].xy: the map in elmos
	float mapFill = clamp(1.0 - offMapDist / px, 0.0, 1.0);
	float mapLine = 1.0 - clamp(offMapDist / JammerSheet_offMapOutlineFade, 0.0, 1.0);

	// The previewed jammer's fill weight (spawn ripple and fade-in included), its exact coverage for the outline (so a
	// moving jammer's old border doesn't linger) and the allied coverage: per radar cell, or per pixel inside the
	// circles. The spawn ripple spreads the previewed coverage outward from the emitter when the preview appears.
	float weight, ownIn, allied;
	float ownSdf = -JammerSheet_sdfRange; // circle mode: elmos inside the previewed jammer's circle
	float alliedSdf = -JammerSheet_sdfRange; // circle mode: elmos inside the allied circles
	float3 alliedOwner = float3(0.0, 0.0, -1.0); // circle mode: the allied circle whose edge runs here, if known
	// circle mode: OUTLINE_WIDTH pixels plus OUTLINE_WORLD_WIDTH elmos along the inside of the circles' edges, thinned
	float bandWidth = px * JammerSheet_outlineWidth * engineParams[2].y / 1080.0 + JammerSheet_outlineWorldWidth;
	bandWidth = JammerSheet_thinOutline(float2(bandWidth), float2(px), thin, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams).x;
	if (circles) {
		float ownDist = distance(w, u.circleParams.xy);
		if (u.previewParams.z >= 0.0 && u.circleParams.z > 0.0) {
			ownSdf = u.circleParams.z - ownDist;
		}
		alliedSdf = JammerSheet_alliedCircleSdf(w, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		// exact near an edge (filtering bends the distance field by up to JammerSheet_cellSize / sqrt(2), most where circles cross,
		// and it lags moving circles by up to JammerSheet_sdfSlack) and off the map, where the field has no texels
		float nearEdge = bandWidth + 2.0 * px + 4.0 + 0.75 * JammerSheet_cellSize + JammerSheet_sdfSlack;
		if (u.emitterAllied.zw.x > 0.5 && u.jammerFlags.w > 0.5 && (abs(alliedSdf) < nearEdge || offMapDist > 0.0)) {
			alliedSdf = JammerSheet_alliedExactSdf(w, alliedOwner, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		}
		float distN = ownDist / max(u.circleParams.z, 1.0);
		float spawn = smoothstep(distN - 0.10, distN + 0.02, u.previewParams.w * JammerSheet_spawnSpeed);
		float inside = clamp(ownSdf / px + 0.5, 0.0, 1.0);
		weight = inside * spawn * u.circleParams.w;
		ownIn = inside * step(0.5, spawn);
		allied = clamp(alliedSdf / px + 0.5, 0.0, 1.0);
	} else {
		float distN = length((float2(cell) + 0.5) * JammerSheet_cellSize - u.emitterAllied.xy) / (max(u.previewParams.z, 1.0) * JammerSheet_cellSize);
		float spawn = smoothstep(distN - 0.10, distN + 0.02, u.previewParams.w * JammerSheet_spawnSpeed);
		weight = smoothstep(0.0, 0.5, JammerSheet_previewCoverageAt(cell, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams)) * spawn;
		ownIn = step(0.5, min(JammerSheet_previewTargetAt(cell, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), spawn));
		allied = JammerSheet_alliedAt(cell, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
	}

	// the previewed jammer's coverage blends in over the muted allied coverage. An inactive one jams nothing: it is a
	// faint hint where no allied jammer covers the spot, allied coverage shows as it is.
	float alliedFade = allied * u.emitterAllied.zw.y * JammerSheet_alliedAlpha;
	float fillFade = mix(mix(alliedFade, 1.0, weight), max(alliedFade, weight * JammerSheet_inactiveAlpha), inactive) * mapFill;
	float jammedFade = max(alliedFade, ownIn * weight * (1.0 - inactive)) * mapLine; // how strongly a jammer covers the spot
	float ownLineFade = ownIn * weight * mix(1.0, JammerSheet_inactiveOutlineAlpha, inactive) * mapLine;
	if (max(fillFade, max(jammedFade, ownLineFade)) < JammerSheet_minCoverage) {
		discard_fragment();
	}
#if MODEL_DEPTH_TEST
	if (PreviewSample(modelDepths,sampler1,screenUV,1u,u).x < mapDepth) {
		discard_fragment(); // a unit or feature in front of the terrain
	}
#endif

	// Stippled outline of the previewed jammer's coverage (drawn even inside allied coverage, faint and standing still
	// while inactive) and of everything jammed. None where the position jumps between pixels (terrain silhouettes).
	// The stipples and neighbour lookups only run where a line can be.
	float ownLine = 0.0;
	float coveredLine = 0.0;
	float outlineOn;
	if (circles) {
		outlineOn = (max(cellPixels.x, cellPixels.y) < 4.0) ? JammerSheet_sheetOutlineAlpha : 0.0;
		float rate = px / JammerSheet_cellSize;
		ownLine = JammerSheet_edgeBand(ownSdf, bandWidth, px, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		if (ownLine > 0.0 && outlineOn > 0.0) {
			ownLine *= JammerSheet_arcStipple(w, u.circleParams.xyz, u.jammerFlags.y * (1.0 - inactive), rate, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		}
		float ownJamSdf = (inactive < 0.5) ? ownSdf : -JammerSheet_sdfRange;
		coveredLine = JammerSheet_edgeBand(max(ownJamSdf, alliedSdf), bandWidth, px, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		if (coveredLine > 0.0 && outlineOn > 0.0) {
			// a solid line where the allied circle isn't known (more of them than MAX_ALLIED_CIRCLES)
			float3 owner = (ownJamSdf >= alliedSdf) ? u.circleParams.xyz : alliedOwner;
			coveredLine *= (owner.z > 0.0) ? JammerSheet_arcStipple(w, owner, u.jammerFlags.y, rate, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams) : 1.0;
		}
	} else {
		// nor once cells shrink below a pixel
		outlineOn = (max(cellPixels.x, cellPixels.y) < 1.0) ? JammerSheet_sheetOutlineAlpha : 0.0;
		float2 inCell = fract(cellCoord);
		float2 edgeDist = 0.5 - abs(inCell - 0.5); // distance to the nearer side, in cells
		// OUTLINE_WIDTH pixels plus OUTLINE_WORLD_WIDTH elmos, so it gets a little thicker when zoomed in, thinned far
		// from the camera; crisp, with a pixel of anti-aliasing on its inner edge
		float2 width = pixelCells * JammerSheet_outlineWidth * engineParams[2].y / 1080.0 + JammerSheet_outlineWorldWidth / JammerSheet_cellSize;
		width = JammerSheet_thinOutline(width, pixelCells, thin, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		if (outlineOn > 0.0 && any(lessThan(edgeDist, width))) {
			// on the nearer x and z side of the cell when it borders a cell the previewed jammer does not cover, or
			// that no jammer covers
			float2 nearSide = step(float2(0.5), inCell); // 0 = the -x/-z side is nearer, 1 = the +x/+z side
			int2 nx = cell + int2(int(nearSide.x) * 2 - 1, 0);
			int2 nz = cell + int2(0, int(nearSide.y) * 2 - 1);
			int2 nd = int2(nx.x, nz.y); // diagonally across the nearer corner
			// the map's edge is no border: cells past it count as covered
			float3 offMapNeighbours = float3(JammerSheet_cellOffMap(nx, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_cellOffMap(nz, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_cellOffMap(nd, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams));
			float3 ownNeighbours = max(float3(JammerSheet_previewTargetAt(nx, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_previewTargetAt(nz, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_previewTargetAt(nd, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams)), offMapNeighbours);
			float3 alliedNeighbours = max(float3(JammerSheet_alliedAt(nx, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_alliedAt(nz, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams), JammerSheet_alliedAt(nd, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams)), offMapNeighbours);
			// x side, z side and diagonal facing outside the previewed jammer's coverage, and outside all jammed cells
			float3 ownOut = ownIn * (1.0 - ownNeighbours);
			float3 coveredOut = 1.0 - max(ownNeighbours * (1.0 - inactive), alliedNeighbours);
			float2 edge = 1.0 - smoothstep(width - pixelCells, width, edgeDist);
			// corners sit in the middle of a dash; an inactive jammer's border stands still
			float2 stipples, ownStipples;
			float cornerStipple, ownCornerStipple;
			JammerSheet_stipplesAt(cellCoord, nearSide, pixelCells, u.jammerFlags.y, stipples, cornerStipple, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
			JammerSheet_stipplesAt(cellCoord, nearSide, pixelCells, u.jammerFlags.y * (1.0 - inactive), ownStipples, ownCornerStipple, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
			ownLine = JammerSheet_borderLine(ownOut.xy, ownOut.z, edge, ownStipples, ownCornerStipple, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
			coveredLine = JammerSheet_borderLine(coveredOut.xy, coveredOut.z, edge, stipples, cornerStipple, u, coverageTex, alliedTex, targetTex, alliedSdfTex, sampler5, engineParams);
		}
	}
	float lineAlpha = max(ownLine * ownLineFade, coveredLine * jammedFade) * outlineOn;

	// the outline over the fill
	float3 fillColor = mix(JammerSheet_alliedColor, JammerSheet_sheetColor, weight * (1.0 - inactive * allied));
	float fillAlpha = JammerSheet_sheetAlpha * fillFade * (1.0 - lineAlpha);
	float alpha = lineAlpha + fillAlpha;
	fragColor = float4((JammerSheet_outlineColor * lineAlpha + fillColor * fillAlpha) / max(alpha, 1e-4), alpha);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original LuaUI/Shaders/sensor_ranges_radar_preview_minimap.frag.glsl SHA512 bd13f21d2439d813fbf6add015c209f4edf52990e5be5501f52068181fc5a71a02ad61c973e902303b0ef479a55bd533b015f2d04065944924d7dbe2a9d44757

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview minimap pass: a flat fill of the radar coverage on the minimap (RadarPreviewMinimap setting),
// outlined along the border with uncovered radar cells like the sheet in the world.
// The fill fades with the same smoothed per-radar-cell coverage the sheet uses (_smooth.frag.glsl); the outline
// is taken from the exact engine coverage (_coverage.frag.glsl) instead, so it is the same at any instant.
// This matters for the engine minimap, whose texture is refreshed from Game::Update at 15-60 Hz and would
// otherwise freeze the smoothing pass mid-way, with cells easing in or out showing up as stray outlines.

//__DEFINES__

#ifndef COVERAGE_CORNER_RADIUS
#define COVERAGE_CORNER_RADIUS 0.0
#endif




constant float3 Minimap_sheetColor = SHEET_COLOR;
constant float3 Minimap_alliedColor = ALLIED_COLOR;
constant float3 Minimap_outlineColor = OUTLINE_COLOR;
constant float Minimap_alliedAlpha = float(ALLIED_ALPHA);
constant float Minimap_fillAlpha = float(MINIMAP_ALPHA);
constant float Minimap_outlineAlpha = float(MINIMAP_OUTLINE_ALPHA);
constant float Minimap_alliedOutlineAlpha = float(MINIMAP_ALLIED_OUTLINE_ALPHA);

// texel of the previewed radar's disc textures for a radar cell, or (-1, -1) outside the disc
inline int2 Minimap_discTexel(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> targetTex) {
	int radius = int(u.discParams.z);
	int2 texel = cell - int2(u.discParams.xy) + int2(radius);
	if (any(lessThan(texel, int2(0))) || any(greaterThanEqual(texel, int2(2 * radius + 1)))) {
		return int2(-1);
	}
	return texel;
}

// smoothed coverage of the previewed radar at a radar cell (0 outside its disc)
inline float Minimap_smoothedAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> targetTex) {
	int2 texel = Minimap_discTexel(cell, u, coverageTex, radarInfoTex, targetTex);
	return (texel.x < 0) ? 0.0 : PreviewFetch(coverageTex,texel,0u,u).r;
}

// exact engine coverage of the previewed radar at a radar cell (0 outside its disc)
inline float Minimap_ownAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> targetTex) {
	int2 texel = Minimap_discTexel(cell, u, coverageTex, radarInfoTex, targetTex);
	return (texel.x < 0) ? 0.0 : step(0.5, PreviewFetch(targetTex,texel,2u,u).r);
}

// 1 where another allied radar covers the radar cell
inline float Minimap_alliedAt(int2 cell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> targetTex) {
	if (u.discParams.w < 0.5 || any(lessThan(cell, int2(0))) || any(greaterThanEqual(cell, PreviewSize(radarInfoTex)))) {
		return 0.0;
	}
	return step(0.5, PreviewFetch(radarInfoTex,cell,1u,u).r);
}

inline float Minimap_roundedBoundary(float covered, float3 neighbours, float2 edgeDist, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> targetTex) {
	float radius = float(COVERAGE_CORNER_RADIUS);
	if (covered > 0.5) {
		float2 sideDist = float2(neighbours.x < 0.5 ? edgeDist.x : 1e6, neighbours.y < 0.5 ? edgeDist.y : 1e6);
		// The concave arc extends into the neighboring empty cell: its old straight edges
		// must also be replaced on this covered side of the cell boundary.
		if (neighbours.x < 0.5 && neighbours.y > 0.5 && neighbours.z > 0.5 && edgeDist.y < radius) {
			sideDist.x = length(float2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
		}
		if (neighbours.y < 0.5 && neighbours.x > 0.5 && neighbours.z > 0.5 && edgeDist.x < radius) {
			sideDist.y = length(float2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
		}
		float sdf = min(sideDist.x, sideDist.y);
		if (neighbours.x < 0.5 && neighbours.y < 0.5 && all(lessThan(edgeDist, float2(radius)))) {
			sdf = radius - length(edgeDist - radius);
		}
		return sdf;
	}
	float2 sideDist = float2(neighbours.x > 0.5 ? edgeDist.x : 1e6, neighbours.y > 0.5 ? edgeDist.y : 1e6);
	// Outside a convex corner, antialias against the arc rather than the removed square edge.
	if (neighbours.x > 0.5 && neighbours.z < 0.5 && edgeDist.y < radius) {
		sideDist.x = length(float2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
	}
	if (neighbours.y > 0.5 && neighbours.z < 0.5 && edgeDist.x < radius) {
		sideDist.y = length(float2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
	}
	float sdf = -min(sideDist.x, sideDist.y);
	if (neighbours.x < 0.5 && neighbours.y < 0.5 && neighbours.z > 0.5) {
		sdf = radius - length(edgeDist + radius);
	}
	if (all(greaterThan(neighbours, float3(0.5))) && all(lessThan(edgeDist, float2(radius)))) {
		sdf = length(edgeDist - radius) - radius;
	}
	return sdf;
}

fragment float4 OriginalPreviewMinimapFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 texture2d<float> coverageTex [[texture(0), function_constant(PreviewTexture0)]],
 texture2d<float> radarInfoTex [[texture(1), function_constant(PreviewTexture1)]],
 texture2d<float> targetTex [[texture(2), function_constant(PreviewTexture2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	// minimap (y up) -> normalized world x/z, the inverse of the world -> minimap mapping the other
	// GL4 minimap overlays use (gui_buildsquare_gl4, map_startpolygon_gl4)
	int rotation = int(u.mapParams.z);
	float2 world;
	if (rotation == 0) {
		world = float2(in.minimapUV.x, 1.0 - in.minimapUV.y);
	} else if (rotation == 1) {
		world = float2(in.minimapUV.y, in.minimapUV.x);
	} else if (rotation == 2) {
		world = float2(1.0 - in.minimapUV.x, in.minimapUV.y);
	} else {
		world = float2(1.0 - in.minimapUV.y, 1.0 - in.minimapUV.x);
	}
	float2 cellPos = world * u.mapParams.xy; // position in radar cells
	int2 cell = int2(floor(cellPos));

	float smoothed = Minimap_smoothedAt(cell, u, coverageTex, radarInfoTex, targetTex);
	float own = Minimap_ownAt(cell, u, coverageTex, radarInfoTex, targetTex);
	float allied = Minimap_alliedAt(cell, u, coverageTex, radarInfoTex, targetTex);
	// an inactive preview covers nothing: it is drawn fainter, and allied cells under it show as they are
	float inactive = u.inactiveParams.x;
	float fill = max(smoothed * mix(1.0, u.inactiveParams.y, inactive), allied * Minimap_alliedAlpha);

	// outline: the sides of this cell that border a cell the previewed radar does not cover (its own coverage
	// border, drawn even inside allied coverage) or that nobody covers. fwidth gives the cell size in pixels
	// for any minimap size, rotation or PIP zoom.
	float2 inCell = fract(cellPos);
	float2 nearSide = step(float2(0.5), inCell); // 0 = the -x/-z side is nearer, 1 = the +x/+z side
	float2 edgeDist = 0.5 - abs(inCell - 0.5); // distance to the nearer side, in cells
	int2 nx = cell + int2(int(nearSide.x) * 2 - 1, 0);
	int2 nz = cell + int2(0, int(nearSide.y) * 2 - 1);
	int2 nd = int2(nx.x, nz.y); // diagonally across the nearer corner
	float3 ownN = float3(Minimap_ownAt(nx, u, coverageTex, radarInfoTex, targetTex), Minimap_ownAt(nz, u, coverageTex, radarInfoTex, targetTex), Minimap_ownAt(nd, u, coverageTex, radarInfoTex, targetTex));
	float3 alliedN = float3(Minimap_alliedAt(nx, u, coverageTex, radarInfoTex, targetTex), Minimap_alliedAt(nz, u, coverageTex, radarInfoTex, targetTex), Minimap_alliedAt(nd, u, coverageTex, radarInfoTex, targetTex));
	float covered = 1.0 - inactive * (1.0 - allied);
	float3 side = max(own * (1.0 - ownN), covered * (1.0 - max(ownN * (1.0 - inactive), alliedN))); // x side, z side, diagonal
	float2 px = max(fwidth(cellPos), float2(1e-5)) * u.mapParams.w;
	float2 edge = 1.0 - smoothstep(float2(0.0), px, edgeDist);
	// at a concave corner only the diagonal cell is outside: this cell's corner square closes the outline there
	float corner = (1.0 - side.x) * (1.0 - side.y) * side.z * min(edge.x, edge.y);
	// only covered cells are outlined; a cell the previewed radar just started covering fades its outline in
	// with its fill. Allied-only cells use their own outline opacity.
	float previewWeight = own * smoothstep(0.0, 0.5, smoothed);
	float outline = max(max(side.x * edge.x, side.y * edge.y), corner) * max(previewWeight, allied);
	float outlineA = mix(Minimap_alliedOutlineAlpha, Minimap_outlineAlpha * mix(1.0, u.inactiveParams.z, inactive), previewWeight);

	float boundaryMask = 1.0;
	if (float(COVERAGE_CORNER_RADIUS) > 0.0) {
		float ownSdf = Minimap_roundedBoundary(own, ownN, edgeDist, u, coverageTex, radarInfoTex, targetTex);
		float alliedSdf = Minimap_roundedBoundary(allied, alliedN, edgeDist, u, coverageTex, radarInfoTex, targetTex);
		float pixelCells = max(max(fwidth(cellPos).x, fwidth(cellPos).y), 1e-5);
		float ownMask = clamp(ownSdf / pixelCells + 0.5, 0.0, 1.0);
		float alliedMask = clamp(alliedSdf / pixelCells + 0.5, 0.0, 1.0);
		// Concave rounding borrows the neighboring cells' fade rather than appearing fully opaque.
		float ownStrength = own > 0.5 ? smoothed : max(Minimap_smoothedAt(nx, u, coverageTex, radarInfoTex, targetTex), max(Minimap_smoothedAt(nz, u, coverageTex, radarInfoTex, targetTex), Minimap_smoothedAt(nd, u, coverageTex, radarInfoTex, targetTex)));
		smoothed = ownStrength * ownMask;
		allied = max(allied, max(alliedN.x, max(alliedN.y, alliedN.z))) * alliedMask;
		fill = max(smoothed * mix(1.0, u.inactiveParams.y, inactive), allied * Minimap_alliedAlpha);
		previewWeight = ownMask * smoothstep(0.0, 0.5, smoothed);
		// the previewed radar's own border (inside allied coverage too) and the outer border of all coverage
		float width = pixelCells * u.mapParams.w;
		float outerSdf = max(smoothed > 0.00001 ? ownSdf : -1e6, allied > 0.00001 ? alliedSdf : -1e6);
		float ownLine = (1.0 - smoothstep(0.0, width, ownSdf)) * previewWeight;
		outline = max(ownLine, (1.0 - smoothstep(0.0, width, outerSdf)) * max(previewWeight, allied));
		outlineA = mix(Minimap_alliedOutlineAlpha, Minimap_outlineAlpha * mix(1.0, u.inactiveParams.z, inactive), previewWeight);
		boundaryMask = max(ownMask * step(0.00001, smoothed), alliedMask * step(0.00001, allied));
	}

	float alpha = mix(fill * Minimap_fillAlpha, outlineA, outline);
	alpha *= boundaryMask;
	if (alpha < 0.002) {
		discard_fragment();
	}
	float3 color = mix(Minimap_alliedColor, Minimap_sheetColor, smoothed * (1.0 - inactive * allied));
	fragColor = float4(mix(color, Minimap_outlineColor, outline), alpha);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}
)MSL";
}
