// Source-consuming generated shader-only leaf; see CONTRACT.md.
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr char NativeSensorPreviewShaderSource[] = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(// SPDX-License-Identifier: MIT
// Original shader bodies: Copyright (c) 2026 Beherith (mysterme@gmail.com).
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
#ifndef RADAR_CELL_SIZE
#error Actual original Preview define RADAR_CELL_SIZE is required
#endif
#ifndef ALLIED_CIRCLE_SLOTS
#error Actual original Preview define ALLIED_CIRCLE_SLOTS is required
#endif
#ifndef ALLIED_CIRCLE_BAND
#error Actual original Preview define ALLIED_CIRCLE_BAND is required
#endif
#ifndef MODEL_DEPTH_TEST
#error Actual original Preview define MODEL_DEPTH_TEST is required
#endif
#ifndef COVERAGE_CORNER_RADIUS
#error Actual original Preview define COVERAGE_CORNER_RADIUS is required
#endif
#ifndef ALLIED_ALPHA
#error Actual original Preview define ALLIED_ALPHA is required
#endif
#ifndef ALLIED_COLOR
#error Actual original Preview define ALLIED_COLOR is required
#endif
#ifndef SHEET_COLOR
#error Actual original Preview define SHEET_COLOR is required
#endif
#ifndef SHEET_PULSE_COLOR
#error Actual original Preview define SHEET_PULSE_COLOR is required
#endif
#ifndef SHEET_ALPHA
#error Actual original Preview define SHEET_ALPHA is required
#endif
#ifndef SHEET_OUTLINE_ALPHA
#error Actual original Preview define SHEET_OUTLINE_ALPHA is required
#endif
#ifndef SHEET_RING_STRENGTH
#error Actual original Preview define SHEET_RING_STRENGTH is required
#endif
#ifndef MINIMAP_ALPHA
#error Actual original Preview define MINIMAP_ALPHA is required
#endif
#ifndef MINIMAP_OUTLINE_ALPHA
#error Actual original Preview define MINIMAP_OUTLINE_ALPHA is required
#endif
#ifndef MINIMAP_ALLIED_OUTLINE_ALPHA
#error Actual original Preview define MINIMAP_ALLIED_OUTLINE_ALPHA is required
#endif
#ifndef OUTLINE_COLOR
#error Actual original Preview define OUTLINE_COLOR is required
#endif
#ifndef OUTLINE_WIDTH
#error Actual original Preview define OUTLINE_WIDTH is required
#endif
#ifndef MIN_COVERAGE
#error Actual original Preview define MIN_COVERAGE is required
#endif
#ifndef SWEEP_SPEED
#error Actual original Preview define SWEEP_SPEED is required
#endif
#ifndef SWEEP_TRAIL
#error Actual original Preview define SWEEP_TRAIL is required
#endif
#ifndef SWEEP_BEAM
#error Actual original Preview define SWEEP_BEAM is required
#endif
#ifndef SWEEP_STRENGTH
#error Actual original Preview define SWEEP_STRENGTH is required
#endif
#ifndef SPAWN_SPEED
#error Actual original Preview define SPAWN_SPEED is required
#endif
#ifndef PULSE_SPACING
#error Actual original Preview define PULSE_SPACING is required
#endif
#ifndef PULSE_SPEED
#error Actual original Preview define PULSE_SPEED is required
#endif
#ifndef PULSE_POWER
#error Actual original Preview define PULSE_POWER is required
#endif
#ifndef PULSE_SYMMETRIC
#error Actual original Preview define PULSE_SYMMETRIC is required
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

// Original source: sensor_ranges_radar_preview_mip.frag.glsl SHA256 23b1b9281d2f78d3ad27089e7e93fa579e45276b6c09f9ede77a4a15cddb6e57

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview: rebuilds the engine's mip-level radar heightmap on the GPU. The engine averages
// 2x2 blocks per mip level starting from the square-center heightmap (itself the mean of a square's
// four corners), so a radar cell's height is a weighted mean of the (squares+1)^2 corner heights
// covering it, with corner weights 1-2-2-...-2-1 along each axis. Runs once a second while the
// preview is shown (terraform), one texel per radar cell.

//__DEFINES__



fragment float4 OriginalPreviewMipFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 texture2d<float> heightmapTex [[texture(0), function_constant(PreviewTexture0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	int squares = int(u.mipParams.x);
	int2 origin = int2(sourceFragment) * squares;
	int2 maxTexel = PreviewSize(heightmapTex) - 1;

	float sum = 0.0;
	for (int j = 0; j <= squares; j++) {
		float wj = (j == 0 || j == squares) ? 1.0 : 2.0;
		for (int i = 0; i <= squares; i++) {
			float wi = (i == 0 || i == squares) ? 1.0 : 2.0;
			sum += wi * wj * PreviewFetch(heightmapTex,min(origin + int2(i, j), maxTexel),0u,u).r;
		}
	}
	fragColor = float4(sum / float(4 * squares * squares), 0.0, 0.0, 1.0);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original source: sensor_ranges_radar_preview_coverage.frag.glsl SHA256 bc32215d7fafc1bd7ebdf5ab2f3b19168fbd7e65849bcc74fa8ce4ecdd50eb17

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview coverage pass: replicates the engine's radar LOS (rts/Sim/Misc/LosMap.cpp) on the
// GPU, one texel per radar cell (8 << radarMipLevel elmos) around the emitter's cell.
// The engine casts a fixed set of rays (to the disk perimeter plus fill-in rays, see
// CLosTableHelper::GetLosRays) in four 90 degree rotations, and a cell stays visible only if no ray
// through it finds it blocked (CastLos angle test with Coverage_LOS_BONUS_HEIGHT on the mip-level heightmap).
// The widget precomputes, per first-quadrant cell, the targets of all rays through it; this shader
// walks each of those rays up to the cell. Only re-rendered when the emitter cell or height changes.


//__DEFINES__

// 0: the target texture is the (2 * radius + 1)^2 disc around the emitter (texel = cell offset + radius)
// 1: the target texture is the whole map (texel = absolute radar cell), used to union allied radars
// 0: the coverage (0/1). Allied circle passes, MAX-blended over the allied radars into map-sized textures: 1, per radar
// cell the radar whose range circle reaches farthest past the cell center among those covering the cell's terrain,
// packed as (distance level, circle index); 2, the runner-up, the same leaving out the radar of pass 1.

// per first-quadrant cell (y * (radius + 1) + x): (offset, count); then per ray through a cell: (targetX, targetY)
// Original SSBO5 supplied as its exact retained range at native buffer4.


constant float Coverage_LOS_BONUS_HEIGHT = 5.0;

// (height - emitter + bonus) / distance like the engine's raycastAngles; off-map cells never block
inline float Coverage_cellAngle(int2 cell, int2 off, float losHeight, constant PreviewUniforms& u, texture2d<float> mipHeightTex, texture2d<float> alliedWinnerTex, const device float4* rayData) {
	if (any(lessThan(cell, int2(0))) || any(greaterThanEqual(cell, PreviewSize(mipHeightTex)))) {
		return -1e8;
	}
	float h = max(0.0, PreviewFetch(mipHeightTex,cell,0u,u).r);
	return (h - losHeight + Coverage_LOS_BONUS_HEIGHT) * rsqrt(float(off.x * off.x + off.y * off.y));
}

// the engine casts each first-quadrant ray as s, -s, (s.y, -s.x) and (-s.y, s.x)
inline int2 Coverage_rotateOut(int2 p, int rot, constant PreviewUniforms& u, texture2d<float> mipHeightTex, texture2d<float> alliedWinnerTex, const device float4* rayData) {
	return (rot == 0) ? p : (rot == 1) ? -p : (rot == 2) ? int2(p.y, -p.x) : int2(-p.y, p.x);
}

inline int2 Coverage_rotateIn(int2 c, int rot, constant PreviewUniforms& u, texture2d<float> mipHeightTex, texture2d<float> alliedWinnerTex, const device float4* rayData) {
	return (rot == 0) ? c : (rot == 1) ? -c : (rot == 2) ? int2(-c.y, c.x) : int2(c.y, -c.x);
}

// CastLos along the ray to `target` (first-quadrant coordinates) up to step `steps`, in rotation `rot`
inline bool Coverage_visibleAlongRay(int2 target, int steps, int rot, int2 base, float losHeight, constant PreviewUniforms& u, texture2d<float> mipHeightTex, texture2d<float> alliedWinnerTex, const device float4* rayData) {
	bool xMajor = target.x > target.y;
	float slope = xMajor ? float(target.y) / float(target.x) : float(target.x) / float(target.y);
	float prvAngle = -1e7;
	float maxAngle = -1e7;
	bool vis = true;
	for (int i = 1; i <= steps; i++) {
		int minor = int(floor(slope * float(i) + 0.5));
		int2 o = Coverage_rotateOut(xMajor ? int2(i, minor) : int2(minor, i), rot, u, mipHeightTex, alliedWinnerTex, rayData);
		float angle = Coverage_cellAngle(base + o, o, losHeight, u, mipHeightTex, alliedWinnerTex, rayData);

		vis = true;
		if (angle < maxAngle) {
			vis = false;
		} else {
			if (angle < prvAngle) {
				maxAngle = prvAngle - Coverage_LOS_BONUS_HEIGHT * rsqrt(float(o.x * o.x + o.y * o.y));
				if (angle < maxAngle) {
					vis = false;
				}
			}
			if (vis) {
				prvAngle = angle;
			}
		}
	}
	return vis;
}

inline float Coverage_engineCoverage(int2 off, constant PreviewUniforms& u, texture2d<float> mipHeightTex, texture2d<float> alliedWinnerTex, const device float4* rayData) {
	int radius = int(u.losParams.z);
	int2 base = int2(u.losParams.xy);
	float losHeight = u.losParams.w;

	if (any(greaterThan(abs(off), int2(radius)))) {
		return 0.0;
	}
	if (all(off == int2(0))) {
		return 1.0;
	}

	int stride = radius + 1;
	bool onDisk = false;
	bool visible = true;
	// cells on an axis belong to two rotations, so check all four
	for (int rot = 0; rot < 4 && visible; rot++) {
		int2 q = Coverage_rotateIn(off, rot, u, mipHeightTex, alliedWinnerTex, rayData);
		if (q.x < 0 || q.y < 0 || q.x > radius || q.y > radius) {
			continue;
		}
		float4 head = rayData[q.y * stride + q.x];
		int listOffset = int(head.x);
		int count = int(head.y);
		if (count > 0) {
			onDisk = true;
		}
		int steps = max(q.x, q.y); // rays step along their major axis, so this is q's index on every ray through it
		for (int k = 0; k < count && visible; k++) {
			int2 target = int2(rayData[listOffset + k].xy);
			visible = Coverage_visibleAlongRay(target, steps, rot, base, losHeight, u, mipHeightTex, alliedWinnerTex, rayData);
		}
	}

	return (onDisk && visible) ? 1.0 : 0.0;
}

fragment float4 OriginalPreviewCoverageFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 texture2d<float> mipHeightTex [[texture(0), function_constant(PreviewTexture0)]],
 texture2d<float> alliedWinnerTex [[texture(1), function_constant(PreviewTexture1)]],
 const device float4* rayData [[buffer(4), function_constant(PreviewStorage)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	int2 base = int2(u.losParams.xy);
	int2 off = int2(sourceFragment) - ((u.coverageFlags.x > 0.5) ? base : int2(int(u.losParams.z)));
	if (u.coverageFlags.y < 0.5) {
		fragColor = float4(Coverage_engineCoverage(off, u, mipHeightTex, alliedWinnerTex, rayData), 0.0, 0.0, 1.0);
		if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
	}

	// distance from the cell center to the range circle (elmos, > 0 inside); cells past the band record nothing
	float band = float(ALLIED_CIRCLE_BAND);
	float sdf = u.alliedCircle.z - distance(sourceFragment * float(RADAR_CELL_SIZE), u.alliedCircle.xy);
	if (sdf <= -band) {
		discard_fragment();
	}
	float coverage = Coverage_engineCoverage(off, u, mipHeightTex, alliedWinnerTex, rayData);
	// past the disc the perimeter's terrain shadow carries on outward, as the sheet's circleCoverage does
	float dist = length(float2(off));
	if (coverage <= 0.0 && dist > u.losParams.z) {
		coverage = Coverage_engineCoverage(int2(floor(float2(off) * (max(u.losParams.z - 0.75, 0.0) / dist) + 0.5)), u, mipHeightTex, alliedWinnerTex, rayData);
	}
	if (coverage <= 0.0) {
		discard_fragment();
	}
	// level 1.. (0 = no radar) above the index: MAX blending keeps the largest distance, and the packed value stays an
	// integer below 2^24, exact in the R32F target
	float slots = float(ALLIED_CIRCLE_SLOTS);
	float levels = 16777216.0 / slots;
	float level = 1.0 + floor(clamp(sdf / (2.0 * band) + 0.5, 0.0, 1.0) * (levels - 2.0) + 0.5);
	float entry = level * slots + u.alliedCircle.w;
	if (u.coverageFlags.y > 1.5 && PreviewFetch(alliedWinnerTex,int2(sourceFragment),1u,u).r == entry) {
		discard_fragment();
	}
	fragColor = float4(entry, 0.0, 0.0, 1.0);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original source: sensor_ranges_radar_preview_smooth.frag.glsl SHA256 a6fa142b81a112ac2858297ae17bbde48e56314917e6e605f2e261d6bc518c6b

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

// Original source: sensor_ranges_radar_preview.frag.glsl SHA256 0fad4d81715ae6870b55660d4eae7f9743440f6503fadeba3c064174809be4c5

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview sheet, projected onto the terrain: drawn over the screen area of the coverage, each pixel rebuilds
// its world position from the map g-buffer depth and takes the coverage of its radar cell (the engine-cell coverage,
// see sensor_ranges_radar_preview_coverage.frag.glsl: the rounded cells follow the exact coverage, the fill fades with
// the smoothed one from _smooth.frag.glsl), with the rings and the sweep as
// smooth per-pixel gradients, outlined along the border with uncovered radar cells. Being the rendered terrain
// itself, it covers steep cliffs a sheet of per-cell quads cuts through. Over water it lies on the water surface;
// units and features in front of the terrain stay untinted.


//__DEFINES__

#if MODEL_DEPTH_TEST
#endif
// Circle mode with allied coverage: per radar cell, the allied radar whose range circle reaches farthest past the cell
// among those covering its terrain, and the runner-up (level * ALLIED_CIRCLE_SLOTS + circle index, 0 = none;
// sensor_ranges_radar_preview_coverage.frag.glsl)
// Original SSBO5 supplied as its exact retained range at native buffer4.

// x = 1: circle mode (SensorPreviewCircles), y = the previewed radar's range (elmos)

//__ENGINEUNIFORMBUFFERDEFS__



constant float Sheet_cellSize = float(RADAR_CELL_SIZE);
constant float Sheet_minCoverage = float(MIN_COVERAGE);
constant float Sheet_spawnSpeed = float(SPAWN_SPEED);
constant float Sheet_alliedAlpha = float(ALLIED_ALPHA);
constant float3 Sheet_alliedColor = ALLIED_COLOR;
constant float3 Sheet_outlineColor = OUTLINE_COLOR;
constant float Sheet_outlineWidth = float(OUTLINE_WIDTH); // pixels at 1080p, scaled with the vertical resolution

constant float Sheet_PI = 3.1415927;
constant float Sheet_pulseSymmetric = float(PULSE_SYMMETRIC); // ring profile: 1 = bell (fade in/out), 0 = sharp front, fade out
constant float3 Sheet_sheetColor = SHEET_COLOR;
constant float3 Sheet_sheetPulseColor = SHEET_PULSE_COLOR;
constant float Sheet_sheetAlpha = float(SHEET_ALPHA);
constant float Sheet_sheetOutlineAlpha = float(SHEET_OUTLINE_ALPHA);
constant float Sheet_sheetRingStrength = float(SHEET_RING_STRENGTH);
constant float Sheet_pulseSpacing = float(PULSE_SPACING); // elmos between rings
constant float Sheet_pulseSpeed = float(PULSE_SPEED);     // elmos per second the rings travel outward
constant float Sheet_pulsePower = float(PULSE_POWER);     // higher = shorter tail behind a ring's leading edge
constant float Sheet_sweepSpeed = float(SWEEP_SPEED);
constant float Sheet_sweepStrength = float(SWEEP_STRENGTH);

// coverage of the previewed radar at a radar cell, x exact (0/1) and y smoothed, -1 outside its disc
inline float2 Sheet_previewCoverageAt(int2 radarCell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	int2 texel = radarCell - int2(u.lookupParams.xy) + int2(int(u.lookupParams.z));
	if (any(lessThan(texel, int2(0))) || any(greaterThanEqual(texel, PreviewSize(coverageTex)))) {
		return float2(-1.0);
	}
	return float2(PreviewFetch(targetTex,texel,6u,u).r, PreviewFetch(coverageTex,texel,2u,u).r);
}

// Circle mode: the previewed radar's coverage at a radar cell, with the cells past its disc (between the engine's
// staircase and the range circle) taking the coverage of the cell further in on the line from the emitter, so terrain
// shadows carry on to the circle. The coverage is 0 both off the disc and in shadow, so every empty cell farther out
// than the disc's radius counts as off it; shadowed cells there take their inner neighbour's shadow anyway.
inline float2 Sheet_circleCoverageAt(int2 radarCell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	float2 coverage = Sheet_previewCoverageAt(radarCell, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float2 offset = float2(radarCell - int2(u.lookupParams.xy));
	float dist = length(offset);
	if (coverage.x <= 0.0 && dist > u.lookupParams.z) {
		int2 inner = int2(u.lookupParams.xy) + int2(floor(offset * ((u.lookupParams.z - 0.75) / dist) + 0.5));
		coverage = Sheet_previewCoverageAt(inner, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	}
	return max(coverage, 0.0);
}

// 1 where an allied radar covers the radar cell (engine radar map or our union), 0 with allied coverage off
inline float Sheet_alliedAt(int2 radarCell, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	if (u.lookupParams.w < 0.5 || any(lessThan(radarCell, int2(0))) || any(greaterThanEqual(radarCell, PreviewSize(radarInfoTex)))) {
		return 0.0;
	}
	return step(0.5, PreviewFetch(radarInfoTex,radarCell,3u,u).r);
}

// signed distance (elmos, > 0 inside) from worldXZ to the range circle of the allied radar in a texel of
// alliedWinnerTex / alliedRunnerUpTex
inline float Sheet_packedCircleDist(float entry, float2 worldXZ, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	float4 circle = alliedCircles[int(entry) % ALLIED_CIRCLE_SLOTS];
	return circle.z - distance(worldXZ, circle.xy);
}

// circle mode: signed distance (cells) from worldXZ to the range circles of the allied radars covering a radar cell's
// terrain (the two reaching farthest past it, which covers the cells where two circles cross), -1e6 for none
inline float Sheet_cellCircleDist(int2 radarCell, float2 worldXZ, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	if (any(lessThan(radarCell, int2(0))) || any(greaterThanEqual(radarCell, PreviewSize(alliedWinnerTex)))) {
		return -1e6;
	}
	float winner = PreviewFetch(alliedWinnerTex,radarCell,4u,u).r;
	if (winner < 0.5) {
		return -1e6;
	}
	float dist = Sheet_packedCircleDist(winner, worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float runnerUp = PreviewFetch(alliedRunnerUpTex,radarCell,5u,u).r;
	if (runnerUp > 0.5) {
		dist = max(dist, Sheet_packedCircleDist(runnerUp, worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles));
	}
	return dist / Sheet_cellSize;
}

// Signed distance in cells to the local boundary, with convex corners cut back and concave
// corners filled. The half-cell radius limit keeps neighboring corners from overlapping.
inline float Sheet_roundedBoundary(float covered, float3 neighbours, float2 edgeDist, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
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

// x exact (0/1), y smoothed
inline float2 Sheet_boundaryCoverageAt(int2 cell, bool allied, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	if (allied) {
		return float2(Sheet_alliedAt(cell, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles));
	}
	return max(u.circleParams.x > 0.5 ? Sheet_circleCoverageAt(cell, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles) : Sheet_previewCoverageAt(cell, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles), 0.0);
}

// The rounded cells of the previewed radar's or the allied coverage at a pixel, shaped by the exact coverage so they move
// with the preview at once: returns the smoothed coverage of its cell (in an uncovered cell, that of its covered
// neighbours, which a filled concave corner borrows), with sdf the signed distance to their boundary in cells (> 0 inside)
inline float Sheet_roundedCells(float2 coord, bool allied, thread float& sdf, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	int2 cell = int2(floor(coord));
	float2 inCell = fract(coord);
	int2 direction = int2(step(float2(0.5), inCell)) * 2 - 1;
	float2 center = Sheet_boundaryCoverageAt(cell, allied, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float2 nx = Sheet_boundaryCoverageAt(cell + int2(direction.x, 0), allied, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float2 nz = Sheet_boundaryCoverageAt(cell + int2(0, direction.y), allied, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float2 nd = Sheet_boundaryCoverageAt(cell + direction, allied, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	float3 neighbours = step(float3(0.5), float3(nx.x, nz.x, nd.x));
	sdf = Sheet_roundedBoundary(step(0.5, center.x), neighbours, 0.5 - abs(inCell - 0.5), u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	if (center.x > 0.5) {
		return center.y;
	}
	return max(nx.y * neighbours.x, max(nz.y * neighbours.y, nd.y * neighbours.z));
}

// Circle mode: signed distance (cells, > 0 inside) to the boundary of the allied coverage at a pixel. Cells near a range
// circle can belong to radars whose circles end short of this pixel, so a neighbouring cell counts as covered for the
// rounded cell boundary only where its own radars' circles reach the pixel; then this cell's coverage is clipped to its
// circles, and a filled concave corner to the neighbours'. Away from the cell sides (further than `reach`, cells) the
// neighbours change nothing visible and are skipped.
inline float Sheet_alliedCircleCells(float2 coord, float2 worldXZ, float reach, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	int2 cell = int2(floor(coord));
	float2 inCell = fract(coord);
	float2 edgeDist = 0.5 - abs(inCell - 0.5);
	float own = Sheet_cellCircleDist(cell, worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	if (all(greaterThan(edgeDist, float2(reach)))) {
		return (own > -1e5) ? min(own, min(edgeDist.x, edgeDist.y)) : -1e6;
	}
	int2 direction = int2(step(float2(0.5), inCell)) * 2 - 1;
	float3 neighbourDist = float3(
		Sheet_cellCircleDist(cell + int2(direction.x, 0), worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles),
		Sheet_cellCircleDist(cell + int2(0, direction.y), worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles),
		Sheet_cellCircleDist(cell + direction, worldXZ, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles));
	float3 neighbours = select(float3(0.0), float3(1.0), greaterThan(neighbourDist, float3(0.0)));
	float sdf = min(Sheet_roundedBoundary(0.0, neighbours, edgeDist, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles), max(neighbourDist.x, max(neighbourDist.y, neighbourDist.z)));
	if (own > -1e5) {
		sdf = max(sdf, min(Sheet_roundedBoundary(1.0, neighbours, edgeDist, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles), own));
	}
	return sdf;
}

// rings travelling outward from the radar (a bright leading edge with a tail fading inward) plus the rotating
// sweep, evaluated per pixel so the sheet shows them as continuous gradients rather than per-cell steps
inline float Sheet_sheetGlow(float2 fromCenter, float time, constant PreviewUniforms& u, texture2d<float> coverageTex, texture2d<float> radarInfoTex, texture2d<float> alliedWinnerTex, texture2d<float> alliedRunnerUpTex, texture2d<float> targetTex, const device float4* alliedCircles) {
	float dist = length(fromCenter);
	float phase = fract((dist - time * Sheet_pulseSpeed) / Sheet_pulseSpacing); // 0 at a ring's center, 1 at the next ring
	// PULSE_SYMMETRIC: smooth bell that fades in and out around the ring, or a sharp front fading out behind it
	float ring = (Sheet_pulseSymmetric > 0.5) ? pow(0.5 + 0.5 * cos(phase * 2.0 * Sheet_PI), Sheet_pulsePower) : pow(1.0 - phase, Sheet_pulsePower);
	float glow = ring * Sheet_sheetRingStrength;
	if (u.animParams.w > 0.5) { // RadarPreviewSweep
		// Keep the original max calls inside the shader; Metal forbids global constructors.
		const float Sheet_sweepTrail = max(float(SWEEP_TRAIL), 0.01); // degrees
		const float Sheet_sweepBeam = max(float(SWEEP_BEAM), 0.01);   // degrees
		float angle = atan2(fromCenter.y, fromCenter.x) / (2.0 * Sheet_PI) + 0.5;
		float behind = (1.0 - fract(angle - time * Sheet_sweepSpeed)) * 360.0; // degrees behind the sweep's leading edge
		float trail = clamp(1.0 - behind / Sheet_sweepTrail, 0.0, 1.0);
		glow += (trail * trail + (1.0 - smoothstep(0.0, Sheet_sweepBeam, behind))) * Sheet_sweepStrength;
	}
	return clamp(glow, 0.0, 1.0);
}

fragment float4 OriginalPreviewSheetFragment(
 PreviewFragmentInput in [[stage_in]], constant PreviewUniforms& u [[buffer(3)]],
 PreviewMapDepthTexture mapDepths [[texture(0), function_constant(PreviewTexture0)]]
#if MODEL_DEPTH_TEST
,
 PreviewModelDepthTexture modelDepths [[texture(1), function_constant(PreviewTexture1)]]
#endif
,
 texture2d<float> coverageTex [[texture(2), function_constant(PreviewTexture2)]],
 texture2d<float> radarInfoTex [[texture(3), function_constant(PreviewTexture3)]],
 texture2d<float> alliedWinnerTex [[texture(4), function_constant(PreviewTexture4)]],
 texture2d<float> alliedRunnerUpTex [[texture(5), function_constant(PreviewTexture5)]],
 texture2d<float> targetTex [[texture(6), function_constant(PreviewTexture6)]],
 sampler sampler0 [[sampler(0)]]
#if MODEL_DEPTH_TEST
 ,sampler sampler1 [[sampler(1)]]
#endif
,
 constant float4x4* engineMatrices [[buffer(0)]], constant float4* engineParams [[buffer(1)]], const device float4* alliedCircles [[buffer(4), function_constant(PreviewStorage)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
 const float2 sourceFragment=in.position.xy*u.fragmentTransform.xy+u.fragmentTransform.zw;
	float2 screenUV = (sourceFragment - engineParams[2].zw) / engineParams[2].xy;
	float mapDepth = PreviewSample(mapDepths,sampler0,screenUV,0u,u).x;
	float4 world = engineMatrices[9] * float4(screenUV * 2.0 - 1.0, mapDepth, 1.0);
	float3 worldPos = world.xyz / world.w;
	// over water the coverage lies on the water surface, where the view ray crosses y = 0
	float3 camPos = engineMatrices[7][3].xyz;
	if (worldPos.y < 0.0 && camPos.y > 0.0) {
		worldPos = mix(camPos, worldPos, camPos.y / (camPos.y - worldPos.y));
	}
	float2 cellCoord = worldPos.xz / Sheet_cellSize; // position in radar cells
	float2 cellPixels = fwidth(cellCoord); // radar cells per pixel, taken before any discard

	if (mapDepth >= 0.999999) {
		discard_fragment(); // sky
	}

	int2 cell = int2(floor(cellCoord));
	bool circles = u.circleParams.x > 0.5;
	float pixelCells = max(max(cellPixels.x, cellPixels.y), 1e-5);
	// circle mode: the previewed radar's coverage ends at a circle at its range around it
	float circleSdf = u.circleParams.y - distance(worldPos.xz, u.radarcenter_range.xz);
	if (u.lookupParams.w < 0.5 && (circles ? circleSdf < -pixelCells * Sheet_cellSize : Sheet_previewCoverageAt(cell, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles).x < 0.0)) {
		discard_fragment();
	}

	// The previewed radar's coverage and, with allied coverage on (u.lookupParams.w), that of all allied radars: each
	// its rounded cells, in circle mode clipped to the range circles, antialiased along the signed distance (cells)
	float ownBoundarySdf;
	float ownCoverage = Sheet_roundedCells(cellCoord, false, ownBoundarySdf, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
	if (circles) {
		ownBoundarySdf = min(ownBoundarySdf, circleSdf / Sheet_cellSize);
	}
	ownCoverage *= clamp(ownBoundarySdf / pixelCells + 0.5, 0.0, 1.0);
	float ownWeight = smoothstep(0.0, 0.5, ownCoverage);
	float coverage = ownCoverage;

	// cells covered only by other allied radars are drawn too but stay static; the previewed radar's animation
	// applies in proportion to its own coverage
	float width = pixelCells * Sheet_outlineWidth * engineParams[2].y / 1080.0; // outline width in cells
	float weight = 1.0;
	float alliedBoundarySdf = -1e6;
	float alliedCoverage = 0.0;
	if (u.lookupParams.w > 0.5) {
		weight = ownWeight;
		if (circles) {
			alliedBoundarySdf = Sheet_alliedCircleCells(cellCoord, worldPos.xz, max(float(COVERAGE_CORNER_RADIUS), width + pixelCells), u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
			alliedCoverage = 1.0;
		} else {
			alliedCoverage = Sheet_roundedCells(cellCoord, true, alliedBoundarySdf, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles);
		}
		alliedCoverage *= clamp(alliedBoundarySdf / pixelCells + 0.5, 0.0, 1.0);
		coverage = max(coverage, alliedCoverage);
	}

	// spawn ripple: the sheet spreads outward from the radar when the preview appears; cells of other
	// allied radars simply fade in
	float2 cellXZ = (float2(cell) + 0.5) * Sheet_cellSize;
	float distN = length(cellXZ - u.radarcenter_range.xz) / u.radarcenter_range.w;
	float front = u.animParams.y * Sheet_spawnSpeed;
	float spawn = mix(min(u.animParams.y * 4.0, 1.0), smoothstep(distN - 0.10, distN + 0.02, front), weight);

	if (coverage < Sheet_minCoverage || spawn < 0.01) {
		discard_fragment();
	}
#if MODEL_DEPTH_TEST
	if (PreviewSample(modelDepths,sampler1,screenUV,1u,u).x < mapDepth) {
		discard_fragment(); // a unit or feature in front of the terrain
	}
#endif
	float fade = spawn * smoothstep(0.0, 0.5, coverage) * mix(Sheet_alliedAlpha, 1.0, weight);

	// outline along the previewed radar's own border (inside allied coverage too) and along the outer border of all
	// coverage; where they coincide they are the same line. None where a pixel spans several cells (far out, or where
	// the position jumps between pixels at terrain silhouettes).
	float outerSdf = max(ownCoverage >= Sheet_minCoverage ? ownBoundarySdf : -1e6, alliedCoverage >= Sheet_minCoverage ? alliedBoundarySdf : -1e6);
	float ownLine = (1.0 - smoothstep(0.0, width, ownBoundarySdf)) * ownWeight;
	float outline = pixelCells < 4.0 ? max(ownLine, 1.0 - smoothstep(0.0, width, outerSdf)) : 0.0;

	// only the previewed radar's own coverage animates (weight), cells of other allied radars stay still
	float glow = 0.0;
	if (weight > 0.0 && u.animParams.z > 0.5) {
		glow = Sheet_sheetGlow(worldPos.xz - u.radarcenter_range.xz, u.animParams.x, u, coverageTex, radarInfoTex, alliedWinnerTex, alliedRunnerUpTex, targetTex, alliedCircles) * weight;
	}
	// allied-only cells muted, the rings blend it all the way to the pulse color
	float3 tint = mix(Sheet_alliedColor, Sheet_sheetColor, weight);
	float3 fillColor = mix(tint, Sheet_sheetPulseColor, glow);
	float fillAlpha = min(Sheet_sheetAlpha * (1.0 + glow), 1.0);
	fragColor = float4(mix(fillColor, Sheet_outlineColor, outline), mix(fillAlpha, Sheet_sheetOutlineAlpha, outline) * fade);

 if(!PreviewAlphaPass(fragColor.a,u))discard_fragment();
 return fragColor;
}

// Original source: sensor_ranges_radar_preview_minimap.frag.glsl SHA256 74dc6eb03ba6de3acb25e8468bdcb78bb77375c5d58a53798c5101f79b19a76b

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
