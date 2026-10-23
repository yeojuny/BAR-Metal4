/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Mechanical translation of exact31657 gui_buildsquare_gl4.lua VS/FS.
// Statuses, merging, colors and all option defaults remain in original Lua.
namespace rendering::metal4 {
inline constexpr const char* NativeBuildSquareShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct BuildSquareUniform {
 float4x4 cameraViewProjection;
 float4 mapSize,viewGeometry,values0,values1,values2,boundaryColor,values3;
 uint4 options,alpha;
};
struct BuildSquareInstance {packed_float4 cellData,color,outlineColor,cellExtra;packed_float2 footprintSize;};
struct BuildSquareVarying {
 float4 position [[position]];
 float4 color [[RECOIL_SAMPLE_PERSPECTIVE]],outlineColor [[RECOIL_SAMPLE_PERSPECTIVE]];
 float footprintEdges [[flat]],footprintValid [[flat]],queuedFootprintConflict [[flat]],mode [[flat]],externalEdges [[flat]];
 float2 quadUnits [[flat]],cellToUnit [[flat]];
 float simplifiedSizeScale [[flat]];
 float2 cellUV [[RECOIL_SAMPLE_PERSPECTIVE]];
};
inline float mod(float x,float y){return x-y*floor(x/y);}
inline float2 heightmapUVatWorldPos(float2 worldpos,constant BuildSquareUniform& u){
 float2 inverseMapSize=float2(1.0)/u.mapSize.xy;
 float2 heightmaptexel=float2(8.0,8.0);
 worldpos+=float2(-8.0,-8.0)*(worldpos*inverseMapSize)+float2(4.0,4.0);
 float2 uvhm=clamp(worldpos,heightmaptexel,u.mapSize.xy-heightmaptexel);
 float2 uv=uvhm*inverseMapSize;if(u.options.w)uv.y=1.0-uv.y;return uv;
}
vertex BuildSquareVarying OriginalBuildSquareWorld(uint id [[vertex_id]],uint instance [[instance_id]],device const packed_float2* vertices [[buffer(0)]],device const BuildSquareInstance* instances [[buffer(1)]],constant BuildSquareUniform& u [[buffer(2)]],texture2d<float> height [[texture(0)]],sampler hs [[sampler(0)]]){

 const float heightOffset=u.values0.x,waterLevel=u.values0.y,cellInset=u.values0.z,cellSize=u.values0.w;
 const float simplifiedReferenceCells=u.values1.x,simplifiedSizeFalloff=u.values1.y,minimumScreenDiameter=u.values1.z,cornerRadius=u.values1.w;
 const float simplifiedOutlineScale=u.values2.x,simplifiedCornerRadiusScale=u.values2.y,footprintBoundaryEnabled=u.values2.z,footprintBoundaryWidth=u.values2.w;
 const float showInvalidFootprintBoundary=u.values3.x,alphaMultiplier=u.values3.y;
 const float4 invalidFootprintBoundaryColor=u.boundaryColor,mapSize=u.mapSize,viewGeometry=u.viewGeometry;
 const int isMiniMap=int(u.options.x),rotationMiniMap=int(u.options.y);
 const float4x4 cameraViewProj=u.cameraViewProjection;
 const float2 a_cornerPos=float2(vertices[id]);
 const auto a=instances[instance];
 const float4 a_cellData=float4(a.cellData),a_color=float4(a.color),a_outlineColor=float4(a.outlineColor),a_cellExtra=float4(a.cellExtra);
 const float2 a_footprintSize=float2(a.footprintSize);
 BuildSquareVarying out;

	float mode = a_cellExtra.y;
	bool simplified = mode > 0.5 && mode < 1.5;
	bool merged = mode > 1.5;
	float2 quadSize = a_cellData.zw;
	float2 cellUV = a_cornerPos / cellSize;
	// Merged blocks are only inset on edges that are not shared with a same-styled neighbour block.
	float2 insetMin = float2(cellInset);
	float2 insetMax = float2(cellInset);
	if (merged) {
		float externalEdges = a_cellExtra.z;
		insetMin.x *= mod(floor(externalEdges), 2.0);
		insetMax.x *= mod(floor(externalEdges / 2.0), 2.0);
		insetMin.y *= mod(floor(externalEdges / 4.0), 2.0);
		insetMax.y *= mod(floor(externalEdges / 8.0), 2.0);
	}
	float2 insetQuadSize = quadSize - insetMin - insetMax;
	float2 insetCorner = insetMin + cellUV * insetQuadSize;
	float wx = a_cellData.x + insetCorner.x;
	float wz = a_cellData.y + insetCorner.y;

	out.color = a_color;
	out.outlineColor = a_outlineColor;
	float packedFootprintData = a_cellExtra.w;
	out.footprintEdges = mod(packedFootprintData, 16.0);
	out.footprintValid = step(15.5, mod(packedFootprintData, 32.0));
	out.queuedFootprintConflict = step(31.5, packedFootprintData);
	out.mode = mode;
	out.externalEdges = a_cellExtra.z;
	// Merged blocks measure distances relative to the inset footprint, exactly like the simplified quad of the
	// whole footprint would, so a fully placeable footprint looks identical to the simplified rendering.
	float2 quadUnits = float2(1.0);
	float2 cellToUnit = float2(1.0);
	float simplifiedSizeScale = 1.0;
	if (simplified || merged) {
		float2 footprintInsetSize = max(a_footprintSize * cellSize - 2.0 * cellInset, float2(0.001));
		// Corner radius and outline width grow with the footprint but with diminishing returns: unchanged at the
		// reference size, scaled by (reference / footprint) ^ falloff elsewhere (falloff 0 = proportional, 1 = constant).
		float referenceInsetSize = max(simplifiedReferenceCells * cellSize - 2.0 * cellInset, 0.001);
		simplifiedSizeScale = pow(referenceInsetSize / min(footprintInsetSize.x, footprintInsetSize.y), simplifiedSizeFalloff);
		if (merged) {
			quadUnits = insetQuadSize / footprintInsetSize;
			cellToUnit = (cellSize - 2.0 * cellInset) / footprintInsetSize;
		}
	}
	out.quadUnits = quadUnits;
	out.cellToUnit = cellToUnit;
	out.simplifiedSizeScale = simplifiedSizeScale;
	out.cellUV = cellUV;

		float2 uvhm = heightmapUVatWorldPos(float2(wx, wz),u);
		float wy = height.sample(hs,uvhm,level(0.0f)).x;
		wy = mix(wy, max(wy, waterLevel), a_cellExtra.x) + heightOffset;
		float4 clipPosition = cameraViewProj * float4(wx, wy, wz, 1.0);
		if (simplified) {
			float2 centerWorldPos = a_cellData.xy + quadSize * 0.5;
			float4 centerClipPosition = cameraViewProj * float4(centerWorldPos.x, wy, centerWorldPos.y, 1.0);
			float2 centerNdcPosition = centerClipPosition.xy / centerClipPosition.w;
			float2 cornerNdcOffset = clipPosition.xy / clipPosition.w - centerNdcPosition;
			float halfDiagonalPixels = length(cornerNdcOffset * viewGeometry.xy * 0.5);
			float minimumHalfDiagonal = minimumScreenDiameter * 0.5;
			float expansion = max(1.0, minimumHalfDiagonal / max(halfDiagonalPixels, 0.001));
			clipPosition.xy = (centerNdcPosition + cornerNdcOffset * expansion) * clipPosition.w;
		}
		out.position = clipPosition;
 if(u.options.z)out.position.z=(out.position.z+out.position.w)*.5f;return out;
}
vertex BuildSquareVarying OriginalBuildSquareMiniMap(uint id [[vertex_id]],uint instance [[instance_id]],device const packed_float2* vertices [[buffer(0)]],device const BuildSquareInstance* instances [[buffer(1)]],constant BuildSquareUniform& u [[buffer(2)]]){

 const float heightOffset=u.values0.x,waterLevel=u.values0.y,cellInset=u.values0.z,cellSize=u.values0.w;
 const float simplifiedReferenceCells=u.values1.x,simplifiedSizeFalloff=u.values1.y,minimumScreenDiameter=u.values1.z,cornerRadius=u.values1.w;
 const float simplifiedOutlineScale=u.values2.x,simplifiedCornerRadiusScale=u.values2.y,footprintBoundaryEnabled=u.values2.z,footprintBoundaryWidth=u.values2.w;
 const float showInvalidFootprintBoundary=u.values3.x,alphaMultiplier=u.values3.y;
 const float4 invalidFootprintBoundaryColor=u.boundaryColor,mapSize=u.mapSize,viewGeometry=u.viewGeometry;
 const int isMiniMap=int(u.options.x),rotationMiniMap=int(u.options.y);
 const float4x4 cameraViewProj=u.cameraViewProjection;
 const float2 a_cornerPos=float2(vertices[id]);
 const auto a=instances[instance];
 const float4 a_cellData=float4(a.cellData),a_color=float4(a.color),a_outlineColor=float4(a.outlineColor),a_cellExtra=float4(a.cellExtra);
 const float2 a_footprintSize=float2(a.footprintSize);
 BuildSquareVarying out;

	float mode = a_cellExtra.y;
	bool simplified = mode > 0.5 && mode < 1.5;
	bool merged = mode > 1.5;
	float2 quadSize = a_cellData.zw;
	float2 cellUV = a_cornerPos / cellSize;
	// Merged blocks are only inset on edges that are not shared with a same-styled neighbour block.
	float2 insetMin = float2(cellInset);
	float2 insetMax = float2(cellInset);
	if (merged) {
		float externalEdges = a_cellExtra.z;
		insetMin.x *= mod(floor(externalEdges), 2.0);
		insetMax.x *= mod(floor(externalEdges / 2.0), 2.0);
		insetMin.y *= mod(floor(externalEdges / 4.0), 2.0);
		insetMax.y *= mod(floor(externalEdges / 8.0), 2.0);
	}
	float2 insetQuadSize = quadSize - insetMin - insetMax;
	float2 insetCorner = insetMin + cellUV * insetQuadSize;
	float wx = a_cellData.x + insetCorner.x;
	float wz = a_cellData.y + insetCorner.y;

	out.color = a_color;
	out.outlineColor = a_outlineColor;
	float packedFootprintData = a_cellExtra.w;
	out.footprintEdges = mod(packedFootprintData, 16.0);
	out.footprintValid = step(15.5, mod(packedFootprintData, 32.0));
	out.queuedFootprintConflict = step(31.5, packedFootprintData);
	out.mode = mode;
	out.externalEdges = a_cellExtra.z;
	// Merged blocks measure distances relative to the inset footprint, exactly like the simplified quad of the
	// whole footprint would, so a fully placeable footprint looks identical to the simplified rendering.
	float2 quadUnits = float2(1.0);
	float2 cellToUnit = float2(1.0);
	float simplifiedSizeScale = 1.0;
	if (simplified || merged) {
		float2 footprintInsetSize = max(a_footprintSize * cellSize - 2.0 * cellInset, float2(0.001));
		// Corner radius and outline width grow with the footprint but with diminishing returns: unchanged at the
		// reference size, scaled by (reference / footprint) ^ falloff elsewhere (falloff 0 = proportional, 1 = constant).
		float referenceInsetSize = max(simplifiedReferenceCells * cellSize - 2.0 * cellInset, 0.001);
		simplifiedSizeScale = pow(referenceInsetSize / min(footprintInsetSize.x, footprintInsetSize.y), simplifiedSizeFalloff);
		if (merged) {
			quadUnits = insetQuadSize / footprintInsetSize;
			cellToUnit = (cellSize - 2.0 * cellInset) / footprintInsetSize;
		}
	}
	out.quadUnits = quadUnits;
	out.cellToUnit = cellToUnit;
	out.simplifiedSizeScale = simplifiedSizeScale;
	out.cellUV = cellUV;

		float2 ndcxy = float2(wx, wz) / mapSize.xy * 2.0 - 1.0;
		if (rotationMiniMap == 0) {
			ndcxy.y *= -1.0;
		} else if (rotationMiniMap == 1) {
			ndcxy.xy = ndcxy.yx;
		} else if (rotationMiniMap == 2) {
			ndcxy.x *= -1.0;
		} else {
			ndcxy.xy = -ndcxy.yx;
		}
		out.position = float4(ndcxy, 0.0, 1.0);
 if(u.options.z)out.position.z=(out.position.z+out.position.w)*.5f;return out;
}
fragment float4 OriginalBuildSquareFragment(BuildSquareVarying in [[stage_in]],constant BuildSquareUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));

 const float heightOffset=u.values0.x,waterLevel=u.values0.y,cellInset=u.values0.z,cellSize=u.values0.w;
 const float simplifiedReferenceCells=u.values1.x,simplifiedSizeFalloff=u.values1.y,minimumScreenDiameter=u.values1.z,cornerRadius=u.values1.w;
 const float simplifiedOutlineScale=u.values2.x,simplifiedCornerRadiusScale=u.values2.y,footprintBoundaryEnabled=u.values2.z,footprintBoundaryWidth=u.values2.w;
 const float showInvalidFootprintBoundary=u.values3.x,alphaMultiplier=u.values3.y;
 const float4 invalidFootprintBoundaryColor=u.boundaryColor,mapSize=u.mapSize,viewGeometry=u.viewGeometry;
 const int isMiniMap=int(u.options.x),rotationMiniMap=int(u.options.y);
 const float4x4 cameraViewProj=u.cameraViewProjection;

	bool merged = in.mode > 1.5;
	// Simplified quads and merged blocks share the same corner radius and outline width (relative to the footprint).
	float simplifiedMix = (in.mode > 0.5) ? 1.0 : 0.0;
	float scaledCornerRadius = min(
		cornerRadius * mix(1.0, simplifiedCornerRadiusScale * in.simplifiedSizeScale, simplifiedMix),
		0.5
	);
	float outlineWidth = 0.045 * mix(1.0, simplifiedOutlineScale * in.simplifiedSizeScale, simplifiedMix);
	// Distances are measured in inset-quad units for cells and simplified quads (the quad is one unit) and in
	// inset-footprint units for merged blocks (in.quadUnits = block size relative to the footprint), whose edges
	// shared with a same-styled neighbour are ignored so adjacent blocks join seamlessly.
	float2 quadUnits = merged ? in.quadUnits : float2(1.0);
	float2 quadPos = in.cellUV * quadUnits;
	float distanceToEdge;
	if (merged) {
		// Shared edges are pushed far away. Use exact selects, never mix() with the large constant: in float
		// precision that quantises the distances and turns the corners into staircases.
		float farAway = 1.0e5;
		bool leftExternal = mod(floor(in.externalEdges), 2.0) > 0.5;
		bool rightExternal = mod(floor(in.externalEdges / 2.0), 2.0) > 0.5;
		bool topExternal = mod(floor(in.externalEdges / 4.0), 2.0) > 0.5;
		bool bottomExternal = mod(floor(in.externalEdges / 8.0), 2.0) > 0.5;
		// The rounding has to fit inside the block: half its extent on an axis with both edges exposed, the full
		// extent with one exposed edge, unlimited when neither edge is exposed.
		float radiusLimitX = (leftExternal && rightExternal) ? quadUnits.x * 0.5
			: ((leftExternal || rightExternal) ? quadUnits.x : farAway);
		float radiusLimitY = (topExternal && bottomExternal) ? quadUnits.y * 0.5
			: ((topExternal || bottomExternal) ? quadUnits.y : farAway);
		float blockCornerRadius = min(scaledCornerRadius, min(radiusLimitX, radiusLimitY));
		float distanceX = min(leftExternal ? quadPos.x : farAway, rightExternal ? quadUnits.x - quadPos.x : farAway);
		float distanceY = min(topExternal ? quadPos.y : farAway, bottomExternal ? quadUnits.y - quadPos.y : farAway);
		float2 cornerDistance = float2(blockCornerRadius) - float2(distanceX, distanceY);
		distanceToEdge = length(max(cornerDistance, 0.0)) + min(max(cornerDistance.x, cornerDistance.y), 0.0) - blockCornerRadius;
		// Concave corners (two shared edges meeting a diagonal block of another style): carve the cell inset around
		// the corner point so that block's margin and outline continue around the corner.
		float reflexCorners = floor(in.externalEdges / 16.0);
		if (reflexCorners > 0.5) {
			float2 cellPos = quadPos / in.cellToUnit;
			float2 blockCells = quadUnits / in.cellToUnit;
			float insetCells = cellInset / max(cellSize - 2.0 * cellInset, 0.001);
			float reflexDistance = -farAway;
			if (mod(reflexCorners, 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos));
			}
			if (mod(floor(reflexCorners / 2.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - float2(blockCells.x, 0.0)));
			}
			if (mod(floor(reflexCorners / 4.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - float2(0.0, blockCells.y)));
			}
			if (mod(floor(reflexCorners / 8.0), 2.0) > 0.5) {
				reflexDistance = max(reflexDistance, insetCells - length(cellPos - blockCells));
			}
			distanceToEdge = max(distanceToEdge, reflexDistance * in.cellToUnit.x);
		}
	} else {
		float2 cornerDistance = abs(in.cellUV - float2(0.5)) - float2(0.5 - scaledCornerRadius);
		distanceToEdge = length(max(cornerDistance, 0.0)) - scaledCornerRadius;
	}
	float antialiasWidth = fwidth(distanceToEdge);
	float coverage = 1.0 - smoothstep(0.0, antialiasWidth, distanceToEdge);
	float outline = smoothstep(-outlineWidth - antialiasWidth, -outlineWidth + antialiasWidth, distanceToEdge);
	float4 cellColor = mix(in.color, in.outlineColor, outline);
	float3 color = cellColor.rgb;
	float alpha = cellColor.a;
	bool showQueuedFootprintBoundary = footprintBoundaryEnabled > 0.5 && in.queuedFootprintConflict > 0.5;
	bool showInvalidPlacementBoundary = showInvalidFootprintBoundary > 0.5 && in.footprintValid < 0.5;
	if (showQueuedFootprintBoundary || showInvalidPlacementBoundary) {
		float leftEdge = mod(floor(in.footprintEdges), 2.0);
		float rightEdge = mod(floor(in.footprintEdges / 2.0), 2.0);
		float topEdge = mod(floor(in.footprintEdges / 4.0), 2.0);
		float bottomEdge = mod(floor(in.footprintEdges / 8.0), 2.0);
		// The boundary band keeps its per-cell width on merged blocks.
		float2 boundaryWidth = footprintBoundaryWidth * in.cellToUnit;
		float leftOutline = leftEdge * (1.0 - smoothstep(0.0, boundaryWidth.x, quadPos.x));
		float rightOutline = rightEdge * smoothstep(quadUnits.x - boundaryWidth.x, quadUnits.x, quadPos.x);
		float topOutline = topEdge * (1.0 - smoothstep(0.0, boundaryWidth.y, quadPos.y));
		float bottomOutline = bottomEdge * smoothstep(quadUnits.y - boundaryWidth.y, quadUnits.y, quadPos.y);
		float footprintOutline = max(max(leftOutline, rightOutline), max(topOutline, bottomOutline));
		color = mix(color, invalidFootprintBoundaryColor.rgb, footprintOutline);
		alpha = mix(alpha, invalidFootprintBoundaryColor.a, footprintOutline);
	}

	float4 fragColor = float4(color, alpha * coverage * alphaMultiplier);

 if(u.alpha.x){bool pass=false;float a=fragColor.a,r=u.values3.z;
  switch(u.alpha.y){case 0:break;case 1:pass=a<r;break;case 2:pass=a==r;break;case 3:pass=a<=r;break;case 4:pass=a>r;break;case 5:pass=a!=r;break;case 6:pass=a>=r;break;case 7:pass=true;break;}if(!pass)discard_fragment();}
 return fragColor;
}
)MSL";
}
