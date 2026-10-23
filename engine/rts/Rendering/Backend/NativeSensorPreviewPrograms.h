/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::sensorPreviewProgram {
inline constexpr std::string_view PassVertex=R"SOURCE(#version 420
#line 30000

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Fullscreen pass vertex shader shared by the radar preview's coverage and smoothing passes.
// Draw with a clip-space rect (InstanceVBOTable.MakeTexRectVAO), no matrices involved.

//__DEFINES__

layout (location = 0) in vec4 pos; // xy = clip space position, zw = uv (unused)

uniform vec4 passRect = vec4(-1.0, -1.0, 1.0, 1.0); // clip-space rectangle the quad is drawn into (whole target by default)

void main() {
	gl_Position = vec4(mix(passRect.xy, passRect.zw, pos.xy * 0.5 + 0.5), 0.0, 1.0);
	// Offscreen passes and the projected sheet do not use the engine's fixed-function clip planes.
	gl_ClipDistance[0] = 1.0;
	gl_ClipDistance[1] = 1.0;
	gl_ClipDistance[2] = 1.0;
	gl_ClipDistance[3] = 1.0;
}
)SOURCE";
inline constexpr std::string_view PassVertexSHA512="254e77f81bd14c68e95805648d479e8a83c04f4304b6b578a118188c2f4e4e223427ae9baeb5ab21e6ecda044b15d8befca024113fe3766d4ed8279af238aad6";
inline constexpr std::string_view MipFragment=R"SOURCE(#version 420
#line 34000

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview: rebuilds the engine's mip-level radar heightmap on the GPU. The engine averages
// 2x2 blocks per mip level starting from the square-center heightmap (itself the mean of a square's
// four corners), so a radar cell's height is a weighted mean of the (squares+1)^2 corner heights
// covering it, with corner weights 1-2-2-...-2-1 along each axis. Runs once a second while the
// preview is shown (terraform), one texel per radar cell.

//__DEFINES__

uniform sampler2D heightmapTex; // engine corner heightmap, one texel per 8 elmo
uniform vec4 mipParams;         // heightmap squares per radar cell (8 for mip level 3), unused x3

out vec4 fragColor;

void main() {
	int squares = int(mipParams.x);
	ivec2 origin = ivec2(gl_FragCoord.xy) * squares;
	ivec2 maxTexel = textureSize(heightmapTex, 0) - 1;

	float sum = 0.0;
	for (int j = 0; j <= squares; j++) {
		float wj = (j == 0 || j == squares) ? 1.0 : 2.0;
		for (int i = 0; i <= squares; i++) {
			float wi = (i == 0 || i == squares) ? 1.0 : 2.0;
			sum += wi * wj * texelFetch(heightmapTex, min(origin + ivec2(i, j), maxTexel), 0).r;
		}
	}
	fragColor = vec4(sum / float(4 * squares * squares), 0.0, 0.0, 1.0);
}
)SOURCE";
inline constexpr std::string_view MipFragmentSHA512="b7fd9118d6b328dd9da185f54f95b467f8433caffbb27acb183edfa921cdb4028e85a979503c648019089150821dd72aa4057d8e4f3fa5147658dfef127b515e";
inline constexpr std::string_view CoverageFragment=R"SOURCE(#version 420
#line 31000

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview coverage pass: replicates the engine's radar LOS (rts/Sim/Misc/LosMap.cpp) on the
// GPU, one texel per radar cell (8 << radarMipLevel elmos) around the emitter's cell.
// The engine casts a fixed set of rays (to the disk perimeter plus fill-in rays, see
// CLosTableHelper::GetLosRays) in four 90 degree rotations, and a cell stays visible only if no ray
// through it finds it blocked (CastLos angle test with LOS_BONUS_HEIGHT on the mip-level heightmap).
// The widget precomputes, per first-quadrant cell, the targets of all rays through it; this shader
// walks each of those rays up to the cell. Only re-rendered when the emitter cell or height changes.

#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_shader_storage_buffer_object : require

//__DEFINES__

uniform sampler2D mipHeightTex; // radar-cell heightmap (sensor_ranges_radar_preview_mip.frag.glsl)
uniform vec4 losParams;         // emitter cell x, emitter cell y, radius in cells, emitter height (bucketed)
// 0: the target texture is the (2 * radius + 1)^2 disc around the emitter (texel = cell offset + radius)
// 1: the target texture is the whole map (texel = absolute radar cell), used to union allied radars
uniform float coverageAbsolute = 0.0;
// 0: the coverage (0/1). Allied circle passes, MAX-blended over the allied radars into map-sized textures: 1, per radar
// cell the radar whose range circle reaches farthest past the cell center among those covering the cell's terrain,
// packed as (distance level, circle index); 2, the runner-up, the same leaving out the radar of pass 1.
uniform float alliedCirclePass = 0.0;
uniform vec4 alliedCircle = vec4(0.0); // circle passes: center x, center z, range (elmos), index into the circle list
uniform sampler2D alliedWinnerTex;     // pass 2: the result of pass 1

// per first-quadrant cell (y * (radius + 1) + x): (offset, count); then per ray through a cell: (targetX, targetY)
layout(std430, binding = 5) buffer RayData {
	vec4 rayData[];
};

out vec4 fragColor;

const float LOS_BONUS_HEIGHT = 5.0;

// (height - emitter + bonus) / distance like the engine's raycastAngles; off-map cells never block
float cellAngle(ivec2 cell, ivec2 off, float losHeight) {
	if (any(lessThan(cell, ivec2(0))) || any(greaterThanEqual(cell, textureSize(mipHeightTex, 0)))) {
		return -1e8;
	}
	float h = max(0.0, texelFetch(mipHeightTex, cell, 0).r);
	return (h - losHeight + LOS_BONUS_HEIGHT) * inversesqrt(float(off.x * off.x + off.y * off.y));
}

// the engine casts each first-quadrant ray as s, -s, (s.y, -s.x) and (-s.y, s.x)
ivec2 rotateOut(ivec2 p, int rot) {
	return (rot == 0) ? p : (rot == 1) ? -p : (rot == 2) ? ivec2(p.y, -p.x) : ivec2(-p.y, p.x);
}

ivec2 rotateIn(ivec2 c, int rot) {
	return (rot == 0) ? c : (rot == 1) ? -c : (rot == 2) ? ivec2(-c.y, c.x) : ivec2(c.y, -c.x);
}

