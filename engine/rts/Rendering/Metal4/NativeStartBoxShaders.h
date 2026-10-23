/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeStartBoxShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"StartBoxMSL(
// SPDX-License-Identifier: MIT
// Original complete exact31657 start polygon/cone and SDF bake shaders: Beherith.
// The Wombat noise attribution is retained below with the complete function.
// Source program defines are inserted only after exact original identity checks.
#include <metal_stdlib>
using namespace metal;
/*__ORIGINAL_STARTBOX_DEFINES__*/
// These types describe the actual retained sampler2D storage, not a source
// shader variant or fabricated fallback image. Empty means GL-incomplete.
/*__ORIGINAL_STARTBOX_IMAGE_TYPES__*/
inline float4 StartBoxSample(texture2d<float> t,sampler s,float2 uv) { return t.sample(s,uv); }
inline float4 StartBoxSample(depth2d<float> t,sampler s,float2 uv) { float d=t.sample(s,uv);return float4(d,d,d,1); }
inline float4 StartBoxSampleLOD0(texture2d<float> t,sampler s,float2 uv) { return t.sample(s,uv,level(0)); }
inline float4 StartBoxSampleLOD0(depth2d<float> t,sampler s,float2 uv) { float d=t.sample(s,uv,level(0));return float4(d,d,d,1); }
inline bool StartBoxAlphaPass(float a,uint test,float reference) {
 if(!(test&256))return true;
 switch(test&255){case 0:return false;case 1:return a<reference;case 2:return a==reference;
 case 3:return a<=reference;case 4:return a>reference;case 5:return a!=reference;
 case 6:return a>=reference;default:return true;}
}
struct StartBoxUniform {
 float4x4 cameraViewProjection,cameraViewProjectionInverse;
 float4 timeInfo,mapSize,pipVisibleArea;
 int4 parameters; // isMiniMap, myAllyTeamID, rotationMiniMap, waterSurfaceMode
 float4 values; // startPosScale, cone isMinimap, alpha-test reference, waterLevel
 uint4 options; // physical image Y-flips, original clip-depth convention
 float4x4 cameraViewInverse; // exact retained engine UBO cameraViewInv
};
#if STARTBOX_POLYGON || STARTBOX_SDF_BAKE
struct StartPolygonOut {
 float4 position [[position]];
 float4 v_position [[RECOIL_SAMPLE_PERSPECTIVE]];
};
vertex StartPolygonOut OriginalStartPolygonVertex(uint id [[vertex_id]],
 const device float4* positions [[buffer(0)]],constant StartBoxUniform& u [[buffer(1)]]) {
 StartPolygonOut o;o.position=float4(positions[id].xy,0.0,1.0);o.v_position=positions[id];
 if(u.options.y)o.position.z=(o.position.z+o.position.w)*.5;
 return o;
}
#endif
#if STARTBOX_POLYGON
struct StartPolygonFragmentProgram {
 float4 v_position,fragColor;
 float4x4 cameraViewProjInv,cameraViewInv;
 float4 timeInfo,mapSize,pipVisibleArea;
 int isMiniMap,rotationMiniMap,waterSurfaceMode;
 float waterLevel;
 uint flips;
 uint clipMinusOneToOne;
 texture2d<float> startPolygonSDF;
 int2 SDFSize() { return int2(startPolygonSDF.get_width(),startPolygonSDF.get_height()); }
 float4 SDFAt(int2 p) {
  if(flips & (1u<<5))p.y=int(startPolygonSDF.get_height())-1-p.y;
  return startPolygonSDF.read(uint2(p),0);
 }
#if STARTBOX_IMAGE_0 != 0
 STARTBOX_TYPE_0 mapDepths; sampler mapDepthSampler;
#endif
#if STARTBOX_IMAGE_1 != 0
 STARTBOX_TYPE_1 mapNormals; sampler normalSampler;
#endif
#if defined(SCAV_ALLYTEAM_ID) && STARTBOX_IMAGE_3 != 0
 STARTBOX_TYPE_3 scavTexture; sampler scavSampler;
#endif
#if defined(RAPTOR_ALLYTEAM_ID) && STARTBOX_IMAGE_4 != 0
 STARTBOX_TYPE_4 raptorTexture; sampler raptorSampler;
#endif
 float4 DepthAt(float2 uv) {
#if STARTBOX_IMAGE_0 != 0
  return StartBoxSampleLOD0(mapDepths,mapDepthSampler,ImageUV(uv,0));
#else
  return float4(0,0,0,1);
#endif
 }
 float4 NormalAt(float2 uv) {
#if STARTBOX_IMAGE_1 != 0
  return StartBoxSampleLOD0(mapNormals,normalSampler,ImageUV(uv,1));
#else
  return float4(0,0,0,1);
#endif
 }
#ifdef SCAV_ALLYTEAM_ID
 float4 ScavAt(float2 uv) {
#if STARTBOX_IMAGE_3 != 0
  return StartBoxSample(scavTexture,scavSampler,ImageUV(uv,3));
#else
  return float4(0,0,0,1);
#endif
 }
#endif
#ifdef RAPTOR_ALLYTEAM_ID
 float4 RaptorAt(float2 uv) {
#if STARTBOX_IMAGE_4 != 0
  return StartBoxSample(raptorTexture,raptorSampler,ImageUV(uv,4));
#else
  return float4(0,0,0,1);
#endif
 }
#endif
 float2 ImageUV(float2 uv,uint slot) {if(flips & (1u<<slot))uv.y=1.0-uv.y;return uv;}
float2 heightmapUVatWorldPos(float2 worldpos){
	float2 inverseMapSize = float2(1.0) / mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	float2 heightmaptexel = float2(8.0, 8.0);
	worldpos +=  float2(-8.0, -8.0) * (worldpos * inverseMapSize) + float2(4.0, 4.0) ;
	float2 uvhm = clamp(worldpos, heightmaptexel, mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;
	return uvhm;
}
//
//  Wombat
//  An efficient texture-free GLSL procedural noise library
//  Source: https://github.com/BrianSharpe/Wombat
//  Derived from: https://github.com/BrianSharpe/GPU-Noise-Lib
//
//  I'm not one for copyrights.  Use the code however you wish.
//  All I ask is that credit be given back to the blog or myself when appropriate.
//  And also to let me know if you come up with any changes, improvements, thoughts or interesting uses for this stuff. :)
//  Thanks!
//
//  Brian Sharpe
//  brisharpe CIRCLE_A yahoo DOT com
//  http://briansharpe.wordpress.com
//  https://github.com/BrianSharpe
//

//
//  This represents a modified version of Stefan Gustavson's work at http://www.itn.liu.se/~stegu/GLSL-cellular
//  The noise is optimized to use a 2x2x2 search window instead of 3x3x3
//  Modifications are...
//  - faster random number generation
//  - analytical final normalization
//  - random point offset is restricted to prevent artifacts
//

//
//  Cellular Noise 3D
//  produces a range of 0.0->1.0
//
float Cellular3D(float3 P)
{
    //	establish our grid cell and unit position
    float3 Pi = floor(P);
    float3 Pf = P - Pi;

    // clamp the domain
    Pi.xyz = Pi.xyz - floor(Pi.xyz * ( 1.0 / 69.0 )) * 69.0;
    float3 Pi_inc1 = step( Pi, float3( 69.0 - 1.5 ) ) * ( Pi + 1.0 );

    // calculate the hash ( over -1.0->1.0 range )
    float4 Pt = float4( Pi.xy, Pi_inc1.xy ) + float2( 50.0, 161.0 ).xyxy;
    Pt *= Pt;
    Pt = Pt.xzxz * Pt.yyww;
    const float3 SOMELARGEFLOATS = float3( 635.298681, 682.357502, 668.926525 );
    const float3 ZINC = float3( 48.500388, 65.294118, 63.934599 );
    float3 lowz_mod = float3( 1.0 / ( SOMELARGEFLOATS + Pi.zzz * ZINC ) );
    float3 highz_mod = float3( 1.0 / ( SOMELARGEFLOATS + Pi_inc1.zzz * ZINC ) );
    float4 hash_x0 = fract( Pt * lowz_mod.xxxx ) * 2.0 - 1.0;
    float4 hash_x1 = fract( Pt * highz_mod.xxxx ) * 2.0 - 1.0;
    float4 hash_y0 = fract( Pt * lowz_mod.yyyy ) * 2.0 - 1.0;
    float4 hash_y1 = fract( Pt * highz_mod.yyyy ) * 2.0 - 1.0;
    float4 hash_z0 = fract( Pt * lowz_mod.zzzz ) * 2.0 - 1.0;
    float4 hash_z1 = fract( Pt * highz_mod.zzzz ) * 2.0 - 1.0;

    //  generate the 8 point positions
    const float JITTER_WINDOW = 0.166666666;	// 0.166666666 will guarantee no artifacts.
    hash_x0 = ( ( hash_x0 * hash_x0 * hash_x0 ) - sign( hash_x0 ) ) * JITTER_WINDOW + float4( 0.0, 1.0, 0.0, 1.0 );
    hash_y0 = ( ( hash_y0 * hash_y0 * hash_y0 ) - sign( hash_y0 ) ) * JITTER_WINDOW + float4( 0.0, 0.0, 1.0, 1.0 );
    hash_x1 = ( ( hash_x1 * hash_x1 * hash_x1 ) - sign( hash_x1 ) ) * JITTER_WINDOW + float4( 0.0, 1.0, 0.0, 1.0 );
    hash_y1 = ( ( hash_y1 * hash_y1 * hash_y1 ) - sign( hash_y1 ) ) * JITTER_WINDOW + float4( 0.0, 0.0, 1.0, 1.0 );
    hash_z0 = ( ( hash_z0 * hash_z0 * hash_z0 ) - sign( hash_z0 ) ) * JITTER_WINDOW + float4( 0.0, 0.0, 0.0, 0.0 );
    hash_z1 = ( ( hash_z1 * hash_z1 * hash_z1 ) - sign( hash_z1 ) ) * JITTER_WINDOW + float4( 1.0, 1.0, 1.0, 1.0 );

    //	return the closest squared distance
    float4 dx1 = Pf.xxxx - hash_x0;
    float4 dy1 = Pf.yyyy - hash_y0;
    float4 dz1 = Pf.zzzz - hash_z0;
    float4 dx2 = Pf.xxxx - hash_x1;
    float4 dy2 = Pf.yyyy - hash_y1;
    float4 dz2 = Pf.zzzz - hash_z1;
    float4 d1 = dx1 * dx1 + dy1 * dy1 + dz1 * dz1;
    float4 d2 = dx2 * dx2 + dy2 * dy2 + dz2 * dz2;
    d1 = min(d1, d2);
    d1.xy = min(d1.xy, d1.wz);
    return min(d1.x, d1.y) * ( 9.0 / 12.0 ); // return a value scaled to 0.0->1.0
}

void Run()
{
	float4 mapWorldPos = float4(1);
	bool isWaterSurface = false;
	// Transform screen-space depth to world-space position
	if (isMiniMap == 1) {
		mapWorldPos.y = (MINY + MAXY) * 0.5;

		// Check if PIP mode (visible area not default)
		bool isPip = (pipVisibleArea.x != 0.0 || pipVisibleArea.y != 1.0 || pipVisibleArea.z != 0.0 || pipVisibleArea.w != 1.0);

		// Start with NDC coords [-1,1] -> normalized coords [0,1]
		float2 normCoords = v_position.xy * 0.5 + 0.5;

		mapWorldPos.xz = normCoords;
		if (rotationMiniMap == 0){
			mapWorldPos.z = 1.0 - mapWorldPos.z;
		}else if (rotationMiniMap == 1){
			mapWorldPos.xz = mapWorldPos.zx;
		}else if (rotationMiniMap == 2){
			mapWorldPos.x = 1.0 - mapWorldPos.x;
		}else if (rotationMiniMap == 3){
			float tmpX = mapWorldPos.x;
			mapWorldPos.x = 1.0 - mapWorldPos.z;
			mapWorldPos.z = 1.0 - tmpX;
		}

		// For PIP: remap the [0,1] world-normalized coords to visible area
		// AFTER rotation has been applied
		if (isPip) {
			// mapWorldPos.xz is now in [0,1] world-normalized space
			// Map screen [0,1] to visible portion of world [visL,visR] x [visB,visT]
			mapWorldPos.x = mix(pipVisibleArea.x, pipVisibleArea.y, mapWorldPos.x);
			// Flip Y: screen top (1) -> visB, screen bottom (0) -> visT
			mapWorldPos.z = mix(pipVisibleArea.w, pipVisibleArea.z, mapWorldPos.z);
		}

		mapWorldPos.xz *= mapSize.xy;

		fragColor.rgba = float4(0.5);
		//return;
	}else{
		float mapdepth = DepthAt(v_position.zw).x;
		// Undo the native terrain clip-depth conversion before the original inverse.
		if (clipMinusOneToOne) mapdepth = mapdepth * 2.0 - 1.0;
		mapWorldPos =  float4( float3(v_position.xy, mapdepth),  1.0);
		mapWorldPos = cameraViewProjInv * mapWorldPos;
		mapWorldPos.xyz = mapWorldPos.xyz / mapWorldPos.w;

		// We are above or below the map by 4 or more elmost, discard
		if (mapWorldPos.y > (MAXY) || mapWorldPos.y < (MINY)){
			fragColor.rgba = float4(0);
			return;
		}

		// We are out of the map, discard:
		if ((mapWorldPos.x < 0) || (mapWorldPos.x > mapSize.x) || (mapWorldPos.z < 0) || (mapWorldPos.z > mapSize.y)){
			fragColor.rgba = float4(0);
			return;
		}

		if (waterSurfaceMode > 0) {
			float3 cameraPos = cameraViewInv[3].xyz;
			float3 waterRayDirection = normalize(mapWorldPos.xyz - cameraPos);
			if (waterRayDirection.y < -0.0001) {
				float waterRayDistance = (waterLevel - cameraPos.y) / waterRayDirection.y;
				float terrainRayDistance = length(mapWorldPos.xyz - cameraPos);
				if (waterRayDistance > 0.0 && waterRayDistance < terrainRayDistance) {
					mapWorldPos.xyz = cameraPos + waterRayDirection * waterRayDistance;
					isWaterSurface = true;
				}
			}
		}
	}

	// One bilinear tap of the baked field replaces walking every polygon edge per pixel.
	// The four texels are fetched by hand: the distances are blended, while the
	// categorical flags come from the most-inside texel, so the last half texel inside an
	// edge does not pick up the outside texel's "no box" flags.
	int2 sdfSize = SDFSize();
	float2 tc = clamp(mapWorldPos.xz / mapSize.xy * float2(sdfSize) - 0.5, float2(0.0), float2(sdfSize - 1));
	int2 i0 = int2(tc);
	int2 i1 = min(i0 + 1, sdfSize - 1);
	float2 f = tc - float2(i0);
	float4 t00 = SDFAt(i0);
	float4 t10 = SDFAt(int2(i1.x, i0.y));
	float4 t01 = SDFAt(int2(i0.x, i1.y));
	float4 t11 = SDFAt(i1);
	float4 sdf = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);
	float4 flagTexel = t00;
	if (t10.x < flagTexel.x) flagTexel = t10;
	if (t01.x < flagTexel.x) flagTexel = t01;
	if (t11.x < flagTexel.x) flagTexel = t11;
	int flags = int(flagTexel.w + 0.5);

	float closestbox = sdf.x;
	float anyBoxEdgeDistance = max(sdf.z, 0.0);
	int inAllyBox = flags & 1;
	int numEnemyBoxes = flags >> 3;
	#ifdef SCAV_ALLYTEAM_ID
		int inScavBox = (flags >> 1) & 1;
	#endif
	#ifdef RAPTOR_ALLYTEAM_ID
		int inRaptorBox = (flags >> 2) & 1;
	#endif
	bool isPassable = false;
	float3 mycolor = float3(0,0,0);

	// Define the colors for the individual cases:

	if (inAllyBox == 1){ // my box
		if (numEnemyBoxes > 0){ // has enemy boxes
			mycolor = float3(0.5, 0.7, 0.0);
		}else{ // solo my box
			mycolor = float3(0.0, 0.7, 0.0);
		}
	}else{ // not my box
		if (numEnemyBoxes <= 1){ // solo enemy box
			mycolor = float3(1.0, 0.0, 0.0);
			#ifdef RAPTOR_ALLYTEAM_ID
				if (inRaptorBox == 1){
					mycolor = float3(1.0, 0.45, 0.0);
				}
			#endif

			#ifdef SCAV_ALLYTEAM_ID
				if (inScavBox == 1){
					mycolor = float3(0.6, 0.0, 1.0);
				}
			#endif

		}else{ // shared enemy box
			mycolor = float3(1.0, 0.2, 0.0);

			#ifdef RAPTOR_ALLYTEAM_ID
				if (inRaptorBox == 1){
					mycolor = float3(1.0, 0.4, 0.0);
				}
			#endif

			#ifdef SCAV_ALLYTEAM_ID
				if (inScavBox == 1){
					mycolor = float3(1.0, 0.3, 1.0);
				}
			#endif
		}
	}


	// Note that now we have the distance to the closest box in closestbox

	// Debug color based on their distance from the closest box
	// fragColor.rgba = float4(mycolor * sin(closestbox*3 / (40/3.14)), 0.5);

	// But if we are within a box, then we set the alpha to 0
	float2 uvhm = heightmapUVatWorldPos(mapWorldPos.xz);
	float3 mapnormal = NormalAt(uvhm).raa; // seems to be in the [-1, 1] range!, raaa is its true return
	mapnormal.g = sqrt( 1.0 - dot( mapnormal.rb, mapnormal.rb)); // reconstruct Y from it
	if (isWaterSurface) {
		mapnormal = float3(0.0, 1.0, 0.0);
	}

	if (mapnormal.y < MAX_STEEPNESS){
		isPassable = true;
	}

	// Generate a build grid (matching the usual 64+32 elmo subgrid)
	float2 fragSize = fwidth(mapWorldPos.xz);
	float fragSizeFactor = 1.0/  dot(float2(1.0),fragSize);

	fragSize *= 1.5;

	float2 buildGrid16 = abs(fract(mapWorldPos.xz/16.0 - 0.5) - 0.5) * (16)/fragSize;
	float grid16 = 0.25* clamp(1.0 - min(buildGrid16.x, buildGrid16.y), 0.0, 1.0);

	float2 buildGrid32 = abs(fract(mapWorldPos.xz/48.0 - 0.5) - 0.5) * (32)/fragSize;
	float grid32 = 0.5*clamp(1.0 - min(buildGrid32.x, buildGrid32.y), 0.0, 1.0);

	float2 buildGrid64 = abs(fract(mapWorldPos.xz/192.0 - 0.5) - 0.5) * (192)/fragSize;
	float grid64 = clamp(1.0 - min(buildGrid64.x, buildGrid64.y), 0.0, 1.0);

	float gridmerge = fragSizeFactor * dot(float3(grid16, grid32, grid64), float3(0.5, 0.75, 2.0));
	//fragColor.rgba = float4(float3(gridmerge), 0.2); return;

	if (closestbox < 0.0) {
		// we are in at least 1 box

		// get edge factor in this case:
		// but make it anti aliased
		float edgeFactor = 1.0 - clamp((anyBoxEdgeDistance / 16.0) + clamp (1.0 - anyBoxEdgeDistance * 0.5,0.0,1.0), 0.0, 1.0);

		fragColor.a = 0.25;
		fragColor.rgb = mycolor;
		//float anim =  Cellular3D(0.01* float3(mapWorldPos.xz, dot (mapWorldPos.xz, float2(1.0)) * 0.1 + timeInfo.y * 50));
		float cellNoise =  Cellular3D((1.0/2048.0)* float3(mapWorldPos.xz, closestbox * 0.5 - timeInfo.y * 190));

		// absclamplify the cellnoise:
		cellNoise += smoothstep( 0.0, 1.0, (1.0 - abs(cellNoise -0.5 ) * 10.0)) * 0.25;
		// zero the cellnoise where you shouldn't be building:
		cellNoise *= smoothstep(0.95, 1.0, mapnormal.y);

		// float expboxedge = 0.5 * expSustainedImpulse(-1* closestbox, 32.0, (1/32.0));
		cellNoise = 0.1 + 1.15 * cellNoise;

		fragColor.a = cellNoise *(gridmerge + 0.45);
		//fragColor.a = clamp(expboxedge , 0.4 * anim, 0.5);
		if (isMiniMap > 0.5){
			edgeFactor = 1.0 - clamp((1.0*anyBoxEdgeDistance * fragSizeFactor), 0.0, 1.0);

			// disable noise on minimap
			fragColor.a = 0.2;

			fragColor.rgba += edgeFactor * 0.5;
			return;
		}

		if (1 == 1){
			float smoothwidth = 0.05 / fragSizeFactor;
			float diagonalstriping = smoothstep(0.5-smoothwidth, 0.5+smoothwidth, abs(fract((mapWorldPos.x + mapWorldPos.z) / 16) -0.5)*2) ;
			float4 impassableColor = float4(mycolor.rgb * diagonalstriping, 0.66);

			//fragColor.a = 0.5;
			//fragColor.g = smoothstep(0.48, 0.52, fract((mapWorldPos.x + mapWorldPos.z) / 16));
			float impassablewidth = 0.05;
			fragColor = mix(fragColor, impassableColor,
			smoothstep(-1 * impassablewidth,impassablewidth, MAX_STEEPNESS - mapnormal.y) * 0.5);
		}

		#ifdef SCAV_ALLYTEAM_ID
			if (inScavBox == 1 && inAllyBox == 0){
				float4 scavTex = ScavAt(mapWorldPos.xz / 1024.0);
				float whiteness = dot(float3(1.0/3.0), scavTex.rgb);
				float scavalpha = 1.0 - whiteness;
				fragColor.rgba = float4(float3(1.0, 0.1, 0.7), scavalpha);
				//fragColor.rgba = scavTex;
				fragColor.a += cellNoise;
			}
		#endif

		#ifdef RAPTOR_ALLYTEAM_ID
			if (inRaptorBox == 1 && inAllyBox == 0){
				float4 raptorTex = RaptorAt(mapWorldPos.xz / 1024.0);
				float whiteness = dot(float3(1.0/3.0), raptorTex.rgb);
				float raptoralpha = 1.0 - whiteness;
				fragColor.rgba = float4(float3(1.0, 0.68, 0.12), raptoralpha);
				//fragColor.rgba = raptorTex;
				fragColor.a += cellNoise;
			}
		#endif

		//fragColor.a = sin( closestbox * 0.02 + timeInfo.y * 0.1);
		//fragColor.a *= Cellular3D(0.01* float3(mapWorldPos.xz, timeInfo.y));
		fragColor.rgba += edgeFactor * 0.33;
	}else{
		//fragColor.a = clamp(sin(smoothDistance * 0.2) * 0.05, 0.0,1.0);
		fragColor.rgba = float4(0.0);
	}
	//fragColor.rgba = float4(mapnormal * 0.5 + 0.5, 1.0);
}
};
fragment float4 OriginalStartPolygonFragment(StartPolygonOut in [[stage_in]],
 constant StartBoxUniform& u [[buffer(1)]],texture2d<float> field [[texture(5)]]
#if STARTBOX_IMAGE_0 != 0
 ,STARTBOX_TYPE_0 mapDepths [[texture(0)]],sampler depthSampler [[sampler(0)]]
#endif
#if STARTBOX_IMAGE_1 != 0
 ,STARTBOX_TYPE_1 normals [[texture(1)]],sampler normalSampler [[sampler(1)]]
#endif
#if defined(SCAV_ALLYTEAM_ID) && STARTBOX_IMAGE_3 != 0
 ,STARTBOX_TYPE_3 scav [[texture(3)]],sampler scavSampler [[sampler(3)]]
#endif
#if defined(RAPTOR_ALLYTEAM_ID) && STARTBOX_IMAGE_4 != 0
 ,STARTBOX_TYPE_4 raptor [[texture(4)]],sampler raptorSampler [[sampler(4)]]
#endif
  RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 StartPolygonFragmentProgram p;
 p.v_position=in.v_position;p.cameraViewProjInv=u.cameraViewProjectionInverse;p.cameraViewInv=u.cameraViewInverse;
 p.startPolygonSDF=field;
 p.timeInfo=u.timeInfo;p.mapSize=u.mapSize;p.pipVisibleArea=u.pipVisibleArea;
 p.isMiniMap=u.parameters.x;p.rotationMiniMap=u.parameters.z;
 p.waterSurfaceMode=u.parameters.w;p.waterLevel=u.values.w;
 p.flips=u.options.x;p.clipMinusOneToOne=u.options.y;
#if STARTBOX_IMAGE_0 != 0
 p.mapDepths=mapDepths;p.mapDepthSampler=depthSampler;
#endif
#if STARTBOX_IMAGE_1 != 0
 p.mapNormals=normals;p.normalSampler=normalSampler;
#endif
#if defined(SCAV_ALLYTEAM_ID) && STARTBOX_IMAGE_3 != 0
 p.scavTexture=scav;p.scavSampler=scavSampler;
#endif
#if defined(RAPTOR_ALLYTEAM_ID) && STARTBOX_IMAGE_4 != 0
 p.raptorTexture=raptor;p.raptorSampler=raptorSampler;
#endif
 p.Run();if(!StartBoxAlphaPass(p.fragColor.a,u.options.z,u.values.z))discard_fragment();
 return p.fragColor;
}
#endif
#if STARTBOX_SDF_BAKE
struct StartSDFBakeProgram {
 float4 v_position,fragColor,mapSize;
 int myAllyTeamID;
 const device float4* polyVerts;
// Distances are clamped so a half-float target stays finite: filtering between a finite
// and an infinite texel yields NaN.
#define SDF_CLAMP 30000.0

// Signed distance to a polygon ring, negative inside.
float sdPolygon2(float2 p, int startOffset, int numVertices)
{
	float d = dot(p - polyVerts[startOffset].zw, p - polyVerts[startOffset].zw);
	float s = 1.0;
	for (int i = 0, j = numVertices - 1; i < numVertices; j = i, i++) {
		int newj = startOffset + j;
		int newi = startOffset + i;
		float2 e = polyVerts[newj].zw - polyVerts[newi].zw;
		float2 w = p - polyVerts[newi].zw;
		float2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0, 1.0);
		d = min(d, dot(b, b));

		// winding number from http://geomalgorithms.com/a03-_inclusion.html
		bool3 cond = bool3(p.y >= polyVerts[newi].w,
		                   p.y < polyVerts[newj].w,
		                   e.x * w.y > e.y * w.x);
		if (all(cond) || all(!cond)) s = -s;
	}

