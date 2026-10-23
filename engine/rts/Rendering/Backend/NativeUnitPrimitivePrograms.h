/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUnitPrimitiveData.h"
#include <array>
#include <string_view>
namespace rendering::unitPrimitiveProgram {
inline constexpr std::uint64_t BuildingWidgetHash=17107388533772402094ULL; // SHA256 3cc117e6b331116d977df1752f65c1080fe97d26e8307ef89e80c4e72e013254
inline constexpr std::uint64_t FeatureWidgetHash=18251267602171962192ULL; // SHA256 a936c989ceaf8050dc5e62739ef028b0ca5e7da34909b0152d3c4ffa7385c826
inline constexpr std::uint64_t TeamWidgetHash=11802421063470133407ULL; // SHA256 58eee18452fd303e15d354952a181f8ac1d0682a2e77ba1111e1f7b61e32a53e
inline constexpr std::uint64_t UnitTrackerWidgetHash=9650774701333630175ULL; // SHA256 261144d178605e590f1118dbeeeb151f342c28195de7c41d95cca3f5c43d39b4
inline constexpr std::uint64_t InstanceHelperHash=11816442776213806391ULL; // SHA256 a502fe58d0948c471858a9acf7dce96e15e5392e3b45385b2dfaebd9b5cc2472
inline constexpr std::uint64_t ShaderHelperHash=2119653433756743315ULL; // SHA256 ce0572dbc01c201ce269b2d2670729427fa883696d4fd3dde96537fc23f0c69e
inline constexpr std::uint64_t PrimitiveHelperHash=15915154361670413775ULL; // SHA256 240b5141a2c2f58fd86e79b172dcf9cfade78acd42794d507026b2fd13cdfcc3
inline constexpr std::uint64_t VertexHash=11956810538909868769ULL; // SHA256 41b75d731266db78c1f89c0c0074e55f9a12506dd9859dd1479fa2f568246677
inline constexpr std::uint64_t GeometryHash=17164912582676741926ULL; // SHA256 55797dab179b4e59b4e2ce56c722e8af0a7dbfe537c33332d7e7a8d7d8cf610a
inline constexpr std::uint64_t FragmentHash=15449294013944329525ULL; // SHA256 353e9f8a3ed04595de3db99fd006fe011aa2845ab3af1120c5cfd20a781d434b
inline constexpr std::uint64_t UnitAtlasHash=1437600297285110498ULL; // SHA256 c6e5d72c467137ba872dbbfb4d3a37ba401a1313b4d53fba5a1af96b0eabf334
inline constexpr std::uint64_t FeatureAtlasHash=13766119380244836037ULL; // SHA256 bc68a17c525859c4d66b93b7e585f8e380445cf3feb280dbc255031b3b6d917f
inline constexpr std::uint64_t QuaternionHash=2444791597314308789ULL;
inline constexpr std::uint64_t SelectedWidgetHash=5778081215354705893ULL; // SHA256 56a3a86176765b0f15c333be1df74bc7425a391c9c4fc1e589b10984238196c8
inline constexpr std::string_view SelectedTexture="luaui/images/solid.png";
inline constexpr std::string_view EnginePrefix=R"DPAT(layout(std140, binding = 0) uniform UniformMatrixBuffer {
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

// glsl rotate convencience funcs: https://github.com/dmnsgn/glsl-rotate

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
//      > 0  – sphere is more distance from the edge of frustrum by that many NDC units
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



// Note that this function does not check the Z or depth of the clip space, but in regular springrts top-down views, this isnt needed either.
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

)DPAT";
inline constexpr std::string_view WaterTemplate=R"DPAT(#define WATERABSORBCOLOR vec3(%f,%f,%f)
#define WATERMINCOLOR vec3(%f,%f,%f)
#define WATERBASECOLOR vec3(%f,%f,%f)
#define SMF_SHALLOW_WATER_DEPTH_INV 0.1