// CastLos along the ray to `target` (first-quadrant coordinates) up to step `steps`, in rotation `rot`
bool visibleAlongRay(ivec2 target, int steps, int rot, ivec2 base, float losHeight) {
	bool xMajor = target.x > target.y;
	float slope = xMajor ? float(target.y) / float(target.x) : float(target.x) / float(target.y);
	float prvAngle = -1e7;
	float maxAngle = -1e7;
	bool vis = true;
	for (int i = 1; i <= steps; i++) {
		int minor = int(floor(slope * float(i) + 0.5));
		ivec2 o = rotateOut(xMajor ? ivec2(i, minor) : ivec2(minor, i), rot);
		float angle = cellAngle(base + o, o, losHeight);

		vis = true;
		if (angle < maxAngle) {
			vis = false;
		} else {
			if (angle < prvAngle) {
				maxAngle = prvAngle - LOS_BONUS_HEIGHT * inversesqrt(float(o.x * o.x + o.y * o.y));
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

float engineCoverage(ivec2 off) {
	int radius = int(losParams.z);
	ivec2 base = ivec2(losParams.xy);
	float losHeight = losParams.w;

	if (any(greaterThan(abs(off), ivec2(radius)))) {
		return 0.0;
	}
	if (off == ivec2(0)) {
		return 1.0;
	}

	int stride = radius + 1;
	bool onDisk = false;
	bool visible = true;
	// cells on an axis belong to two rotations, so check all four
	for (int rot = 0; rot < 4 && visible; rot++) {
		ivec2 q = rotateIn(off, rot);
		if (q.x < 0 || q.y < 0 || q.x > radius || q.y > radius) {
			continue;
		}
		vec4 head = rayData[q.y * stride + q.x];
		int listOffset = int(head.x);
		int count = int(head.y);
		if (count > 0) {
			onDisk = true;
		}
		int steps = max(q.x, q.y); // rays step along their major axis, so this is q's index on every ray through it
		for (int k = 0; k < count && visible; k++) {
			ivec2 target = ivec2(rayData[listOffset + k].xy);
			visible = visibleAlongRay(target, steps, rot, base, losHeight);
		}
	}

	return (onDisk && visible) ? 1.0 : 0.0;
}

void main() {
	ivec2 base = ivec2(losParams.xy);
	ivec2 off = ivec2(gl_FragCoord.xy) - ((coverageAbsolute > 0.5) ? base : ivec2(int(losParams.z)));
	if (alliedCirclePass < 0.5) {
		fragColor = vec4(engineCoverage(off), 0.0, 0.0, 1.0);
		return;
	}

	// distance from the cell center to the range circle (elmos, > 0 inside); cells past the band record nothing
	float band = float(ALLIED_CIRCLE_BAND);
	float sdf = alliedCircle.z - distance(gl_FragCoord.xy * float(RADAR_CELL_SIZE), alliedCircle.xy);
	if (sdf <= -band) {
		discard;
	}
	float coverage = engineCoverage(off);
	// past the disc the perimeter's terrain shadow carries on outward, as the sheet's circleCoverage does
	float dist = length(vec2(off));
	if (coverage <= 0.0 && dist > losParams.z) {
		coverage = engineCoverage(ivec2(floor(vec2(off) * (max(losParams.z - 0.75, 0.0) / dist) + 0.5)));
	}
	if (coverage <= 0.0) {
		discard;
	}
	// level 1.. (0 = no radar) above the index: MAX blending keeps the largest distance, and the packed value stays an
	// integer below 2^24, exact in the R32F target
	float slots = float(ALLIED_CIRCLE_SLOTS);
	float levels = 16777216.0 / slots;
	float level = 1.0 + floor(clamp(sdf / (2.0 * band) + 0.5, 0.0, 1.0) * (levels - 2.0) + 0.5);
	float entry = level * slots + alliedCircle.w;
	if (alliedCirclePass > 1.5 && texelFetch(alliedWinnerTex, ivec2(gl_FragCoord.xy), 0).r == entry) {
		discard;
	}
	fragColor = vec4(entry, 0.0, 0.0, 1.0);
}
)SOURCE";
inline constexpr std::string_view CoverageFragmentSHA512="a25bcfb6b7a8542cc9cdb58b1feb574e14e1846d74d9360e3fc23286d585d4e29538b3dd17c705728f04f65b2cbe74ca37d75d0f8edd8729cb59a3321e7342c6";
inline constexpr std::string_view SmoothFragment=R"SOURCE(#version 420
#line 33000

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview smoothing pass. Eases the displayed coverage towards the freshly computed coverage
// every frame (ping-ponged between two textures), so the sheet fades smoothly while the radar
// is dragged around instead of popping in and out.
// The previous state is read with a texel shift, because when the emitter moves to another cell the
// same texel index refers to a different spot in the world.

//__DEFINES__

uniform sampler2D prevTex;   // previously displayed coverage
uniform sampler2D targetTex; // freshly computed coverage

uniform vec4 smoothParams; // texel shift x, texel shift y, lerp factor, reset flag (1 = ignore prevTex)

out vec4 fragColor;

void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);
	float target = texelFetch(targetTex, texel, 0).r;

	ivec2 prevTexel = texel + ivec2(smoothParams.xy);
	ivec2 prevSize = textureSize(prevTex, 0);
	float prev = 0.0;
	if (smoothParams.w < 0.5 && all(greaterThanEqual(prevTexel, ivec2(0))) && all(lessThan(prevTexel, prevSize))) {
		prev = texelFetch(prevTex, prevTexel, 0).r;
	}

	fragColor = vec4(mix(prev, target, smoothParams.z), 0.0, 0.0, 1.0);
}
)SOURCE";
inline constexpr std::string_view SmoothFragmentSHA512="5f6fecc723655aaca4531e99cac9fb5b2bac735912bc441a4a890b12ec5995f1a256f449b25e3719e5d9a42274d657016273918916b65351528692c28ab8202e";
inline constexpr std::string_view SheetFragment=R"SOURCE(#version 420
#line 20000

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

#extension GL_ARB_shader_storage_buffer_object : require

//__DEFINES__

uniform sampler2D mapDepths;   // $map_gbuffer_zvaltex
#if MODEL_DEPTH_TEST
uniform sampler2D modelDepths; // $model_gbuffer_zvaltex
#endif
uniform sampler2D coverageTex;  // smoothed coverage of the previewed radar, one texel per radar cell of its disc
uniform sampler2D targetTex;    // its exact engine coverage (0/1), same layout
uniform sampler2D radarInfoTex; // allied radar coverage map, R = 1 where any allied radar covers the radar cell (only read when lookupParams.w = 1)
// Circle mode with allied coverage: per radar cell, the allied radar whose range circle reaches farthest past the cell
// among those covering its terrain, and the runner-up (level * ALLIED_CIRCLE_SLOTS + circle index, 0 = none;
// sensor_ranges_radar_preview_coverage.frag.glsl)
uniform sampler2D alliedWinnerTex;
uniform sampler2D alliedRunnerUpTex;
layout(std430, binding = 5) readonly buffer AlliedCircles {
	vec4 alliedCircles[]; // center x, center z, range (elmos) per allied radar
};

uniform vec4 radarcenter_range; // radar x, emitter height, radar z, effective range (elmo)
uniform vec4 lookupParams;      // emitter cell x, emitter cell y, radius in cells, allied coverage on (1) / off (0)
uniform vec4 animParams;        // time (s), seconds since the preview appeared, z = 1: rings and sweep on, w = 1: sweep on
// x = 1: circle mode (SensorPreviewCircles), y = the previewed radar's range (elmos)
uniform vec4 circleParams;

