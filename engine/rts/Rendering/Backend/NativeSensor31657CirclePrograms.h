/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <string_view>
#include <array>
#include <cstdint>
namespace rendering::sensor31657CircleProgram {
inline constexpr std::string_view CircleVertex=R"SOURCE(#version 430 core
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack: require

// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  

#line 10000

layout (location = 0) in vec4 circlepointposition;
layout (location = 1) in vec4 radius_params; //x is startradius, pos y is growstarttime, neg y is shrinkstarttime, zw: model mid offset x, z (0 = unit position)
layout (location = 2) in uvec4 instData;

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__

struct SUniformsBuffer {
    uint composite; //     u8 drawFlag; u8 unused1; u16 id;
    
    uint unused2;
    uint unused3;
    uint unused4;

    float maxHealth;
    float health;
    float unused5;
    float unused6;
    
    vec4 drawPos;
    vec4 speed;
    vec4[4] userDefined; //can't use float[16] because float in arrays occupies 4 * float space
};

layout(std140, binding=1) readonly buffer UniformsBuffer {
    SUniformsBuffer uni[];
}; 

#define UNITID (uni[instData.y].composite >> 16)

uniform float teamColorMix = 1.0;
uniform vec4 rangeColor;
#if defined(LINE_THIN_END) && !defined(STENCILPASS)
	uniform float lineWidthPx = 1.0; // the line width set with glLineWidth
#endif

uniform sampler2D heightmapTex;

out DataVS {
	vec4 v_radius_circum_height;
	#ifndef STENCILPASS
		flat vec4 v_blendedcolor;
		#if defined(STIPPLE_RATE) || defined(LINE_THIN_END)
			vec3 v_lineClipXYW; // line centre: lines interpolate varyings along their length only
		#endif
		#ifdef LINE_THIN_END
			float v_lineHalfWidth; // pixels, thinned far from the camera
		#endif
	#endif
};

#line 11000

float heightAtWorldPos(vec2 w){
	vec2 uvhm = heightmapUVatWorldPos(w);
	return textureLod(heightmapTex, uvhm, 0.0).x;
}


void main() {
	
	// Get the center position of each unit. 
	vec4 circleCenterWorldPos = vec4(uni[instData.y].drawPos.xyz, 1.0);
	// sensors sit at the unit's midPos: the model's mid offset turned with the heading (drawPos.w, radians)
	vec2 front = vec2(sin(uni[instData.y].drawPos.w), cos(uni[instData.y].drawPos.w));
	circleCenterWorldPos.xz += front * radius_params.w + vec2(-front.y, front.x) * radius_params.z;

	float nowTime = timeInfo.x + timeInfo.w;
	float sizeFactor;
	if (radius_params.y < -1){ //negative means shrinking
		 sizeFactor = 15.0 - (nowTime + radius_params.y); // size factor is the time since shrink started divided by the shrink duration
	}else{
		 sizeFactor = nowTime - radius_params.y; // size factor is the time since shrink started divided by the shrink duration
	}
	sizeFactor = clamp(sizeFactor / 15.0, 0.0, 1.0); // clamp to [0,1] range
	float circleRadius = radius_params.x * sizeFactor;

	// Early bails if the circle is outside of the screen frustum
	#ifdef VISIBILITYCULLING
		if (SphereInViewSignedDistance(circleCenterWorldPos.xyz, circleRadius) > 0.0){
			gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
			return;
		}
	#endif

	circleCenterWorldPos.y = max(0.0, circleCenterWorldPos.y);

	vec4 circleVertexWorldPos = vec4(circleCenterWorldPos.xyz, 1.0);
	circleVertexWorldPos.xz += circlepointposition.xy * circleRadius;
	

	float groundHeight = heightAtWorldPos(circleVertexWorldPos.xz); //the y component of v_radius_circum_height
	// on the ground (the water surface over water), where the radar preview's sheet lies, so they line up from any angle
	circleVertexWorldPos.y = max(0.0, groundHeight);

	#ifdef STENCILPASS
		// the circlepointposition is zero at the center vertex of the circle, and we will be using the these varyings as a distance from the center
		// for the fragment shader 
		// hack in the additional 16 radius 
		// TODO: why isn't this added BEFORE?


	    circleRadius += 16.0;		


		bool isCenterVertex = (dot(circlepointposition.xy, circlepointposition.xy) < 0.0001) ;
		if (!isCenterVertex) {
			// make the vector pointing from the center to the vertex exactly circleRadius longer! 
			
			vec3 dir = normalize(circleVertexWorldPos.xyz - circleCenterWorldPos.xyz);
			// this is the vector from the center to the vertex, which is now ABOUT circleRadius long, but we want it to be exactly circleRadius  + 16 long


			circleVertexWorldPos.xyz += dir.xyz * 16.0;
			v_radius_circum_height.x = 0;

		}else{
			v_radius_circum_height.x = circleRadius + 16.0; // the center vertex is always circleRadius + 16 long

		}
		
	#endif
	

	gl_Position = cameraViewProj * circleVertexWorldPos;
	#ifndef STENCILPASS // Circle Pass
		// depth tested 16 elmos closer to the camera, so terrain bulging between the vertices doesn't hide the line
		vec3 toCamera = normalize(cameraViewInv[3].xyz - circleVertexWorldPos.xyz);
		vec4 depthTestPos = cameraViewProj * vec4(circleVertexWorldPos.xyz + toCamera * 16.0, 1.0);
		gl_Position.z = depthTestPos.z / depthTestPos.w * gl_Position.w;
		#if defined(STIPPLE_RATE) || defined(LINE_THIN_END)
			v_lineClipXYW = gl_Position.xyw;
		#endif
		#ifdef LINE_THIN_END
			// LINE_THIN_SCALE of the width from LINE_THIN_END elmos from the camera on, easing in from LINE_THIN_START,
			// though not below LINE_MIN_WIDTH pixels unless it was thinner to begin with
			float thinStart = float(LINE_THIN_START);
			float camDist = distance(cameraViewInv[3].xyz, circleVertexWorldPos.xyz);
			float thin = mix(1.0, float(LINE_THIN_SCALE), smoothstep(thinStart, max(float(LINE_THIN_END), thinStart + 1.0), camDist));
			v_lineHalfWidth = 0.5 * max(lineWidthPx * thin, min(lineWidthPx, float(LINE_MIN_WIDTH)));
		#endif

		v_radius_circum_height.z = groundHeight; // store the ground height here for sonar and stuff
		// -- MAP OUT OF BOUNDS
		vec2 mymin = min(circleVertexWorldPos.xz,mapSize.xy - circleVertexWorldPos.xz);
		float inboundsness = min(mymin.x, mymin.y); // how distant the vertex is from the map edge
		inboundsness = 1.0 - clamp(inboundsness*(-0.02),0.0,1.0); // clamp to [0,1] range, and invert it so that the closer to the edge, the lower the value
		v_blendedcolor.rgb = rangeColor.rgb;
		#ifdef USE_TEAMCOLOR
			uint teamIndex = (instData.z & 0x000000FFu); //leftmost ubyte is teamIndex
			vec4 myTeamColor = teamColor[teamIndex];  // We can lookup the teamcolor right here
			v_blendedcolor.rgb = mix(v_blendedcolor.rgb, myTeamColor.rgb, teamColorMix);
		#endif
		v_blendedcolor.a = rangeColor.a ;
		//blendedcolor.a *= 1.0 - clamp(inboundsness*(-0.01),0.0,1.0);
		v_radius_circum_height.w = inboundsness;
		v_radius_circum_height.y = circleRadius * circlepointposition.z * 5.2345; // store the radius in the first component of v_radius_circum_height
	#endif
}
)SOURCE";
inline constexpr std::string_view CircleFragment=R"SOURCE(#version 430
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  