// vertex below shallow water depth --> alpha=1
// vertex above shallow water depth --> alpha=waterShadeAlpha
vec4 waterBlend(float fragmentheight){
	if (fragmentheight>=0) return vec4(0.0);
	vec4 waterBlendResult = vec4(1.0, 1.0, 1.0, 0.0);
	waterBlendResult.rgb = WATERBASECOLOR.rgb;
	waterBlendResult.rgb -= WATERABSORBCOLOR * clamp( -fragmentheight, 0, 1023);
	waterBlendResult.rgb = max(waterBlendResult.rgb, WATERMINCOLOR);
	waterBlendResult.a = clamp(-fragmentheight * SMF_SHALLOW_WATER_DEPTH_INV, 0.0, 1.0);
	return waterBlendResult;
}
)DPAT";
inline constexpr std::array<std::string_view,6> Names{"Ground AO PlatesShader GL4","Ground AO Plates FeaturesShader GL4","teamPlattersShader GL4","selectedUnitsGroundShader GL4","selectedUnitsUnfinishedShader GL4","selectedUnitsWaterShader GL4"};
inline constexpr std::array<std::string_view,2> AtlasNames{"unittextures/decals/unitaoplates_atlas.dds","unittextures/decals_features/featureaoplates_atlas.dds"};
struct NumberDefine {std::string_view name;float value;};
struct TextDefine {std::string_view name,value;};
inline constexpr std::array<NumberDefine,17> BuildingNumbers{{{"ANIMATION",0.0f},{"BILLBOARD",0.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.0500000007f},{"CLIPTOLERANCE",1.10000002f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.660000026f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",1.0f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<TextDefine,6> BuildingTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY","g_uv.w = dataIn[0].v_parameters.w;"},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb* (1.0 + g_uv.w), texcolor.a * g_uv.z);"},{"POST_VERTEX","v_parameters.w = max(-0.2, sin((timeInfo.x + timeInfo.w) * 2.0/30.0 + float(UNITID) * 0.1)) + 0.2; // match CUS glow rate"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<NumberDefine,18> FeatureNumbers{{{"ANIMATION",0.0f},{"BILLBOARD",0.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.0500000007f},{"CLIPTOLERANCE",1.20000005f},{"DISCARD",0.0f},{"FULL_ROTATION",1.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",0.0f},{"INITIALSIZE",0.660000026f},{"MAXVERTICES",4.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",1.0f},{"USETEXTURE",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",512.0f}}};
inline constexpr std::array<TextDefine,6> FeatureTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = vec4(texcolor.rgb, pow(texcolor.a,0.85) * g_uv.z);"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
inline constexpr std::array<NumberDefine,20> TeamNumbers{{{"ANIMATION",0.0f},{"BILLBOARD",0.0f},{"BREATHERATE",30.0f},{"BREATHESIZE",0.0500000007f},{"CLIPTOLERANCE",1.10000002f},{"DISCARD",0.0f},{"FULL_ROTATION",0.0f},{"GROWTHRATE",4.0f},{"HEIGHTOFFSET",3.99000001f},{"INITIALSIZE",0.660000026f},{"MAXVERTICES",64.0f},{"ROTATE_CIRCLES",1.0f},{"TEAMCOLORIZATION",1.0f},{"TRANSPARENCY",0.25f},{"USETEXTURE",0.0f},{"USE_CIRCLES",1.0f},{"USE_CORNERRECT",1.0f},{"USE_QUADS",1.0f},{"USE_TRIANGLES",1.0f},{"ZPULL",256.0f}}};
inline constexpr std::array<TextDefine,6> TeamTexts{{{"POST_ANIM"," "},{"POST_GEOMETRY",""},{"POST_SHADING","fragColor.rgba = fragColor.rgba;"},{"POST_VERTEX","v_color = v_color;"},{"PRE_OFFSET",""},{"USEQUATERNIONS","1"}}};
// Full pinned Selected Units variants, sharing the exact DPAT shader sources.
// TRANSPARENCY/TEAMCOLORIZATION and the POST_SHADING white-mix literal are
// supplied by the original widget configuration, including re-initialization.
inline constexpr auto SelectedNumbers=[] {
 auto values=TeamNumbers;
 for(auto& d:values){
  if(d.name=="ANIMATION" || d.name=="USETEXTURE")d.value=1.0f;
  if(d.name=="HEIGHTOFFSET")d.value=4.0f;
  if(d.name=="INITIALSIZE")d.value=.75f;
 }
 return values;
}();
inline constexpr auto SelectedUnfinishedNumbers=[] {
 auto values=SelectedNumbers;
 for(auto& d:values)if(d.name=="INITIALSIZE")d.value=.9999f;
 return values;
}();
inline constexpr std::string_view SelectedShadingPrefix="fragColor.rgba = vec4(mix(g_color.rgb * texcolor.rgb + addRadius, vec3(1.0), ";
inline constexpr std::string_view SelectedShadingSuffix=") , texcolor.a * TRANSPARENCY + addRadius);";
}