//__ENGINEUNIFORMBUFFERDEFS__

#line 21000

out vec4 fragColor;

const float cellSize = float(RADAR_CELL_SIZE);
const float minCoverage = float(MIN_COVERAGE);
const float spawnSpeed = float(SPAWN_SPEED);
const float alliedAlpha = float(ALLIED_ALPHA);
const vec3 alliedColor = ALLIED_COLOR;
const vec3 outlineColor = OUTLINE_COLOR;
const float outlineWidth = float(OUTLINE_WIDTH); // pixels at 1080p, scaled with the vertical resolution

const float PI = 3.1415927;
const float pulseSymmetric = float(PULSE_SYMMETRIC); // ring profile: 1 = bell (fade in/out), 0 = sharp front, fade out
const vec3 sheetColor = SHEET_COLOR;
const vec3 sheetPulseColor = SHEET_PULSE_COLOR;
const float sheetAlpha = float(SHEET_ALPHA);
const float sheetOutlineAlpha = float(SHEET_OUTLINE_ALPHA);
const float sheetRingStrength = float(SHEET_RING_STRENGTH);
const float pulseSpacing = float(PULSE_SPACING); // elmos between rings
const float pulseSpeed = float(PULSE_SPEED);     // elmos per second the rings travel outward
const float pulsePower = float(PULSE_POWER);     // higher = shorter tail behind a ring's leading edge
const float sweepSpeed = float(SWEEP_SPEED);
const float sweepTrail = max(float(SWEEP_TRAIL), 0.01); // degrees
const float sweepBeam = max(float(SWEEP_BEAM), 0.01);   // degrees
const float sweepStrength = float(SWEEP_STRENGTH);

// coverage of the previewed radar at a radar cell, x exact (0/1) and y smoothed, -1 outside its disc
vec2 previewCoverageAt(ivec2 radarCell) {
	ivec2 texel = radarCell - ivec2(lookupParams.xy) + ivec2(int(lookupParams.z));
	if (any(lessThan(texel, ivec2(0))) || any(greaterThanEqual(texel, textureSize(coverageTex, 0)))) {
		return vec2(-1.0);
	}
	return vec2(texelFetch(targetTex, texel, 0).r, texelFetch(coverageTex, texel, 0).r);
}

// Circle mode: the previewed radar's coverage at a radar cell, with the cells past its disc (between the engine's
// staircase and the range circle) taking the coverage of the cell further in on the line from the emitter, so terrain
// shadows carry on to the circle. The coverage is 0 both off the disc and in shadow, so every empty cell farther out
// than the disc's radius counts as off it; shadowed cells there take their inner neighbour's shadow anyway.
vec2 circleCoverageAt(ivec2 radarCell) {
	vec2 coverage = previewCoverageAt(radarCell);
	vec2 offset = vec2(radarCell - ivec2(lookupParams.xy));
	float dist = length(offset);
	if (coverage.x <= 0.0 && dist > lookupParams.z) {
		ivec2 inner = ivec2(lookupParams.xy) + ivec2(floor(offset * ((lookupParams.z - 0.75) / dist) + 0.5));
		coverage = previewCoverageAt(inner);
	}
	return max(coverage, 0.0);
}

// 1 where an allied radar covers the radar cell (engine radar map or our union), 0 with allied coverage off
float alliedAt(ivec2 radarCell) {
	if (lookupParams.w < 0.5 || any(lessThan(radarCell, ivec2(0))) || any(greaterThanEqual(radarCell, textureSize(radarInfoTex, 0)))) {
		return 0.0;
	}
	return step(0.5, texelFetch(radarInfoTex, radarCell, 0).r);
}

// signed distance (elmos, > 0 inside) from worldXZ to the range circle of the allied radar in a texel of
// alliedWinnerTex / alliedRunnerUpTex
float packedCircleDist(float entry, vec2 worldXZ) {
	vec4 circle = alliedCircles[int(entry) % ALLIED_CIRCLE_SLOTS];
	return circle.z - distance(worldXZ, circle.xy);
}

// circle mode: signed distance (cells) from worldXZ to the range circles of the allied radars covering a radar cell's
// terrain (the two reaching farthest past it, which covers the cells where two circles cross), -1e6 for none
float cellCircleDist(ivec2 radarCell, vec2 worldXZ) {
	if (any(lessThan(radarCell, ivec2(0))) || any(greaterThanEqual(radarCell, textureSize(alliedWinnerTex, 0)))) {
		return -1e6;
	}
	float winner = texelFetch(alliedWinnerTex, radarCell, 0).r;
	if (winner < 0.5) {
		return -1e6;
	}
	float dist = packedCircleDist(winner, worldXZ);
	float runnerUp = texelFetch(alliedRunnerUpTex, radarCell, 0).r;
	if (runnerUp > 0.5) {
		dist = max(dist, packedCircleDist(runnerUp, worldXZ));
	}
	return dist / cellSize;
}