#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack: require

#line 20000

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__

#if !defined(STENCILPASS) && !defined(EXACT_CIRCLES)
	uniform sampler2D losStencilTexture;
#endif

#ifdef EXACT_CIRCLES
	// Every circle of this widget, to clip the lines exactly where they enter another circle: the low resolution
	// stencil texture blurs the corner where two circles cross, cutting a gap into both lines when zoomed out.
	struct SUniformsBuffer {
		uint composite; //     u8 drawFlag; u8 unused1; u16 id;

		uint unused2;
		uint unused3;
		uint unused4;

		float maxHealth;
		float health;
		float unused5;
		float unused6;

		vec4 drawPos;
		vec4 speed;
		vec4[4] userDefined; //can't use float[16] because float in arrays occupies 4 * float space
	};

	layout(std140, binding=1) readonly buffer UniformsBuffer {
		SUniformsBuffer uni[];
	};

	struct SensorCircle {
		vec4 radius_params; // as the vertex shader's instance attributes
		uvec4 instData;
	};

	layout(std430, binding=CIRCLE_SSBO_BINDING) readonly buffer SensorCircles {
		SensorCircle circles[];
	};

	uniform int circleCount;
#endif

in DataVS {
	vec4 v_radius_circum_height;
	#ifndef STENCILPASS
		flat vec4 v_blendedcolor;
		#if defined(STIPPLE_RATE) || defined(LINE_THIN_END)
			vec3 v_lineClipXYW;
		#endif
		#ifdef LINE_THIN_END
			float v_lineHalfWidth;
		#endif
	#endif
};

