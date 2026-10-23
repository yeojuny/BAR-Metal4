/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeObjectSpotlightData.h"
#include <string>
#include <string_view>
namespace rendering::objectspotlight {
inline constexpr std::string_view Vertex=R"SPOTLIGHT(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack: require
#line 10000

//__ENGINEUNIFORMBUFFERDEFS__

layout (location = 0) in vec3 localPos;

layout (location = 1) in float radius;
layout (location = 2) in float height;
layout (location = 3) in vec4 color;
layout (location = 4) in float startTime;
layout (location = 5) in float expireTime;
layout (location = 6) in uvec4 instData;
layout (location = 7) in vec3 worldPosOverride;

out DataVS {
	vec4 color;
	vec3 localPos;
	float unitID;
	float cameraDistance;
	float lifetimeElapsed;
} outputVars;

uint SO_NODRAW_FLAG = 0;
uint SO_OPAQUE_FLAG = 1;
uint SO_ALPHAF_FLAG = 2;
uint SO_REFLEC_FLAG = 4;
uint SO_REFRAC_FLAG = 8;
uint SO_SHOPAQ_FLAG = 16;
uint SO_SHTRAN_FLAG = 32;
uint SO_DRICON_FLAG = 128;

bool hasDrawFlag(uint drawFlag, uint flag) {
	return (drawFlag & flag) == flag;
}

struct SUniformsBuffer {
	uint composite; // u8 drawFlag; u8 unused1; u16 id;

	uint unused2;
	uint unused3;
	uint unused4;

	float maxHealth;
	float health;
	float unused5;
	float unused6;

	vec4 drawPos;
	vec4 speed;
	vec4[4] userDefined;
};

layout(std140, binding=1) readonly buffer UniformsBuffer {
	SUniformsBuffer uni[];
};

#line 15000
void main() {
	vec3 worldPos = uni[instData.y].drawPos.xyz;

	if (worldPosOverride.x != 0 && worldPosOverride.y != 0  && worldPosOverride.z != 0) {
		worldPos = worldPosOverride;
	}

	float cameraDistance = length(worldPos.xyz - cameraViewInv[3].xyz);

	float effectiveHeight = height * clamp(cameraDistance / 4500, 1.0, 2.5);

	vec2 resultPosXZ = localPos.xz * radius + worldPos.xz;
	vec3 resultPos = vec3(resultPosXZ.x, worldPos.y + localPos.y * effectiveHeight, resultPosXZ.y);

	gl_Position = cameraViewProj * vec4(resultPos, 1.0);

	outputVars.color = color;
	outputVars.localPos = localPos;
	outputVars.unitID = instData.y;
	outputVars.cameraDistance = cameraDistance;

	if (startTime != 0 && expireTime != 0 && startTime != expireTime) {
		// percent of lifetime that has passed so far
		outputVars.lifetimeElapsed = clamp((timeInfo.y - startTime) / (expireTime - startTime), 0.0, 1.0);
	} else {
		outputVars.lifetimeElapsed = 0;
	}
}
)SPOTLIGHT";
inline constexpr std::string_view Fragment=R"SPOTLIGHT(#version 420
#line 20000

//__ENGINEUNIFORMBUFFERDEFS__

in DataVS {
	vec4 color;
	vec3 localPos;
	float unitID;
	float cameraDistance;
	float lifetimeElapsed;
} inputVars;

out vec4 outputColor;

float noise(vec3 pos, float time) {
    float flicker = sin(time * 3.0 + pos.y * 2.0 + cos(pos.x * 4.0 + pos.z * 3.0)) * 0.5;
    float wave1 = sin(time * 2.0 + pos.x * 3.0 + pos.z * 2.0) * 0.5;
    float wave2 = cos(time * 1.5 + pos.y * 2.5 - pos.z * 3.5) * 0.3;
    return flicker + wave1 + wave2;
}

vec3 transformRGB(vec3 c, float v) {
	if (v > 0) {
		return mix(c, vec3(1.0), v);
	} else {
		return mix(c, vec3(0.0), abs(v));
	}
}

#line 25000
void main() {
	float effectiveY = inputVars.localPos.y;
	if (effectiveY < 0) {
		// spotlights below the object fade closer
		effectiveY = clamp(abs(effectiveY) * 4, 0.0, 1.0);
	}
	float t = 0.25 * timeInfo.y;
	float v1 = clamp(0.05 * noise(inputVars.localPos, t), -0.25, 0.25);
	float v2 = clamp(0.5 * noise(inputVars.localPos, t * 2), -0.5, 0.5);

	outputColor.rgba = vec4(
		transformRGB(inputVars.color.rgb, v1),
		inputVars.color.a
		 * (1 + v2)
		 * pow(1 - effectiveY, 4) // more opacity near the unit
		 * clamp(inputVars.cameraDistance / 4500, 0.1, 1.1) // more opacity viewing from far away
		 * pow(1 - inputVars.lifetimeElapsed, 1) // less opacity as it gets closer to expiring
	);
}
)SPOTLIGHT";
inline constexpr std::string_view EngineDefinitions=R"SPOTLIGHT(layout(std140, binding = 0) uniform UniformMatrixBuffer {
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

)SPOTLIGHT";
struct ArchiveSource {std::string_view path;uint64_t hash;};
inline constexpr ArchiveSource ArchiveSources[]={
 {"LuaUI/Widgets/api_object_spotlight.lua",0x3eeea979b28b25b2ULL},
 {"modules/graphics/luashader.lua",0xd291eb46066d7b4aULL},
 {"modules/graphics/instancevbotable.lua",0x8cef105d28038367ULL},
};
inline std::string Expanded(std::string_view raw){
 std::string out(raw);const std::string marker="//__ENGINEUNIFORMBUFFERDEFS__";const auto at=out.find(marker);
 if(at==std::string::npos||out.find(marker,at+marker.size())!=std::string::npos)throw std::runtime_error("Object Spotlight engine uniform anchor differs");
 out.replace(at,marker.size(),EngineDefinitions);return out;
}
inline void ValidateProgram(std::string_view vertex,std::string_view fragment){
 if(vertex!=Expanded(Vertex)||fragment!=Expanded(Fragment))throw std::runtime_error("Object Spotlight requires the exact expanded original31657 vertex/fragment sources");
}
}
