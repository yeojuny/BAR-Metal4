/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

namespace rendering::metal4::smforiginal {
// A translation of the exact SMF forward, deferred and border sources in NativeSMFTerrainOriginalSource.h.
// Feature and resource-slot definitions are generated from the actual original
// link request. This has no model/CUS shader or approximate WORLD_SHADER path.
inline constexpr const char* SHADERS = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"SMFMSL(
#include <metal_stdlib>
using namespace metal;
struct SMFImage {
	uint4 flags; // depth, flipV, legacy depth mode, fixed-depth compare clamp
	uint4 swizzle;
	float4 parameters; // original LOD bias
};
struct SMFUniform {
	float4x4 modelView, projection, modelViewInverse, shadowMat;
	float4 texGen0, texGen1;
	float4 cameraPos, lightDir, groundAmbient, groundDiffuse, groundSpecular;
	float4 waterMin, waterBase, waterAbsorb, mapHeights;
	float4 splatScales, splatMults, fogColor, fogParameters;
	float4 clipEye[3], inheritedClip;
	uint4 flags; // clip depth/Y bits, clip enable bits, alpha enable, compare
	int4 texSquare; // original uniform xy; zw unused
	float4 alpha;
	SMFImage heightImage, images[16];
};
// Clip distances feed the rasterizer, not fragment stage_in.
struct SMFVarying {
	float4 position [[position]];
	float clipDistance [[clip_distance]] [4];
	float4 world [[user(locn0)]];
	float2 diffuseUV [[user(locn1)]];
	float3 halfDir [[user(locn2)]];
	float fogFactor [[user(locn3)]];
#ifdef NATIVE_SMF_BORDER
	float4 vertexColor [[user(locn4)]];
	float2 detailUV [[user(locn5)]];
#endif
};
struct SMFFragmentInput {
	float4 position [[position]];
	float4 world [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];
	float2 diffuseUV [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
	float3 halfDir [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
	float fogFactor [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
#ifdef NATIVE_SMF_BORDER
	float4 vertexColor [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
	float2 detailUV [[user(locn5), RECOIL_SAMPLE_PERSPECTIVE]];
#endif
};
float SMFChannel(float4 v, uint channel) {
	switch(channel) {
		case 0u:return 0; case 1u:return 1;
		case 0x1903u:return v.r; case 0x1904u:return v.g;
		case 0x1905u:return v.b; default:return v.a;
	}
}
float4 SMFSwizzle(float4 v, constant SMFImage& image) {
	return float4(SMFChannel(v,image.swizzle.x),SMFChannel(v,image.swizzle.y),
		SMFChannel(v,image.swizzle.z),SMFChannel(v,image.swizzle.w));
}
float4 SMFDepthChannels(float value, uint mode) {
	if(mode==0x1906u)return float4(0,0,0,value);
	if(mode==0x8049u)return float4(value);
	if(mode==0x1903u)return float4(value,0,0,1);
	return float4(value,value,value,1);
}
float4 SMFSample2D(texture2d<float> color,sampler state,
	constant SMFImage& image,float2 uv) {
	if(image.flags.y)uv.y=1-uv.y;
	float4 value=color.sample(state,uv,bias(image.parameters.x));
	return SMFSwizzle(value,image);
}
float4 SMFSample2D(depth2d<float> depth,sampler state,
	constant SMFImage& image,float2 uv) {
	if(image.flags.y)uv.y=1-uv.y;
	float4 value=SMFDepthChannels(depth.sample(state,uv,bias(image.parameters.x)),image.flags.z);
	return SMFSwizzle(value,image);
}
bool SMFAlphaCompare(float a,float b,uint op) {
	switch(op){case 0:return false;case 1:return a<b;case 2:return a==b;
		case 3:return a<=b;case 4:return a>b;case 5:return a!=b;case 6:return a>=b;default:return true;}
}
#define SMF_COLOR_ARGUMENT(NAME) TYPE_##NAME NAME [[texture(IMAGE_##NAME)]], sampler NAME##_sampler [[sampler(SLOT_##NAME)]]
#define SMF_SAMPLE(NAME,UV) SMFSample2D(NAME,NAME##_sampler,u.images[SLOT_##NAME],UV)
#define SMF_NORMAL(UV) float2(SMF_SAMPLE(normalsTex,UV).ra)
#ifdef NATIVE_SMF_COMPOSITE
vertex SMFVarying NativeSMFTerrainVertex(uint vid [[vertex_id]],device const packed_float3* positions [[buffer(0)]],constant SMFUniform& u [[buffer(1)]]) {
	SMFVarying out{};float2 xy=float3(positions[vid]).xy;
	out.position=float4(xy,(u.flags.x&1u)?0.5f:0.0f,1.0f);if(u.flags.x&2u)out.position.y=-out.position.y;
	for(uint i=0;i<4;++i)out.clipDistance[i]=1.0f;
	out.diffuseUV=xy*0.5f+0.5f;return out;
}
struct SMFCompositeOutput {float4 color [[color(0)]];float depth [[depth(any)]];};
fragment SMFCompositeOutput NativeSMFTerrainFragment(SMFFragmentInput in [[stage_in]],constant SMFUniform& u [[buffer(1)]],
	SMF_COLOR_ARGUMENT(colorTex),SMF_COLOR_ARGUMENT(depthTex) RECOIL_SAMPLE_ID_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	float depth=SMF_SAMPLE(depthTex,in.diffuseUV).r;float4 color=SMF_SAMPLE(colorTex,in.diffuseUV);
	if(depth>=1.0f||color.a==0.0f)discard_fragment();
	if(u.flags.z&&!SMFAlphaCompare(color.a,u.alpha.x,u.flags.w))discard_fragment();
	return {color,depth};
}
#else
vertex SMFVarying NativeSMFTerrainVertex(uint vid [[vertex_id]],
	device const packed_float3* positions [[buffer(0)]],constant SMFUniform& u [[buffer(1)]],
#ifdef NATIVE_SMF_BORDER
	device const uchar4* colors [[buffer(2)]],
#endif
#if NATIVE_SMF_HEIGHT_DEPTH
	depth2d<float> heightDepth [[texture(1)]],
#else
	texture2d<float> heightColor [[texture(0)]],
#endif
	sampler heightSampler [[sampler(0)]]) {
	// Basic/ROAM provide their actual local vertexPos and source index order.
	// There is no tile expansion, square-range substitution or generated mesh.
	float4 world=float4(float3(positions[vid]),1.0f);
	world.xz+=float2(u.texSquare.xy)*1024.0f;
#ifdef NATIVE_SMF_BORDER
	float2 mapSize=u.texGen0.xy;
	float2 inverseMapSize=u.texGen0.zw;
#else
	float2 inverseMapSize=u.texGen0.xy;
	float2 mapSize=1.0f/inverseMapSize;
#endif
	float2 nudged=world.xz-8.0f*(world.xz*inverseMapSize)+4.0f;
	float2 heightUV=clamp(nudged,float2(8.0f),mapSize-8.0f)*inverseMapSize;
	if(u.heightImage.flags.y)heightUV.y=1-heightUV.y;
	float heightLod=clamp(0.0f,u.heightImage.parameters.y,u.heightImage.parameters.z);
	float4 heightSample;
#if NATIVE_SMF_HEIGHT_DEPTH
	heightSample=SMFDepthChannels(heightDepth.sample(heightSampler,heightUV,level(heightLod)),u.heightImage.flags.z);
#else
	heightSample=heightColor.sample(heightSampler,heightUV,level(heightLod));
#endif
	SMFVarying out;
	float height=SMFSwizzle(heightSample,u.heightImage).x;
#ifdef NATIVE_SMF_BORDER
	world.y=mix(u.mapHeights.x,height,float(world.y==0.0f));
	out.vertexColor=float4(colors[vid])/255.0f;
	out.detailUV=float2(dot(world,float4(0.005f,0,0.005f,0.5f)),dot(world,float4(0,0.005f,0,0.5f)));
#else
	world.y=height;
#endif
	out.world=world;
	float4 eye=u.modelView*out.world;
	float4 glClip=u.projection*eye;
	out.position=glClip;
	if(u.flags.x&1u)out.position.z=(glClip.z+glClip.w)*0.5f;
	if(u.flags.x&2u)out.position.y=-out.position.y;
	for(uint i=0;i<3;++i)out.clipDistance[i]=(u.flags.y&(1u<<i))?dot(eye,u.clipEye[i]):1.0f;
	out.clipDistance[3]=(u.flags.y&8u)?dot(glClip,u.inheritedClip):1.0f;
	out.diffuseUV=out.world.xz/1024.0f-float2(u.texSquare.xy);
#if defined(SMF_ADV_SHADING) && !defined(DEFERRED_MODE)
	float3 viewDir=(u.modelViewInverse*float4(0,0,0,1)).xyz;
	viewDir=normalize(viewDir-out.world.xyz);
	out.halfDir=normalize(u.lightDir.xyz+viewDir);
#else
	// The original standard/deferred fragment stages do not consume halfDir.
	out.halfDir=float3(0);
#endif
#if defined(NATIVE_SMF_BORDER) || defined(DEFERRED_MODE)
	// Border and the original deferred output have no fog expression.
	out.fogFactor=1.0f;
#else
	// The original forward shader computes this even when fixed-function fog is off.
	out.fogFactor=clamp((u.fogParameters.y-length(eye.xyz))*u.fogParameters.w,0.0f,1.0f);
#endif
	return out;
}
#ifdef NATIVE_SMF_BORDER
fragment float4 NativeSMFTerrainFragment(SMFFragmentInput in [[stage_in]],constant SMFUniform& u [[buffer(1)]],
	SMF_COLOR_ARGUMENT(diffuseTex),SMF_COLOR_ARGUMENT(detailsTex) RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	float2 diffuseUV=clamp(in.diffuseUV,float2(1e-2f),float2(1.0f-1e-2f));
	float4 diffuse=SMF_SAMPLE(diffuseTex,diffuseUV)*float4(110.0f/255.0f,110.0f/255.0f,110.0f/255.0f,0.4f);
	float4 details=SMF_SAMPLE(detailsTex,in.detailUV)*2.0f-1.0f;
	float4 color=(diffuse+details)*in.vertexColor;
	if(u.flags.z&&!SMFAlphaCompare(color.a,u.alpha.x,u.flags.w))discard_fragment();
	return color;
}
#else
#if defined(DEFERRED_MODE) || defined(SMF_COMBINED_PASS)
struct SMFDeferredOutput {
	float4 normal [[color(GBUFFER_NORMTEX_IDX)]];
	float4 diffuse [[color(GBUFFER_DIFFTEX_IDX)]];
	float4 specular [[color(GBUFFER_SPECTEX_IDX)]];
	float4 emission [[color(GBUFFER_EMITTEX_IDX)]];
	float4 misc [[color(GBUFFER_MISCTEX_IDX)]];
#ifdef SMF_COMBINED_PASS
	float4 forwardColor [[color(GBUFFER_COLORTEX_IDX)]];
#endif
};
#define SMF_OUTPUT SMFDeferredOutput
#else
#define SMF_OUTPUT float4
#endif
fragment SMF_OUTPUT NativeSMFTerrainFragment(SMFFragmentInput in [[stage_in]],constant SMFUniform& u [[buffer(1)]],
	SMF_COLOR_ARGUMENT(diffuseTex)
#ifdef SMF_ADV_SHADING
	,SMF_COLOR_ARGUMENT(normalsTex)
#else
	,SMF_COLOR_ARGUMENT(shadingTex)
#endif
#if defined(SMF_DETAIL_NORMAL_TEXTURE_SPLATTING)
	,SMF_COLOR_ARGUMENT(splatDetailNormalTex1),SMF_COLOR_ARGUMENT(splatDetailNormalTex2)
	,SMF_COLOR_ARGUMENT(splatDetailNormalTex3),SMF_COLOR_ARGUMENT(splatDetailNormalTex4)
	,SMF_COLOR_ARGUMENT(splatDistrTex)
#elif defined(SMF_DETAIL_TEXTURE_SPLATTING)
	,SMF_COLOR_ARGUMENT(splatDetailTex),SMF_COLOR_ARGUMENT(splatDistrTex)
#else
	,SMF_COLOR_ARGUMENT(detailTex)
#endif
#if defined(HAVE_INFOTEX) && !defined(DEFERRED_MODE)
	,SMF_COLOR_ARGUMENT(infoTex)
#endif
#ifdef SMF_SPECULAR_LIGHTING
	,SMF_COLOR_ARGUMENT(specularTex)
#endif
#if defined(HAVE_SHADOWS) && !defined(DEFERRED_MODE)
	,depth2d<float> shadowTex [[texture(TEX_shadowTex)]],sampler shadowSampler [[sampler(SLOT_shadowTex)]]
	,SMF_COLOR_ARGUMENT(shadowColorTex)
#endif
#if defined(SMF_SKY_REFLECTIONS) && !defined(DEFERRED_MODE)
	,texturecube<float> skyReflectTex [[texture(TEX_skyReflectTex)]],sampler skyReflectSampler [[sampler(SLOT_skyReflectTex)]]
	,SMF_COLOR_ARGUMENT(skyReflectModTex)
#endif
#ifdef SMF_BLEND_NORMALS
	,SMF_COLOR_ARGUMENT(blendNormalsTex)
#endif
#ifdef SMF_LIGHT_EMISSION
	,SMF_COLOR_ARGUMENT(lightEmissionTex)
#endif
#ifdef SMF_PARALLAX_MAPPING
	,SMF_COLOR_ARGUMENT(parallaxHeightTex)
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	float2 diffUV=in.diffuseUV;
	float2 specUV=in.world.xz*u.texGen0.xy;
#if defined(HAVE_INFOTEX) && !defined(DEFERRED_MODE)
	float2 infoUV=in.world.xz*u.texGen1.xy;
#endif
#ifdef SMF_ADV_SHADING
	float2 normalUV=in.world.xz*u.texGen0.zw;
	float3 cameraDir=in.world.xyz-u.cameraPos.xyz;
	float2 nxz=SMF_NORMAL(normalUV);
	float3 normal=float3(nxz.x,sqrt(1.0f-dot(nxz,nxz)),nxz.y);
#endif
#if defined(SMF_BLEND_NORMALS) || defined(SMF_PARALLAX_MAPPING) || defined(SMF_DETAIL_NORMAL_TEXTURE_SPLATTING)
	float3 tTangent=normalize(cross(normal,float3(-1,0,0)));
	float3 sTangent=cross(normal,tTangent);
	float3x3 stnMatrix=float3x3(sTangent,tTangent,normal);
#endif
#ifdef SMF_PARALLAX_MAPPING
	float4 parallax=SMF_SAMPLE(parallaxHeightTex,specUV);
	float heightValue=dot(parallax.rg,float2(255.0f*256.0f,256.0f))/65536.0f;
	float heightOffset=heightValue*parallax.b+parallax.a-0.5f;
	float3 tangentDirection=transpose(stnMatrix)*cameraDir;
	float2 uvOffset=tangentDirection.xy/tangentDirection.z*heightOffset;
	diffUV+=uvOffset/(float(SMF_TEXSQUARE_SIZE)*u.texGen0.xy);
	normalUV+=uvOffset*(u.texGen0.zw/u.texGen0.xy);
	specUV+=uvOffset;
	nxz=SMF_NORMAL(normalUV);
	normal=float3(nxz.x,sqrt(1.0f-dot(nxz,nxz)),nxz.y);
#endif
#ifdef SMF_BLEND_NORMALS
	float4 dtSample=SMF_SAMPLE(blendNormalsTex,normalUV);
	normal=normalize(mix(normal,stnMatrix*(dtSample.xyz*2.0f-1.0f),dtSample.a));
#endif
	float4 detailCol;
#if defined(SMF_DETAIL_NORMAL_TEXTURE_SPLATTING) || defined(SMF_DETAIL_TEXTURE_SPLATTING)
	float4 splatCoords0=in.world.xzxz*u.splatScales.rrgg;
	float4 splatCoords1=in.world.xzxz*u.splatScales.bbaa;
	float4 cofactors=SMF_SAMPLE(splatDistrTex,specUV)*u.splatMults;
#ifdef SMF_DETAIL_NORMAL_TEXTURE_SPLATTING
	float4 detailNormal=(SMF_SAMPLE(splatDetailNormalTex1,splatCoords0.xy)*2.0f-1.0f)*cofactors.r;
	detailNormal+=(SMF_SAMPLE(splatDetailNormalTex2,splatCoords0.zw)*2.0f-1.0f)*cofactors.g;
	detailNormal+=(SMF_SAMPLE(splatDetailNormalTex3,splatCoords1.xy)*2.0f-1.0f)*cofactors.b;
	detailNormal+=(SMF_SAMPLE(splatDetailNormalTex4,splatCoords1.zw)*2.0f-1.0f)*cofactors.a;
	detailNormal.y=max(detailNormal.y,0.01f);
	float strength=min(1.0f,dot(cofactors,float4(1.0f)));
	float detailIntensity=0.0f;
#ifdef SMF_DETAIL_NORMAL_DIFFUSE_ALPHA
	detailIntensity=clamp(detailNormal.a,-1.0f,1.0f);
#endif
	detailCol=float4(detailIntensity);
	normal=normalize(mix(normal,normalize(stnMatrix*detailNormal.xyz),strength));
#else
	float4 splatDetails=float4(SMF_SAMPLE(splatDetailTex,splatCoords0.xy).r,
		SMF_SAMPLE(splatDetailTex,splatCoords0.zw).g,
		SMF_SAMPLE(splatDetailTex,splatCoords1.xy).b,
		SMF_SAMPLE(splatDetailTex,splatCoords1.zw).a)*2.0f-1.0f;
	detailCol=float4(dot(splatDetails,cofactors));
#endif
#else
	detailCol=SMF_SAMPLE(detailTex,in.world.xz*0.02f)*2.0f-1.0f;
#endif
#if defined(SMF_ADV_SHADING) && !defined(DEFERRED_MODE)
	float cosDiffuse=clamp(dot(u.lightDir.xyz,normal),0.0f,1.0f);
	float cosSpecular=clamp(dot(normalize(in.halfDir),normal),0.001f,1.0f);
#endif
	float4 diffuseCol=SMF_SAMPLE(diffuseTex,diffUV);
#ifdef DEFERRED_MODE
	// These five values are the original fragData outputs, before any forward
	// lighting, reflection, info overlay, water absorption or fog operations.
	SMFDeferredOutput out;
	out.normal=float4((normal+float3(1.0f))*0.5f,1.0f);
	out.diffuse=diffuseCol+detailCol;
#ifdef SMF_SPECULAR_LIGHTING
	out.specular=SMF_SAMPLE(specularTex,specUV);
#else
	out.specular=float4(u.groundSpecular.rgb,1.0f);
#endif
#ifdef SMF_LIGHT_EMISSION
	out.emission=SMF_SAMPLE(lightEmissionTex,specUV);
#else
	out.emission=float4(0);
#endif
	out.misc=float4(0);
	// Legacy alpha testing consumes output zero, whose original deferred alpha
	// is 1. Void-{ground,water} shading alpha belongs to forward/combined only.
	if(u.flags.z&&!SMFAlphaCompare(out.normal.a,u.alpha.x,u.flags.w))discard_fragment();
	return out;
#else
#ifdef SMF_SKY_REFLECTIONS
	constant SMFImage& reflectionImage=u.images[SLOT_skyReflectTex];
	float3 reflectDir=reflect(cameraDir,normal);
	float3 reflectCol=SMFSwizzle(skyReflectTex.sample(skyReflectSampler,reflectDir,bias(reflectionImage.parameters.x)),reflectionImage).rgb;
	float3 reflectMod=SMF_SAMPLE(skyReflectModTex,specUV).rgb;
	diffuseCol.rgb=mix(diffuseCol.rgb,reflectCol,reflectMod);
#endif
#ifdef HAVE_INFOTEX
	diffuseCol.rgb+=SMF_SAMPLE(infoTex,infoUV).rgb*u.texGen1.z;
	diffuseCol.rgb-=float3(0.5f)*float(u.texGen1.z==1.0f);
#endif
	float3 shadowCoeff=float3(1.0f);
#ifdef HAVE_SHADOWS
	float4 shadowPos=u.shadowMat*in.world;
	shadowPos.xy+=0.5f;
	// Color's original lookup is NOT projective; depth uses shadow2DProj.
	float3 shadowColor=SMF_SAMPLE(shadowColorTex,shadowPos.xy).rgb;
	constant SMFImage& shadowImage=u.images[SLOT_shadowTex];
	float2 shadowUV=shadowPos.xy/shadowPos.w;
	if(shadowImage.flags.y)shadowUV.y=1.0f-shadowUV.y;
	float reference=shadowPos.z/shadowPos.w;
	if(shadowImage.flags.w)reference=clamp(reference,0.0f,1.0f);
	float shadow=shadowTex.sample_compare(shadowSampler,shadowUV,reference);
	shadowCoeff=mix(float3(1.0f),shadow*shadowColor,u.mapHeights.z);
#endif
	float4 fragColor;
#ifdef SMF_ADV_SHADING
	float4 shade=float4((u.groundAmbient.rgb+u.groundDiffuse.rgb*(cosDiffuse*shadowCoeff))*float(SMF_INTENSITY_MULT),1.0f);
#ifdef SMF_VOID_WATER
	shade.a=float(in.world.y>=0.0f);
#endif
#ifdef SMF_VOID_GROUND
	shade.a=diffuseCol.a;
#endif
#ifdef SMF_WATER_ABSORPTION
	float4 waterShade=float4(u.waterBase.rgb,shade.a);
	if(u.mapHeights.x<=0.0f) {
		float waterAlpha=abs(in.world.y)*0.1f;
		float waterDecay=0.2f+waterAlpha*0.1f;
		float vertexStepHeight=min(1023.0f,-in.world.y);
		float waterLight=min(cosDiffuse*2.0f+0.4f,1.0f);
		waterAlpha=min(1.0f,waterAlpha+float(in.world.y<=-10.0f));
		waterShade.rgb-=u.waterAbsorb.rgb*vertexStepHeight;
		waterShade.rgb=max(u.waterMin.rgb,waterShade.rgb);
		waterShade.rgb*=float(SMF_INTENSITY_MULT)*waterLight;
		waterShade.rgb*=1.0f-waterDecay*(1.0f-shadowCoeff);
		waterShade.rgb=mix(shade.rgb,waterShade.rgb,waterAlpha);
	}
	shade=mix(shade,waterShade,float(in.world.y<0.0f));
#endif
	fragColor=float4((diffuseCol.rgb+detailCol.rgb)*shade.rgb,shade.a);
#else
	fragColor=float4((diffuseCol.rgb+detailCol.rgb)*SMF_SAMPLE(shadingTex,specUV).rgb,diffuseCol.a);
#endif
#ifdef SMF_LIGHT_EMISSION
	float4 emission=SMF_SAMPLE(lightEmissionTex,specUV);
	fragColor.rgb=fragColor.rgb*(1.0f-emission.a)+emission.rgb;
#endif
#ifdef SMF_ADV_SHADING
#ifdef SMF_SPECULAR_LIGHTING
	float4 specular=SMF_SAMPLE(specularTex,specUV);
	float exponent=specular.a*16.0f;
#else
	float4 specular=float4(u.groundSpecular.rgb,1.0f);
	float exponent=u.groundSpecular.w;
#endif
	fragColor.rgb+=specular.rgb*pow(cosSpecular,exponent)*shadowCoeff;
#endif
	fragColor.rgb=mix(u.fogColor.rgb,fragColor.rgb,in.fogFactor);
	if(u.flags.z&&!SMFAlphaCompare(fragColor.a,u.alpha.x,u.flags.w))discard_fragment();
#ifdef SMF_COMBINED_PASS
	SMFDeferredOutput out;out.normal=float4((normal+float3(1.0f))*0.5f,fragColor.a);
	out.diffuse=diffuseCol+detailCol;out.specular=specular;
#ifdef SMF_LIGHT_EMISSION
	out.emission=emission;
#else
	out.emission=float4(0);
#endif
	out.misc=float4(0);out.forwardColor=fragColor;return out;
#else
	return fragColor;
#endif
#endif // DEFERRED_MODE
}
#undef SMF_OUTPUT
#endif // NATIVE_SMF_BORDER
#endif // NATIVE_SMF_COMPOSITE
)SMFMSL";
} // namespace rendering::metal4::smforiginal