out vec4 fragColor;

#ifdef EXACT_CIRCLES
	// 0 inside any circle, 1 outside all of them. The circles are the CIRCLE_SEGMENTS-gons the lines are drawn as,
	// centred and sized like in the vertex shader, with the stencil texture's cut: its fan reaches 16 elmos past the
	// circle and fades over 32, which puts the edge 256 / (radius + 32) elmos outside, softened over 0.64 elmos.
	// So zoomed in the lines look as before; the edge is never softer than a pixel.
	float OutsideCircles() {
		// this pixel on the ground (the line's height), along the camera ray
		vec2 ndc = (gl_FragCoord.xy - viewGeometry.zw) / viewGeometry.xy * 2.0 - 1.0;
		vec4 rayEnd = cameraViewProjInv * vec4(ndc, 1.0, 1.0);
		vec3 camPos = cameraViewInv[3].xyz;
		vec3 rayDir = rayEnd.xyz / rayEnd.w - camPos;
		float lineHeight = max(0.0, v_radius_circum_height.z);
		vec2 pos = (camPos + rayDir * ((lineHeight - camPos.y) / rayDir.y)).xz;
		float pixelSize = max(length(dFdx(pos)), length(dFdy(pos))); // elmos

		float nowTime = timeInfo.x + timeInfo.w;
		float segment = 6.2831853 / float(CIRCLE_SEGMENTS);
		float nearest = 1.0; // distance to the nearest circle edge in edge widths, positive outside
		for (int i = 0; i < circleCount; i++) {
			vec4 radius_params = circles[i].radius_params;
			vec4 drawPos = uni[circles[i].instData.y].drawPos;
			// cheap reject: well outside even at full size and the largest mid offset
			float reach = radius_params.x + length(radius_params.zw) + pixelSize + 8.0;
			vec2 toUnit = pos - drawPos.xz;
			if (dot(toUnit, toUnit) > reach * reach) {
				continue;
			}
			vec2 front = vec2(sin(drawPos.w), cos(drawPos.w));
			vec2 center = drawPos.xz + front * radius_params.w + vec2(-front.y, front.x) * radius_params.z;
			float sizeFactor = (radius_params.y < -1.0) ? 15.0 - (nowTime + radius_params.y) : nowTime - radius_params.y;
			float radius = radius_params.x * clamp(sizeFactor / 15.0, 0.0, 1.0);
			vec2 toPos = pos - center;
			// the polygon's radius in this direction; vertices sit at (sin, cos) of multiples of `segment`
			float local = mod(atan(toPos.x, toPos.y), segment) - 0.5 * segment;
			float edgeRadius = (radius + 256.0 / (radius + 32.0)) * cos(0.5 * segment) / cos(local);
			float edgeWidth = max(pixelSize, 0.64 * (radius + 16.0) / (radius + 32.0));
			nearest = min(nearest, (length(toPos) - edgeRadius) / edgeWidth);
		}
		return smoothstep(-0.5, 0.5, nearest);
	}
#endif

