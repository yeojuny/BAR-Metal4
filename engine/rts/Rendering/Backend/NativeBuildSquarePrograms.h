/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <string_view>
#include <cstdint>
namespace rendering::buildSquareProgram {
inline constexpr std::string_view Vertex=R"original(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require

layout (location = 0) in vec2 a_cornerPos;
layout (location = 1) in vec4 a_cellData; // x, z, quad width, quad height (world units)
layout (location = 2) in vec4 a_color;
layout (location = 3) in vec4 a_outlineColor;
layout (location = 4) in vec4 a_cellExtra; // floatOnWater, mode (0 cell, 1 simplified, 2 merged block), external edge mask, packed footprint data
layout (location = 5) in vec2 a_footprintSize; // footprint size in cells; merged blocks measure corner radius and outline relative to it

//__ENGINEUNIFORMBUFFERDEFS__

out vec4 v_color;
out vec4 v_outlineColor;
flat out float v_footprintEdges;
flat out float v_footprintValid;
flat out float v_queuedFootprintConflict;
flat out float v_mode;
flat out float v_externalEdges;
flat out vec2 v_quadUnits;
flat out vec2 v_cellToUnit;
flat out float v_simplifiedSizeScale;
out vec2 v_cellUV;

uniform sampler2D heightmapTex;
uniform float heightOffset;
uniform float waterLevel;
uniform float cellInset;
uniform float cellSize;
uniform float simplifiedReferenceCells;
uniform float simplifiedSizeFalloff;
uniform float minimumScreenDiameter;
uniform int isMiniMap;
uniform int rotationMiniMap;

vec2 heightmapUVatWorldPos(vec2 worldpos) {
	vec2 inverseMapSize = vec2(1.0) / mapSize.xy;
	vec2 heightmaptexel = vec2(8.0, 8.0);
	worldpos += vec2(-8.0, -8.0) * (worldpos * inverseMapSize) + vec2(4.0, 4.0);
	vec2 uvhm = clamp(worldpos, heightmaptexel, mapSize.xy - heightmaptexel);
	return uvhm * inverseMapSize;
}

void main() {
	float mode = a_cellExtra.y;
	bool simplified = mode > 0.5 && mode < 1.5;
	bool merged = mode > 1.5;
	vec2 quadSize = a_cellData.zw;
	vec2 cellUV = a_cornerPos / cellSize;
	// Merged blocks are only inset on edges that are not shared with a same-styled neighbour block.
	vec2 insetMin = vec2(cellInset);
	vec2 insetMax = vec2(cellInset);
	if (merged) {
		float externalEdges = a_cellExtra.z;
		insetMin.x *= mod(floor(externalEdges), 2.0);
		insetMax.x *= mod(floor(externalEdges / 2.0), 2.0);
		insetMin.y *= mod(floor(externalEdges / 4.0), 2.0);
		insetMax.y *= mod(floor(externalEdges / 8.0), 2.0);
	}
	vec2 insetQuadSize = quadSize - insetMin - insetMax;
	vec2 insetCorner = insetMin + cellUV * insetQuadSize;
	float wx = a_cellData.x + insetCorner.x;
	float wz = a_cellData.y + insetCorner.y;

	v_color = a_color;
	v_outlineColor = a_outlineColor;
	float packedFootprintData = a_cellExtra.w;
	v_footprintEdges = mod(packedFootprintData, 16.0);
	v_footprintValid = step(15.5, mod(packedFootprintData, 32.0));
	v_queuedFootprintConflict = step(31.5, packedFootprintData);
	v_mode = mode;
	v_externalEdges = a_cellExtra.z;
	// Merged blocks measure distances relative to the inset footprint, exactly like the simplified quad of the
	// whole footprint would, so a fully placeable footprint looks identical to the simplified rendering.
	vec2 quadUnits = vec2(1.0);
	vec2 cellToUnit = vec2(1.0);
	float simplifiedSizeScale = 1.0;
	if (simplified || merged) {
		vec2 footprintInsetSize = max(a_footprintSize * cellSize - 2.0 * cellInset, vec2(0.001));
		// Corner radius and outline width grow with the footprint but with diminishing returns: unchanged at the
		// reference size, scaled by (reference / footprint) ^ falloff elsewhere (falloff 0 = proportional, 1 = constant).
		float referenceInsetSize = max(simplifiedReferenceCells * cellSize - 2.0 * cellInset, 0.001);
		simplifiedSizeScale = pow(referenceInsetSize / min(footprintInsetSize.x, footprintInsetSize.y), simplifiedSizeFalloff);
		if (merged) {
			quadUnits = insetQuadSize / footprintInsetSize;
			cellToUnit = (cellSize - 2.0 * cellInset) / footprintInsetSize;
		}
	}
	v_quadUnits = quadUnits;
	v_cellToUnit = cellToUnit;
	v_simplifiedSizeScale = simplifiedSizeScale;
	v_cellUV = cellUV;
	if (isMiniMap == 0) {
		vec2 uvhm = heightmapUVatWorldPos(vec2(wx, wz));
		float wy = textureLod(heightmapTex, uvhm, 0.0).x;
		wy = mix(wy, max(wy, waterLevel), a_cellExtra.x) + heightOffset;
		vec4 clipPosition = cameraViewProj * vec4(wx, wy, wz, 1.0);
		if (simplified) {
			vec2 centerWorldPos = a_cellData.xy + quadSize * 0.5;
			vec4 centerClipPosition = cameraViewProj * vec4(centerWorldPos.x, wy, centerWorldPos.y, 1.0);
			vec2 centerNdcPosition = centerClipPosition.xy / centerClipPosition.w;
			vec2 cornerNdcOffset = clipPosition.xy / clipPosition.w - centerNdcPosition;
			float halfDiagonalPixels = length(cornerNdcOffset * viewGeometry.xy * 0.5);
			float minimumHalfDiagonal = minimumScreenDiameter * 0.5;
			float expansion = max(1.0, minimumHalfDiagonal / max(halfDiagonalPixels, 0.001));
			clipPosition.xy = (centerNdcPosition + cornerNdcOffset * expansion) * clipPosition.w;
		}
		gl_Position = clipPosition;
	} else {
		vec2 ndcxy = vec2(wx, wz) / mapSize.xy * 2.0 - 1.0;
		if (rotationMiniMap == 0) {
			ndcxy.y *= -1.0;
		} else if (rotationMiniMap == 1) {
			ndcxy.xy = ndcxy.yx;
		} else if (rotationMiniMap == 2) {
			ndcxy.x *= -1.0;
		} else {
			ndcxy.xy = -ndcxy.yx;
		}
		gl_Position = vec4(ndcxy, 0.0, 1.0);
	}
}
)original";
inline constexpr std::string_view Fragment=R"original(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require