	return s * sqrt(d);
}

void Run()
{
	// The quad's uv spans the whole map, so each texel centre lands on a map position.
	float2 p = v_position.zw * mapSize.xy;

	float closestbox = SDF_CLAMP;
	float closestKey = 0.0;
	// Least-inside signed distance among the polygons containing p; stays at the sentinel
	// when none does.
	float maxInsideDistance = -SDF_CLAMP;
	bool insideAny = false;
	int numEnemyBoxes = 0;
	int inAllyBox = 0;
	int inScavBox = 0;
	int inRaptorBox = 0;

	int startpoint = 0;
	for (int i = 0; i < NUM_POLYGONS; i++) {
		if (startpoint >= NUM_POINTS) {
			break;
		}

		int key = int(polyVerts[startpoint].x);
		int vertexCount = max(int(polyVerts[startpoint].y), 0);
		int endpoint = min(startpoint + vertexCount, NUM_POINTS);
		if ((endpoint - startpoint) < 3) {
			startpoint = endpoint;
			continue;
		}

		float signedDistance = sdPolygon2(p, startpoint, endpoint - startpoint);
		if (signedDistance < closestbox) {
			closestbox = signedDistance;
			closestKey = float(key);
		}

		if (signedDistance < 0.0) {
			insideAny = true;
			maxInsideDistance = max(maxInsideDistance, signedDistance);
			if (key == myAllyTeamID) {
				inAllyBox = 1;
			} else {
				numEnemyBoxes = numEnemyBoxes + 1;
			}
			#ifdef SCAV_ALLYTEAM_ID
				if (key == SCAV_ALLYTEAM_ID) {
					inScavBox = 1;
				}
			#endif
			#ifdef RAPTOR_ALLYTEAM_ID
				if (key == RAPTOR_ALLYTEAM_ID) {
					inRaptorBox = 1;
				}
			#endif
		}

		startpoint = endpoint;
	}

	// Distance to the edge of the containing polygon, positive inside. Outside it continues
	// as -closestbox so the field is continuous across the edge and filters cleanly.
	float edgeDistance = -(insideAny ? maxInsideDistance : closestbox);

	int flags = inAllyBox | (inScavBox << 1) | (inRaptorBox << 2) | (min(numEnemyBoxes, 2) << 3);

	fragColor = float4(
		clamp(closestbox, -SDF_CLAMP, SDF_CLAMP),
		closestKey,
		clamp(edgeDistance, -SDF_CLAMP, SDF_CLAMP),
		float(flags)
	);
}
};
fragment float4 OriginalStartSDFBakeFragment(StartPolygonOut in [[stage_in]],
 constant StartBoxUniform& u [[buffer(1)]],const device float4* polygons [[buffer(4)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 StartSDFBakeProgram p;p.v_position=in.v_position;p.mapSize=u.mapSize;
 p.myAllyTeamID=u.parameters.y;p.polyVerts=polygons;p.Run();
 if(!StartBoxAlphaPass(p.fragColor.a,u.options.z,u.values.z))discard_fragment();
 return p.fragColor;
}
#endif
#if !STARTBOX_POLYGON && !STARTBOX_SDF_BAKE
struct StartConeInstance {float4 worldposrad,teamcolor;};
struct StartConeOut {float4 position [[position]];float4 v_worldposrad [[RECOIL_SAMPLE_PERSPECTIVE]],v_teamcolor [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct StartConeVertexProgram {
 float4 position,worldposrad,teamcolor,v_worldposrad,v_teamcolor,gl_Position;
 float4x4 cameraViewProj;
 float4 mapSize,pipVisibleArea;
 float isMinimap,startPosScale;
 int rotationMiniMap;
void Run()
{
	v_teamcolor = teamcolor;
	float4 worldPos = float4(position.xyz, 1.0);
	
	if (isMinimap < 0.5) { // world
		worldPos.xyz = worldPos.xyz + worldposrad.xyz;
		v_worldposrad = float4(worldPos.xyz, worldposrad.w);
		gl_Position = cameraViewProj * worldPos;
	}else{
		// Calculate cone offset in screen space
		float2 coneOffset = position.xz * startPosScale;
		coneOffset.y *= mapSize.x/mapSize.y;

		// Get world position of the start point in normalized coords [0,1]
		float2 normPos = worldposrad.xz / mapSize.xy;
		
		// Convert to NDC first (standard minimap transform)
		float2 ndcxy = normPos * 2.0 - 1.0;
		
		// Apply rotation
		if (rotationMiniMap == 0) {
			ndcxy.y *= -1;
		}else if (rotationMiniMap == 1) {
			ndcxy.xy = ndcxy.yx;
		}else if (rotationMiniMap == 2) {
			ndcxy.x *= -1;
		}else if (rotationMiniMap == 3) {
			ndcxy.xy = -ndcxy.yx;
		}
		
		// Check if PIP mode (visible area not default)
		bool isPip = (pipVisibleArea.x != 0.0 || pipVisibleArea.y != 1.0 || pipVisibleArea.z != 0.0 || pipVisibleArea.w != 1.0);
		
		// For PIP: transform from world-normalized to visible area screen coords AFTER rotation
		if (isPip) {
			// Convert NDC back to [0,1] for transform
			float2 screenPos = ndcxy * 0.5 + 0.5;
			// Map from world [0,1] to screen position based on visible area
			// World position normPos.x in [visL, visR] -> screen [0,1]
			screenPos.x = (normPos.x - pipVisibleArea.x) / (pipVisibleArea.y - pipVisibleArea.x);
			// Flip Y: world Z in [visB, visT] -> screen Y flipped
			screenPos.y = 1.0 - (normPos.y - pipVisibleArea.z) / (pipVisibleArea.w - pipVisibleArea.z);
			// Apply rotation to screen position
			if (rotationMiniMap == 0) {
				screenPos.y = 1.0 - screenPos.y;
			}else if (rotationMiniMap == 1) {
				screenPos.xy = screenPos.yx;
			}else if (rotationMiniMap == 2) {
				screenPos.x = 1.0 - screenPos.x;
			}else if (rotationMiniMap == 3) {
				screenPos.xy = float2(1.0) - screenPos.yx;
			}
			ndcxy = screenPos * 2.0 - 1.0;
			// Scale cone offset for zoom level
			float zoomFactor = 1.0 / max(pipVisibleArea.y - pipVisibleArea.x, 0.001);
			coneOffset *= zoomFactor;
		}
		
		// Add cone offset
		ndcxy += coneOffset;
		
		gl_Position = float4(ndcxy, 0.0, 1.0);
	}
}
};
vertex StartConeOut OriginalStartConeVertex(uint id [[vertex_id]],uint instanceID [[instance_id]],
 const device float4* positions [[buffer(0)]],constant StartBoxUniform& u [[buffer(1)]],
 const device StartConeInstance* instances [[buffer(2)]]) {
 StartConeVertexProgram p;
 p.position=positions[id];p.worldposrad=instances[instanceID].worldposrad;p.teamcolor=instances[instanceID].teamcolor;
 p.v_worldposrad=float4(0); // Unused by the original fragment in minimap mode.
 p.cameraViewProj=u.cameraViewProjection;p.mapSize=u.mapSize;p.pipVisibleArea=u.pipVisibleArea;
 p.isMinimap=u.values.y;p.startPosScale=u.values.x;p.rotationMiniMap=u.parameters.z;
 p.Run();StartConeOut o;o.position=p.gl_Position;o.v_teamcolor=p.v_teamcolor;o.v_worldposrad=p.v_worldposrad;
 if(u.options.y)o.position.z=(o.position.z+o.position.w)*.5;
 return o;
}
fragment float4 OriginalStartConeFragment(StartConeOut in [[stage_in]],constant StartBoxUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 if(!StartBoxAlphaPass(in.v_teamcolor.a,u.options.z,u.values.z))discard_fragment();return in.v_teamcolor;
}
#endif
)StartBoxMSL";
}