// Signed distance in cells to the local boundary, with convex corners cut back and concave
// corners filled. The half-cell radius limit keeps neighboring corners from overlapping.
float roundedBoundary(float covered, vec3 neighbours, vec2 edgeDist) {
	float radius = float(COVERAGE_CORNER_RADIUS);
	if (covered > 0.5) {
		vec2 sideDist = vec2(neighbours.x < 0.5 ? edgeDist.x : 1e6, neighbours.y < 0.5 ? edgeDist.y : 1e6);
		// The concave arc extends into the neighboring empty cell: its old straight edges
		// must also be replaced on this covered side of the cell boundary.
		if (neighbours.x < 0.5 && neighbours.y > 0.5 && neighbours.z > 0.5 && edgeDist.y < radius) {
			sideDist.x = length(vec2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
		}
		if (neighbours.y < 0.5 && neighbours.x > 0.5 && neighbours.z > 0.5 && edgeDist.x < radius) {
			sideDist.y = length(vec2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
		}
		float sdf = min(sideDist.x, sideDist.y);
		if (neighbours.x < 0.5 && neighbours.y < 0.5 && all(lessThan(edgeDist, vec2(radius)))) {
			sdf = radius - length(edgeDist - radius);
		}
		return sdf;
	}
	vec2 sideDist = vec2(neighbours.x > 0.5 ? edgeDist.x : 1e6, neighbours.y > 0.5 ? edgeDist.y : 1e6);
	// Outside a convex corner, antialias against the arc rather than the removed square edge.
	if (neighbours.x > 0.5 && neighbours.z < 0.5 && edgeDist.y < radius) {
		sideDist.x = length(vec2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
	}
	if (neighbours.y > 0.5 && neighbours.z < 0.5 && edgeDist.x < radius) {
		sideDist.y = length(vec2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
	}
	float sdf = -min(sideDist.x, sideDist.y);
	if (neighbours.x < 0.5 && neighbours.y < 0.5 && neighbours.z > 0.5) {
		sdf = radius - length(edgeDist + radius);
	}
	if (all(greaterThan(neighbours, vec3(0.5))) && all(lessThan(edgeDist, vec2(radius)))) {
		sdf = length(edgeDist - radius) - radius;
	}
	return sdf;
}

// x exact (0/1), y smoothed
vec2 boundaryCoverageAt(ivec2 cell, bool allied) {
	if (allied) {
		return vec2(alliedAt(cell));
	}
	return max(circleParams.x > 0.5 ? circleCoverageAt(cell) : previewCoverageAt(cell), 0.0);
}

// The rounded cells of the previewed radar's or the allied coverage at a pixel, shaped by the exact coverage so they move
// with the preview at once: returns the smoothed coverage of its cell (in an uncovered cell, that of its covered
// neighbours, which a filled concave corner borrows), with sdf the signed distance to their boundary in cells (> 0 inside)
float roundedCells(vec2 coord, bool allied, out float sdf) {
	ivec2 cell = ivec2(floor(coord));
	vec2 inCell = fract(coord);
	ivec2 direction = ivec2(step(vec2(0.5), inCell)) * 2 - 1;
	vec2 center = boundaryCoverageAt(cell, allied);
	vec2 nx = boundaryCoverageAt(cell + ivec2(direction.x, 0), allied);
	vec2 nz = boundaryCoverageAt(cell + ivec2(0, direction.y), allied);
	vec2 nd = boundaryCoverageAt(cell + direction, allied);
	vec3 neighbours = step(vec3(0.5), vec3(nx.x, nz.x, nd.x));
	sdf = roundedBoundary(step(0.5, center.x), neighbours, 0.5 - abs(inCell - 0.5));
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
float alliedCircleCells(vec2 coord, vec2 worldXZ, float reach) {
	ivec2 cell = ivec2(floor(coord));
	vec2 inCell = fract(coord);
	vec2 edgeDist = 0.5 - abs(inCell - 0.5);
	float own = cellCircleDist(cell, worldXZ);
	if (all(greaterThan(edgeDist, vec2(reach)))) {
		return (own > -1e5) ? min(own, min(edgeDist.x, edgeDist.y)) : -1e6;
	}
	ivec2 direction = ivec2(step(vec2(0.5), inCell)) * 2 - 1;
	vec3 neighbourDist = vec3(
		cellCircleDist(cell + ivec2(direction.x, 0), worldXZ),
		cellCircleDist(cell + ivec2(0, direction.y), worldXZ),
		cellCircleDist(cell + direction, worldXZ));
	vec3 neighbours = vec3(greaterThan(neighbourDist, vec3(0.0)));
	float sdf = min(roundedBoundary(0.0, neighbours, edgeDist), max(neighbourDist.x, max(neighbourDist.y, neighbourDist.z)));
	if (own > -1e5) {
		sdf = max(sdf, min(roundedBoundary(1.0, neighbours, edgeDist), own));
	}
	return sdf;
}

// rings travelling outward from the radar (a bright leading edge with a tail fading inward) plus the rotating
// sweep, evaluated per pixel so the sheet shows them as continuous gradients rather than per-cell steps
float sheetGlow(vec2 fromCenter, float time) {
	float dist = length(fromCenter);
	float phase = fract((dist - time * pulseSpeed) / pulseSpacing); // 0 at a ring's center, 1 at the next ring
	// PULSE_SYMMETRIC: smooth bell that fades in and out around the ring, or a sharp front fading out behind it
	float ring = (pulseSymmetric > 0.5) ? pow(0.5 + 0.5 * cos(phase * 2.0 * PI), pulsePower) : pow(1.0 - phase, pulsePower);
	float glow = ring * sheetRingStrength;
	if (animParams.w > 0.5) { // RadarPreviewSweep
		float angle = atan(fromCenter.y, fromCenter.x) / (2.0 * PI) + 0.5;
		float behind = (1.0 - fract(angle - time * sweepSpeed)) * 360.0; // degrees behind the sweep's leading edge
		float trail = clamp(1.0 - behind / sweepTrail, 0.0, 1.0);
		glow += (trail * trail + (1.0 - smoothstep(0.0, sweepBeam, behind))) * sweepStrength;
	}
	return clamp(glow, 0.0, 1.0);
}

void main() {
	vec2 screenUV = (gl_FragCoord.xy - viewGeometry.zw) / viewGeometry.xy;
	float mapDepth = texture(mapDepths, screenUV).x;
	vec4 world = cameraViewProjInv * vec4(screenUV * 2.0 - 1.0, mapDepth, 1.0);
	vec3 worldPos = world.xyz / world.w;
	// over water the coverage lies on the water surface, where the view ray crosses y = 0
	vec3 camPos = cameraViewInv[3].xyz;
	if (worldPos.y < 0.0 && camPos.y > 0.0) {
		worldPos = mix(camPos, worldPos, camPos.y / (camPos.y - worldPos.y));
	}
	vec2 cellCoord = worldPos.xz / cellSize; // position in radar cells
	vec2 cellPixels = fwidth(cellCoord); // radar cells per pixel, taken before any discard

	if (mapDepth >= 0.999999) {
		discard; // sky
	}

	ivec2 cell = ivec2(floor(cellCoord));
	bool circles = circleParams.x > 0.5;
	float pixelCells = max(max(cellPixels.x, cellPixels.y), 1e-5);
	// circle mode: the previewed radar's coverage ends at a circle at its range around it
	float circleSdf = circleParams.y - distance(worldPos.xz, radarcenter_range.xz);
	if (lookupParams.w < 0.5 && (circles ? circleSdf < -pixelCells * cellSize : previewCoverageAt(cell).x < 0.0)) {
		discard;
	}

	// The previewed radar's coverage and, with allied coverage on (lookupParams.w), that of all allied radars: each
	// its rounded cells, in circle mode clipped to the range circles, antialiased along the signed distance (cells)
	float ownBoundarySdf;
	float ownCoverage = roundedCells(cellCoord, false, ownBoundarySdf);
	if (circles) {
		ownBoundarySdf = min(ownBoundarySdf, circleSdf / cellSize);
	}
	ownCoverage *= clamp(ownBoundarySdf / pixelCells + 0.5, 0.0, 1.0);
	float ownWeight = smoothstep(0.0, 0.5, ownCoverage);
	float coverage = ownCoverage;

	// cells covered only by other allied radars are drawn too but stay static; the previewed radar's animation
	// applies in proportion to its own coverage
	float width = pixelCells * outlineWidth * viewGeometry.y / 1080.0; // outline width in cells
	float weight = 1.0;
	float alliedBoundarySdf = -1e6;
	float alliedCoverage = 0.0;
	if (lookupParams.w > 0.5) {
		weight = ownWeight;
		if (circles) {
			alliedBoundarySdf = alliedCircleCells(cellCoord, worldPos.xz, max(float(COVERAGE_CORNER_RADIUS), width + pixelCells));
			alliedCoverage = 1.0;
		} else {
			alliedCoverage = roundedCells(cellCoord, true, alliedBoundarySdf);
		}
		alliedCoverage *= clamp(alliedBoundarySdf / pixelCells + 0.5, 0.0, 1.0);
		coverage = max(coverage, alliedCoverage);
	}

	// spawn ripple: the sheet spreads outward from the radar when the preview appears; cells of other
	// allied radars simply fade in
	vec2 cellXZ = (vec2(cell) + 0.5) * cellSize;
	float distN = length(cellXZ - radarcenter_range.xz) / radarcenter_range.w;
	float front = animParams.y * spawnSpeed;
	float spawn = mix(min(animParams.y * 4.0, 1.0), smoothstep(distN - 0.10, distN + 0.02, front), weight);

	if (coverage < minCoverage || spawn < 0.01) {
		discard;
	}
#if MODEL_DEPTH_TEST
	if (texture(modelDepths, screenUV).x < mapDepth) {
		discard; // a unit or feature in front of the terrain
	}
#endif
	float fade = spawn * smoothstep(0.0, 0.5, coverage) * mix(alliedAlpha, 1.0, weight);

	// outline along the previewed radar's own border (inside allied coverage too) and along the outer border of all
	// coverage; where they coincide they are the same line. None where a pixel spans several cells (far out, or where
	// the position jumps between pixels at terrain silhouettes).
	float outerSdf = max(ownCoverage >= minCoverage ? ownBoundarySdf : -1e6, alliedCoverage >= minCoverage ? alliedBoundarySdf : -1e6);
	float ownLine = (1.0 - smoothstep(0.0, width, ownBoundarySdf)) * ownWeight;
	float outline = pixelCells < 4.0 ? max(ownLine, 1.0 - smoothstep(0.0, width, outerSdf)) : 0.0;

	// only the previewed radar's own coverage animates (weight), cells of other allied radars stay still
	float glow = 0.0;
	if (weight > 0.0 && animParams.z > 0.5) {
		glow = sheetGlow(worldPos.xz - radarcenter_range.xz, animParams.x) * weight;
	}
	// allied-only cells muted, the rings blend it all the way to the pulse color
	vec3 tint = mix(alliedColor, sheetColor, weight);
	vec3 fillColor = mix(tint, sheetPulseColor, glow);
	float fillAlpha = min(sheetAlpha * (1.0 + glow), 1.0);
	fragColor = vec4(mix(fillColor, outlineColor, outline), mix(fillAlpha, sheetOutlineAlpha, outline) * fade);
}
)SOURCE";
inline constexpr std::string_view SheetFragmentSHA512="e1e1d82d608462005aa552690348abfac158a757baad3e4c79dc6b0a168b87b8447edc9a41930f33e1332051755ff13c2db3de90341fde904754a90f5966c49d";
inline constexpr std::string_view MinimapVertex=R"SOURCE(#version 420
#line 34000

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.

// Radar preview minimap pass: a quad over the whole minimap (DrawInMiniMap), the fragment shader
// maps every minimap pixel to its radar cell. Written straight to clip space like the other GL4
// minimap overlays (buildsquare, start polygons); the PIP minimap remaps the viewport for those.

//__DEFINES__

layout (location = 0) in vec4 pos; // xy = clip space position, zw = uv (unused)

out vec2 minimapUV; // 0..1 across the minimap, y up

void main() {
	gl_Position = vec4(pos.xy, 0.0, 1.0);
	minimapUV = pos.xy * 0.5 + 0.5;
	// The engine enables four user clip planes around the Lua minimap draw (CMiniMap::SetClipPlanes), set up
	// for its fixed-function matrices. A custom vertex shader that leaves them alone gets clipped against
	// gl_Position instead, which cuts the quad down to one quadrant, so declare every vertex inside.
	gl_ClipDistance[0] = 1.0;
	gl_ClipDistance[1] = 1.0;
	gl_ClipDistance[2] = 1.0;
	gl_ClipDistance[3] = 1.0;
}
)SOURCE";
inline constexpr std::string_view MinimapVertexSHA512="7f2d14c73e9416d4be0c45ff84055eed0a1989815fc84e79c7ce0632f9bd2ab00b54b778ee4fe2ba02fa192f5940c2da4d41f4e271bf0f95b8f92136b05e9caf";
inline constexpr std::string_view MinimapFragment=R"SOURCE(#version 420
#line 35000

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

uniform sampler2D coverageTex;  // smoothed coverage of the previewed radar, R = coverage, one texel per radar cell of its disc
uniform sampler2D radarInfoTex; // allied radar coverage map, R = 1 where any allied radar covers the radar cell (only read when discParams.w = 1)
uniform sampler2D targetTex;    // exact engine coverage of the previewed radar (0/1), same layout as coverageTex
uniform vec4 discParams;        // emitter cell x, emitter cell y, radius in cells, allied coverage on (1) / off (0)
uniform vec3 inactiveParams;    // 0 while the previewed emitter works, 1 while it does not (eased in between); its fill and outline opacity then, relative to a working one's. Unset it always works.
uniform vec4 mapParams;         // map width in radar cells, map height in radar cells, minimap rotation (0..3 quarter turns clockwise), outline width in pixels

in vec2 minimapUV;

out vec4 fragColor;

const vec3 sheetColor = SHEET_COLOR;
const vec3 alliedColor = ALLIED_COLOR;
const vec3 outlineColor = OUTLINE_COLOR;
const float alliedAlpha = float(ALLIED_ALPHA);
const float fillAlpha = float(MINIMAP_ALPHA);
const float outlineAlpha = float(MINIMAP_OUTLINE_ALPHA);
const float alliedOutlineAlpha = float(MINIMAP_ALLIED_OUTLINE_ALPHA);

// texel of the previewed radar's disc textures for a radar cell, or (-1, -1) outside the disc
ivec2 discTexel(ivec2 cell) {
	int radius = int(discParams.z);
	ivec2 texel = cell - ivec2(discParams.xy) + ivec2(radius);
	if (any(lessThan(texel, ivec2(0))) || any(greaterThanEqual(texel, ivec2(2 * radius + 1)))) {
		return ivec2(-1);
	}
	return texel;
}

// smoothed coverage of the previewed radar at a radar cell (0 outside its disc)
float smoothedAt(ivec2 cell) {
	ivec2 texel = discTexel(cell);
	return (texel.x < 0) ? 0.0 : texelFetch(coverageTex, texel, 0).r;
}

// exact engine coverage of the previewed radar at a radar cell (0 outside its disc)
float ownAt(ivec2 cell) {
	ivec2 texel = discTexel(cell);
	return (texel.x < 0) ? 0.0 : step(0.5, texelFetch(targetTex, texel, 0).r);
}

// 1 where another allied radar covers the radar cell
float alliedAt(ivec2 cell) {
	if (discParams.w < 0.5 || any(lessThan(cell, ivec2(0))) || any(greaterThanEqual(cell, textureSize(radarInfoTex, 0)))) {
		return 0.0;
	}
	return step(0.5, texelFetch(radarInfoTex, cell, 0).r);
}

float roundedBoundary(float covered, vec3 neighbours, vec2 edgeDist) {
	float radius = float(COVERAGE_CORNER_RADIUS);
	if (covered > 0.5) {
		vec2 sideDist = vec2(neighbours.x < 0.5 ? edgeDist.x : 1e6, neighbours.y < 0.5 ? edgeDist.y : 1e6);
		// The concave arc extends into the neighboring empty cell: its old straight edges
		// must also be replaced on this covered side of the cell boundary.
		if (neighbours.x < 0.5 && neighbours.y > 0.5 && neighbours.z > 0.5 && edgeDist.y < radius) {
			sideDist.x = length(vec2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
		}
		if (neighbours.y < 0.5 && neighbours.x > 0.5 && neighbours.z > 0.5 && edgeDist.x < radius) {
			sideDist.y = length(vec2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
		}
		float sdf = min(sideDist.x, sideDist.y);
		if (neighbours.x < 0.5 && neighbours.y < 0.5 && all(lessThan(edgeDist, vec2(radius)))) {
			sdf = radius - length(edgeDist - radius);
		}
		return sdf;
	}
	vec2 sideDist = vec2(neighbours.x > 0.5 ? edgeDist.x : 1e6, neighbours.y > 0.5 ? edgeDist.y : 1e6);
	// Outside a convex corner, antialias against the arc rather than the removed square edge.
	if (neighbours.x > 0.5 && neighbours.z < 0.5 && edgeDist.y < radius) {
		sideDist.x = length(vec2(edgeDist.x + radius, edgeDist.y - radius)) - radius;
	}
	if (neighbours.y > 0.5 && neighbours.z < 0.5 && edgeDist.x < radius) {
		sideDist.y = length(vec2(edgeDist.x - radius, edgeDist.y + radius)) - radius;
	}
	float sdf = -min(sideDist.x, sideDist.y);
	if (neighbours.x < 0.5 && neighbours.y < 0.5 && neighbours.z > 0.5) {
		sdf = radius - length(edgeDist + radius);
	}
	if (all(greaterThan(neighbours, vec3(0.5))) && all(lessThan(edgeDist, vec2(radius)))) {
		sdf = length(edgeDist - radius) - radius;
	}
	return sdf;
}

void main() {
	// minimap (y up) -> normalized world x/z, the inverse of the world -> minimap mapping the other
	// GL4 minimap overlays use (gui_buildsquare_gl4, map_startpolygon_gl4)
	int rotation = int(mapParams.z);
	vec2 world;
	if (rotation == 0) {
		world = vec2(minimapUV.x, 1.0 - minimapUV.y);
	} else if (rotation == 1) {
		world = vec2(minimapUV.y, minimapUV.x);
	} else if (rotation == 2) {
		world = vec2(1.0 - minimapUV.x, minimapUV.y);
	} else {
		world = vec2(1.0 - minimapUV.y, 1.0 - minimapUV.x);
	}
	vec2 cellPos = world * mapParams.xy; // position in radar cells
	ivec2 cell = ivec2(floor(cellPos));

	float smoothed = smoothedAt(cell);
	float own = ownAt(cell);
	float allied = alliedAt(cell);
	// an inactive preview covers nothing: it is drawn fainter, and allied cells under it show as they are
	float inactive = inactiveParams.x;
	float fill = max(smoothed * mix(1.0, inactiveParams.y, inactive), allied * alliedAlpha);

	// outline: the sides of this cell that border a cell the previewed radar does not cover (its own coverage
	// border, drawn even inside allied coverage) or that nobody covers. fwidth gives the cell size in pixels
	// for any minimap size, rotation or PIP zoom.
	vec2 inCell = fract(cellPos);
	vec2 nearSide = step(vec2(0.5), inCell); // 0 = the -x/-z side is nearer, 1 = the +x/+z side
	vec2 edgeDist = 0.5 - abs(inCell - 0.5); // distance to the nearer side, in cells
	ivec2 nx = cell + ivec2(int(nearSide.x) * 2 - 1, 0);
	ivec2 nz = cell + ivec2(0, int(nearSide.y) * 2 - 1);
	ivec2 nd = ivec2(nx.x, nz.y); // diagonally across the nearer corner
	vec3 ownN = vec3(ownAt(nx), ownAt(nz), ownAt(nd));
	vec3 alliedN = vec3(alliedAt(nx), alliedAt(nz), alliedAt(nd));
	float covered = 1.0 - inactive * (1.0 - allied);
	vec3 side = max(own * (1.0 - ownN), covered * (1.0 - max(ownN * (1.0 - inactive), alliedN))); // x side, z side, diagonal
	vec2 px = max(fwidth(cellPos), vec2(1e-5)) * mapParams.w;
	vec2 edge = 1.0 - smoothstep(vec2(0.0), px, edgeDist);
	// at a concave corner only the diagonal cell is outside: this cell's corner square closes the outline there
	float corner = (1.0 - side.x) * (1.0 - side.y) * side.z * min(edge.x, edge.y);
	// only covered cells are outlined; a cell the previewed radar just started covering fades its outline in
	// with its fill. Allied-only cells use their own outline opacity.
	float previewWeight = own * smoothstep(0.0, 0.5, smoothed);
	float outline = max(max(side.x * edge.x, side.y * edge.y), corner) * max(previewWeight, allied);
	float outlineA = mix(alliedOutlineAlpha, outlineAlpha * mix(1.0, inactiveParams.z, inactive), previewWeight);

	float boundaryMask = 1.0;
	if (float(COVERAGE_CORNER_RADIUS) > 0.0) {
		float ownSdf = roundedBoundary(own, ownN, edgeDist);
		float alliedSdf = roundedBoundary(allied, alliedN, edgeDist);
		float pixelCells = max(max(fwidth(cellPos).x, fwidth(cellPos).y), 1e-5);
		float ownMask = clamp(ownSdf / pixelCells + 0.5, 0.0, 1.0);
		float alliedMask = clamp(alliedSdf / pixelCells + 0.5, 0.0, 1.0);
		// Concave rounding borrows the neighboring cells' fade rather than appearing fully opaque.
		float ownStrength = own > 0.5 ? smoothed : max(smoothedAt(nx), max(smoothedAt(nz), smoothedAt(nd)));
		smoothed = ownStrength * ownMask;
		allied = max(allied, max(alliedN.x, max(alliedN.y, alliedN.z))) * alliedMask;
		fill = max(smoothed * mix(1.0, inactiveParams.y, inactive), allied * alliedAlpha);
		previewWeight = ownMask * smoothstep(0.0, 0.5, smoothed);
		// the previewed radar's own border (inside allied coverage too) and the outer border of all coverage
		float width = pixelCells * mapParams.w;
		float outerSdf = max(smoothed > 0.00001 ? ownSdf : -1e6, allied > 0.00001 ? alliedSdf : -1e6);
		float ownLine = (1.0 - smoothstep(0.0, width, ownSdf)) * previewWeight;
		outline = max(ownLine, (1.0 - smoothstep(0.0, width, outerSdf)) * max(previewWeight, allied));
		outlineA = mix(alliedOutlineAlpha, outlineAlpha * mix(1.0, inactiveParams.z, inactive), previewWeight);
		boundaryMask = max(ownMask * step(0.00001, smoothed), alliedMask * step(0.00001, allied));
	}

	float alpha = mix(fill * fillAlpha, outlineA, outline);
	alpha *= boundaryMask;
	if (alpha < 0.002) {
		discard;
	}
	vec3 color = mix(alliedColor, sheetColor, smoothed * (1.0 - inactive * allied));
	fragColor = vec4(mix(color, outlineColor, outline), alpha);
}
)SOURCE";
inline constexpr std::string_view MinimapFragmentSHA512="bd13f21d2439d813fbf6add015c209f4edf52990e5be5501f52068181fc5a71a02ad61c973e902303b0ef479a55bd533b015f2d04065944924d7dbe2a9d44757";
inline constexpr std::string_view EngineDefinitions=R"SOURCE(layout(std140, binding = 0) uniform UniformMatrixBuffer {
	mat4 screenView;
	mat4 screenProj;
	mat4 screenViewProj;

	mat4 cameraView;
	mat4 cameraProj;
	mat4 cameraViewProj;
	mat4 cameraBillboardView;

	mat4 cameraViewInv;
	mat4 cameraProjInv;
	mat4 cameraViewProjInv;

	mat4 shadowView;
	mat4 shadowProj;
	mat4 shadowViewProj;

	mat4 reflectionView;
	mat4 reflectionProj;
	mat4 reflectionViewProj;

	mat4 orthoProj01;

	// transforms for [0] := Draw, [1] := DrawInMiniMap, [2] := Lua DrawInMiniMap
	mat4 mmDrawView; //world to MM
	mat4 mmDrawProj; //world to MM
	mat4 mmDrawViewProj; //world to MM

	mat4 mmDrawIMMView; //heightmap to MM
	mat4 mmDrawIMMProj; //heightmap to MM
	mat4 mmDrawIMMViewProj; //heightmap to MM

	mat4 mmDrawDimView; //mm dims
	mat4 mmDrawDimProj; //mm dims
	mat4 mmDrawDimViewProj; //mm dims
};

layout(std140, binding = 1) uniform UniformParamsBuffer {
	vec3 rndVec3; //new every draw frame.
	uint renderCaps; //various render booleans

	vec4 timeInfo; //gameFrame, drawSeconds, interpolated(unsynced)GameSeconds(synced), frameTimeOffset
	vec4 viewGeometry; //vsx, vsy, vpx, vpy
	vec4 mapSize; //xz, xzPO2
	vec4 mapHeight; //height minCur, maxCur, minInit, maxInit

	vec4 fogColor; //fog color
	vec4 fogParams; //fog {start, end, 0.0, scale}

	vec4 sunDir; // (sky != nullptr) ? sky->GetLight()->GetLightDir() : float4(/*map default*/ 0.0f, 0.447214f, 0.894427f, 1.0f);

	vec4 sunAmbientModel;
	vec4 sunAmbientMap;
	vec4 sunDiffuseModel;
	vec4 sunDiffuseMap;
	vec4 sunSpecularModel; // float4{ sunLighting->modelSpecularColor.xyz, sunLighting->specularExponent };
	vec4 sunSpecularMap; //  float4{ sunLighting->groundSpecularColor.xyz, sunLighting->specularExponent };

	vec4 shadowDensity; //  float4{ sunLighting->groundShadowDensity, sunLighting->modelShadowDensity, 0.0, 0.0 };

	vec4 windInfo; // windx, windy, windz, windStrength
	vec2 mouseScreenPos; //x, y. Screen space.
	uint mouseStatus; // bits 0th to 32th: LMB, MMB, RMB, offscreen, mmbScroll, locked
	uint mouseUnused;
	vec4 mouseWorldPos; //x,y,z; w=0 -- offmap. Ignores water, doesn't ignore units/features under the mouse cursor

	vec4 teamColor[255]; //all team colors
};

// glsl rotate convenience funcs: https://github.com/dmnsgn/glsl-rotate

mat3 rotation3dX(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return mat3(
		1.0, 0.0, 0.0,
		0.0, c, s,
		0.0, -s, c
	);
}

mat3 rotation3dY(float a) {
	float s = sin(a);
	float c = cos(a);

  return mat3(
    c, 0.0, -s,
    0.0, 1.0, 0.0,
    s, 0.0, c);
}

mat3 rotation3dZ(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return mat3(
		c, s, 0.0,
		-s, c, 0.0,
		0.0, 0.0, 1.0
	);
}

mat4 scaleMat(vec3 s) {
	return mat4(
		s.x, 0.0, 0.0, 0.0,
		0.0, s.y, 0.0, 0.0,
		0.0, 0.0, s.z, 0.0,
		0.0, 0.0, 0.0, 1.0
	);
}

mat4 translationMat(vec3 t) {
	return mat4(
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		t.x, t.y, t.z, 1.0
	);
}

mat4 mat4mix(mat4 a, mat4 b, float alpha) {
	return (a * (1.0 - alpha) + b * alpha);
}

// Additional helper functions useful in Spring

vec2 heightmapUVatWorldPos(vec2 worldpos){
	vec2 inverseMapSize = vec2(1.0) / mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	vec2 heightmaptexel = vec2(8.0, 8.0);
	worldpos +=  vec2(-8.0, -8.0) * (worldpos * inverseMapSize) + vec2(4.0, 4.0) ;
	vec2 uvhm = clamp(worldpos, heightmaptexel, mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;
	return uvhm;
}

// This does 'mirror' style tiling of UVs like the way the map edge extension works
vec2 heightmapUVatWorldPosMirrored(vec2 worldpos) {
	vec2 inverseMapSize = vec2(1.0) / mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	vec2 heightmaptexel = vec2(8.0, 8.0);
	worldpos +=  vec2(-8.0, -8.0) * (worldpos * inverseMapSize) + vec2(4.0, 4.0) ;
	vec2 uvhm = worldpos * inverseMapSize;

	return abs(fract(uvhm * 0.5 + 0.5) - 0.5) * 2.0;
}

//  SphereInViewSignedDistance
//  Signed distance from a world-space sphere to the X-Y NDC box.
//  Z (near/far) is ignored. Very reliable, reasonably optimized.
//  Returns:
//      < 0  – sphere at least partially inside the X-Y clip box
//      = 0  – sphere exactly touches at least one edge
//      > 0  – sphere is more distance from the edge of frustum by that many NDC units
//
float SphereInViewSignedDistance(vec3 centerWS,  float radiusWS)
{
    // 1.  centre → clip space
    vec4 clipC = cameraViewProj * vec4(centerWS, 1.0);
    if (clipC.w <= 0.0)           // behind the eye? treat as inside
        return -1.0;

    // 2.  NDC centre (one perspective divide, reused later)
    vec2  ndc   = clipC.xy / clipC.w;        // = clipC.xy / clipC.w

    // 3.  Project world-space +X displacement
    //     M * (p + (r,0,0,0)) = (M*p) + r*M*Xcol
    vec4 deltaClip = radiusWS * cameraViewProj[0];   // first column of matrix
    vec4 clipX     = clipC + deltaClip;

    // 4.  Radius in NDC (second perspective divide only once)
    float ndcRadius = length((clipX.xy / clipX.w) - ndc);

    // 5.  Four signed distances, keep the worst (Chebyshev / max)
    float dLeft   = -(ndc.x + 1.0) - ndcRadius;
    float dRight  =  (ndc.x - 1.0) - ndcRadius;
    float dBottom = -(ndc.y + 1.0) - ndcRadius;
    float dTop    =  (ndc.y - 1.0) - ndcRadius;

    return max(max(dLeft, dRight), max(dBottom, dTop));
}



// Note that this function does not check the Z or depth of the clip space, but in regular springrts top-down views, this isn't needed either.
// the radius to cameradist ratio is a good proxy for visibility in the XY plane
bool isSphereVisibleXY(vec4 wP, float wR){ //worldPos, worldRadius
	vec3 ToCamera = wP.xyz - cameraViewInv[3].xyz; // vector from worldpos to camera
	float isqrtDistRatio = wR * inversesqrt(dot(ToCamera, ToCamera)); // calculate the relative screen-space size of it
	vec4 cWPpos = cameraViewProj * wP; // transform the worldpos into clip space
	vec2 clipVec = cWPpos.ww * (1.0 + isqrtDistRatio); // normalize the clip tolerance
	return any(greaterThan(abs(cWPpos.xy), clipVec)); // check if the clip space coords lie outside of the tolerance relaxed [-1.0, 1.0] space
}

/*
vec3 hsv2rgb(vec3 c){
	vec4 K=vec4(1.,2./3.,1./3.,3.);
	return c.z*mix(K.xxx,saturate(abs(fract(c.x+K.xyz)*6.-K.w)-K.x),c.y);
}

vec3 rgb2hsv(vec3 c){
	vec4 K=vec4(0.,-1./3.,2./3.,-1.);
	vec4 p=mix(vec4(c.bg ,K.wz),vec4(c.gb,K.xy ),step(c.b,c.g));
	vec4 q=mix(vec4(p.xyw,c.r ),vec4(c.r ,p.yzx),step(p.x,c.r));
	float d=q.x-min(q.w,q.y);
	float e=1e-10;
	return vec3(abs(q.z+(q.w-q.y)/(6.*d+e)),d/(q.x+e),q.x);
}
*/

)SOURCE";
struct ArchiveSource {std::string_view path,sha512;std::uint64_t hash;};
inline constexpr std::array<ArchiveSource,15> ArchiveSources{{
 {"LuaUI/Widgets/gui_sensor_ranges_radar_preview.lua","482b93ed84d20a9a13771c644d1db8a7a7168491cfeca1f0323482bc7f7959ccc5820a1cc70bcd127d5412cbfac160aa16d5e306652be5795c97751403962605",4768958046131214049ull},
 {"modules/graphics/luashader.lua","81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436",15173167305575988042ull},
 {"modules/graphics/instancevbotable.lua","bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256",10155353677033079655ull},
 {"gamedata/modrules.lua","ed3ea9e3030e1d4eb79aa5ab4cf49a0283858f454b4c83fb108ce676a21290bb71ba5604193d9d2c6494bd58443ca44f7a2b954bda9ec39c5697bc18c1786aa5",11047447410957178612ull},
 {"LuaUI/Include/minimap_utils.lua","3e42ade6dbebc6413054d698731d9a3684953b54c0239165667aa53dcd679e19e03b40cd37d771e69f180738533e20aacf64ef279fb03ba5bde6f6bd3bd9d9dc",1654698509267778146ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_pass.vert.glsl","254e77f81bd14c68e95805648d479e8a83c04f4304b6b578a118188c2f4e4e223427ae9baeb5ab21e6ecda044b15d8befca024113fe3766d4ed8279af238aad6",2898038703386481647ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_mip.frag.glsl","b7fd9118d6b328dd9da185f54f95b467f8433caffbb27acb183edfa921cdb4028e85a979503c648019089150821dd72aa4057d8e4f3fa5147658dfef127b515e",1120130191407205315ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_coverage.frag.glsl","a25bcfb6b7a8542cc9cdb58b1feb574e14e1846d74d9360e3fc23286d585d4e29538b3dd17c705728f04f65b2cbe74ca37d75d0f8edd8729cb59a3321e7342c6",4397610373452719608ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_smooth.frag.glsl","5f6fecc723655aaca4531e99cac9fb5b2bac735912bc441a4a890b12ec5995f1a256f449b25e3719e5d9a42274d657016273918916b65351528692c28ab8202e",3490483154148430844ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview.frag.glsl","e1e1d82d608462005aa552690348abfac158a757baad3e4c79dc6b0a168b87b8447edc9a41930f33e1332051755ff13c2db3de90341fde904754a90f5966c49d",1754498232522405061ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_minimap.vert.glsl","7f2d14c73e9416d4be0c45ff84055eed0a1989815fc84e79c7ce0632f9bd2ab00b54b778ee4fe2ba02fa192f5940c2da4d41f4e271bf0f95b8f92136b05e9caf",9257543663060489290ull},
 {"LuaUI/Shaders/sensor_ranges_radar_preview_minimap.frag.glsl","bd13f21d2439d813fbf6add015c209f4edf52990e5be5501f52068181fc5a71a02ad61c973e902303b0ef479a55bd533b015f2d04065944924d7dbe2a9d44757",13953599862799782695ull},
 {"LuaUI/Widgets/gui_sensor_ranges_jammer_preview.lua","1b3cbb82c2ec2ce5f71298731869e3e7098a6c84d2959581092cbfcb48131fa84f3416adc7082709e27a9acd98a6e30eab137fbb6c4e3e4aea8da4f7a6521815",1474220737286453364ull},
 {"LuaUI/Shaders/sensor_ranges_jammer_preview_disc.frag.glsl","bdb7558fd4762a3738f9877e00e469d82d184f150a27a2ac2cc2ff96080225b4c70ceed1e95a5603264de67756c2d54c11041d0af6795ff5c961ed070ef6bb60",4542595179422259947ull},
 {"LuaUI/Shaders/sensor_ranges_jammer_preview.frag.glsl","b10db7ac9db0b8a58ade0bc399a361842d050e0ce2ecd780c4ad7b3e53003f0f7c6bbc26459e155cbf117dc31a59923ea9163b1ed89a6026762846a1e3a408b0",5370664342548113593ull},
}};
}