//__ENGINEUNIFORMBUFFERDEFS__

in vec4 v_color;
in vec4 v_outlineColor;
flat in float v_footprintEdges;
flat in float v_footprintValid;
flat in float v_queuedFootprintConflict;
flat in float v_mode;
flat in float v_externalEdges;
flat in vec2 v_quadUnits;
flat in vec2 v_cellToUnit;
flat in float v_simplifiedSizeScale;
in vec2 v_cellUV;
out vec4 fragColor;

uniform float simplifiedOutlineScale;
uniform float simplifiedCornerRadiusScale;
uniform float cornerRadius;
uniform float cellInset;
uniform float cellSize;
uniform float footprintBoundaryEnabled;
uniform float footprintBoundaryWidth;
uniform float showInvalidFootprintBoundary;
uniform vec4 invalidFootprintBoundaryColor;
uniform float alphaMultiplier;

void main() {
	bool merged = v_mode > 1.5;
	// Simplified quads and merged blocks share the same corner radius and outline width (relative to the footprint).
	float simplifiedMix = (v_mode > 0.5) ? 1.0 : 0.0;
	float scaledCornerRadius = min(
		cornerRadius * mix(1.0, simplifiedCornerRadiusScale * v_simplifiedSizeScale, simplifiedMix),
		0.5
	);
	float outlineWidth = 0.045 * mix(1.0, simplifiedOutlineScale * v_simplifiedSizeScale, simplifiedMix);
	// Distances are measured in inset-quad units for cells and simplified quads (the quad is one unit) and in
	// inset-footprint units for merged blocks (v_quadUnits = block size relative to the footprint), whose edges
	// shared with a same-styled neighbour are ignored so adjacent blocks join seamlessly.
	vec2 quadUnits = merged ? v_quadUnits : vec2(1.0);
	vec2 quadPos = v_cellUV * quadUnits;
	float distanceToEdge;
	if (merged) {
		// Shared edges are pushed far away. Use exact selects, never mix() with the large constant: in float
		// precision that quantises the distances and turns the corners into staircases.
		float farAway = 1.0e5;
		bool leftExternal = mod(floor(v_externalEdges), 2.0) > 0.5;
		bool rightExternal = mod(floor(v_externalEdges / 2.0), 2.0) > 0.5;
		bool topExternal = mod(floor(v_externalEdges / 4.0), 2.0) > 0.5;
		bool bottomExternal = mod(floor(v_externalEdges / 8.0), 2.0) > 0.5;
		// The rounding has to fit inside the block: half its extent on an axis with both edges exposed, the full
		// extent with one exposed edge, unlimited when neither edge is exposed.
		float radiusLimitX = (leftExternal && rightExternal) ? quadUnits.x * 0.5
			: ((leftExternal || rightExternal) ? quadUnits.x : farAway);
		float radiusLimitY = (topExternal && bottomExternal) ? quadUnits.y * 0.5
			: ((topExternal || bottomExternal) ? quadUnits.y : farAway);
		float blockCornerRadius = min(scaledCornerRadius, min(radiusLimitX, radiusLimitY));
		float distanceX = min(leftExternal ? quadPos.x : farAway, rightExternal ? quadUnits.x - quadPos.x : farAway);
		float distanceY = min(topExternal ? quadPos.y : farAway, bottomExternal ? quadUnits.y - quadPos.y : farAway);
		vec2 cornerDistance = vec2(blockCornerRadius) - vec2(distanceX, distanceY);
		distanceToEdge = length(max(cornerDistance, 0.0)) + min(max(cornerDistance.x, cornerDistance.y), 0.0) - blockCornerRadius;
		// Concave corners (two shared edges meeting a diagonal block of another style): carve the cell inset around
		// the corner point so that block's margin and outline continue around the corner.
		float reflexCorners = floor(v_externalEdges / 16.0);
		if (reflexCorners > 0.5) {
			vec2 cellPos = quadPos / v_cellToUnit;
			vec2 blockCells = quadUnits / v_cellToUnit;
			float insetCells = cellInset / max(cellSize - 2.0 * cellInset, 0.001);
			float reflexDistance = -farAway;
			if (mod(reflexCorners, 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos));
			}
			if (mod(floor(reflexCorners / 2.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - vec2(blockCells.x, 0.0)));
			}
			if (mod(floor(reflexCorners / 4.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - vec2(0.0, blockCells.y)));
			}
			if (mod(floor(reflexCorners / 8.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - blockCells));
			}
			distanceToEdge = max(distanceToEdge, reflexDistance * v_cellToUnit.x);
		}
	} else {
		vec2 cornerDistance = abs(v_cellUV - vec2(0.5)) - vec2(0.5 - scaledCornerRadius);
		distanceToEdge = length(max(cornerDistance, 0.0)) - scaledCornerRadius;
	}
	float antialiasWidth = fwidth(distanceToEdge);
	float coverage = 1.0 - smoothstep(0.0, antialiasWidth, distanceToEdge);
	float outline = smoothstep(-outlineWidth - antialiasWidth, -outlineWidth + antialiasWidth, distanceToEdge);
	vec4 cellColor = mix(v_color, v_outlineColor, outline);
	vec3 color = cellColor.rgb;
	float alpha = cellColor.a;
	bool showQueuedFootprintBoundary = footprintBoundaryEnabled > 0.5 && v_queuedFootprintConflict > 0.5;
	bool showInvalidPlacementBoundary = showInvalidFootprintBoundary > 0.5 && v_footprintValid < 0.5;
	if (showQueuedFootprintBoundary || showInvalidPlacementBoundary) {
		float leftEdge = mod(floor(v_footprintEdges), 2.0);
		float rightEdge = mod(floor(v_footprintEdges / 2.0), 2.0);
		float topEdge = mod(floor(v_footprintEdges / 4.0), 2.0);
		float bottomEdge = mod(floor(v_footprintEdges / 8.0), 2.0);
		// The boundary band keeps its per-cell width on merged blocks.
		vec2 boundaryWidth = footprintBoundaryWidth * v_cellToUnit;
		float leftOutline = leftEdge * (1.0 - smoothstep(0.0, boundaryWidth.x, quadPos.x));
		float rightOutline = rightEdge * smoothstep(quadUnits.x - boundaryWidth.x, quadUnits.x, quadPos.x);
		float topOutline = topEdge * (1.0 - smoothstep(0.0, boundaryWidth.y, quadPos.y));
		float bottomOutline = bottomEdge * smoothstep(quadUnits.y - boundaryWidth.y, quadUnits.y, quadPos.y);
		float footprintOutline = max(max(leftOutline, rightOutline), max(topOutline, bottomOutline));
		color = mix(color, invalidFootprintBoundaryColor.rgb, footprintOutline);
		alpha = mix(alpha, invalidFootprintBoundaryColor.a, footprintOutline);
	}

	fragColor = vec4(color, alpha * coverage * alphaMultiplier);
}
)original";
inline constexpr std::uint64_t WidgetHash=0xdfaf58a59bda1a75ULL;
}