void main() {
	// sample the stencil texture to determine if this pixel is visible
	#ifdef STENCILPASS
		// if we are in stencil pass, we just output the blended color
		fragColor.rgba = vec4(vec3(v_radius_circum_height.x / 32), 1.0);
	#else
		#ifdef EXACT_CIRCLES
			float smoothstenc = OutsideCircles();
		#else
			//vec2 stencilUV = fract((v_uv_camdist_radius.xy * vec2(1.0/VSX, 1.0/ VSY) - 0.5)); // adjust UV coordinates if needed
			vec2 stencilUV = gl_FragCoord.xy * vec2(1.0/VSX, 1.0/ VSY); // adjust UV coordinates if needed
			float stencilValue = texture(losStencilTexture, stencilUV ).r;

			// if the stencil value is less than 0.5, this pixel is not visible
			//float smoothstenc = 1.0 - smoothstep(0.47, 0.477, stencilValue);
			float smoothstenc = 1.0 - smoothstep(0.49, 0.51, stencilValue);
		#endif
		fragColor.rgba = vec4(v_blendedcolor.rgb,v_blendedcolor.a *( smoothstenc)  );
		fragColor.a *= v_radius_circum_height.w; // inboundsness

		#ifdef LINE_THIN_END
			// thinner far from the camera: the line is rasterized at its full width, the part beyond the thinned half
			// width is dropped (discarded, so it doesn't mark the stencil that keeps overlapping circles from doubling up)
			vec2 thinCentrePx = (v_lineClipXYW.xy / v_lineClipXYW.z * 0.5 + 0.5) * vec2(VSX, VSY);
			float thinCoverage = clamp(v_lineHalfWidth + 0.5 - distance(gl_FragCoord.xy, thinCentrePx), 0.0, 1.0);
			if (thinCoverage <= 0.0) {
				discard;
			}
			fragColor.a *= thinCoverage;
		#endif

		#ifdef MAX_HEIGHT
			// if the height is above the max height, discard the pixel
			if (v_radius_circum_height.z > MAX_HEIGHT) {
				fragColor.a = 0.0;
			}
		#endif
			
		#ifdef STIPPLE_RATE
			float stipplePhase = v_radius_circum_height.y * STIPPLE_RATE + timeInfo.x*-0.2;
			float phasePerPixel = length(vec2(dFdx(stipplePhase), dFdy(stipplePhase)));
			// negative alpha is not clamped and tints the gaps magenta
			float stipple = max(3.0 * sin(stipplePhase), 0.0);
			// dashes under ~4 px per period alias into moire, so fade them to their mean (3/pi)
			fragColor.a *= mix(stipple, 0.9549297, smoothstep(1.5, 3.0, phasePerPixel));
			// thin the line to the dash length (half a period) when zoomed out, so dashes never turn into bars across it
			vec2 lineCentrePx = (v_lineClipXYW.xy / v_lineClipXYW.z * 0.5 + 0.5) * vec2(VSX, VSY);
			float maxOffsetPx = max(3.14159265 / phasePerPixel, 2.0);
			fragColor.a *= clamp(maxOffsetPx + 0.5 - distance(gl_FragCoord.xy, lineCentrePx), 0.0, 1.0);
		#endif

		// fully transparent (clipped, thinned away, between dashes): don't let it mark the stencil that keeps overlapping
		// lines from doubling up, or it cuts a gap into the lines drawn later where they cross this one
		if (fragColor.a <= 0.0) {
			discard;
		}
	#endif

})SOURCE";
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
inline constexpr std::array<ArchiveSource,7> ArchiveSources{{
 {"LuaUI/Widgets/gui_sensor_ranges_radar.lua","28eb94f8160e60603d655fd26441124473aefee249298f8544a8aa4aa9b7d81d318d8f31ea5dae25299579579b18bf439fe0c29b7bf5edbb39af07f657f0ad6a",5183466503525461133ull},
 {"LuaUI/Widgets/gui_sensor_ranges_sonar.lua","d057d07c821ce6a09c18eb1bf0554e536043e617d94194535468e8636badb02f9286ca39211aa4acfd0333459eecee898f46dd31f072968fa4ac406d2063a66a",18212413414181706717ull},
 {"modules/graphics/luashader.lua","81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436",15173167305575988042ull},
 {"modules/graphics/instancevbotable.lua","bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256",10155353677033079655ull},
 {"LuaUI/Shaders/sensor_ranges_los.vert.glsl","17a061cd48e9988e8518312e881816c0f7550b4a90163c5e7fe8ea1aec4b4958f5e5f5f53dd15bd023f19d1a7738302ddd8c662f8f162d0e12097a14336b565a",12801218352011633310ull},
 {"LuaUI/Shaders/sensor_ranges_los.frag.glsl","1d785d45aa8d6d491075e01ef6d80dc22a1a170fde60b0047477bd0c93fda281de28a9f85dc73d46d74257fba71f4e84c48ff0e35d569cabc931a59aecf82540",6432919883938138873ull},
 {"gamedata/modrules.lua","ed3ea9e3030e1d4eb79aa5ab4cf49a0283858f454b4c83fb108ce676a21290bb71ba5604193d9d2c6494bd58443ca44f7a2b954bda9ec39c5697bc18c1786aa5",11047447410957178612ull},
}};
}
