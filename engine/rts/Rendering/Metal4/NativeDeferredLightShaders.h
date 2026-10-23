/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Generated from all original31657 deferred-light shader function bodies.
namespace rendering::metal4::deferredLights {
inline constexpr char SHADER[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"DLMETAL(/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include <metal_stdlib>
using namespace metal;

// All original shaderConfig definitions are prepended by the captured program.
// Transport choices describe actual resolved textures, independently of source
// compile definitions. A source define with value zero is still defined.
#ifndef NATIVE_MAP_DEPTH_COLOR
#define NATIVE_MAP_DEPTH_COLOR 0
#endif
#ifndef NATIVE_MODEL_DEPTH_COLOR
#define NATIVE_MODEL_DEPTH_COLOR 0
#endif
#if NATIVE_MAP_DEPTH_COLOR
using NativeMapDepthTexture = texture2d<float>;
#else
using NativeMapDepthTexture = depth2d<float>;
#endif
#if NATIVE_MODEL_DEPTH_COLOR
using NativeModelDepthTexture = texture2d<float>;
#else
using NativeModelDepthTexture = depth2d<float>;
#endif

struct NativeDeferredLightDrawUniforms {
	float pointbeamcone, attachedtounitID, windX, windZ;
	float radiusMultiplier, intensityMultiplier, nightFactor;
	int screenSpaceShadows;
	uint4 transport;
	float4 framebuffer;
	float4 alpha;
	uint4 imageSwizzle[10];
	uint4 depthMode[3];
	uint4 imageSize[10];
};
struct NativeDeferredLightInstance {
	packed_float4 worldposrad, worldposrad2, lightcolor;
	packed_float4 modelfactor_specular_scattering_lensflare, otherparams, color2;
	uint pieceIndex;
	packed_uint4 instData;
};
struct NativeDeferredLightMatrices {
	float4x4 screenView, screenProj, screenViewProj;
	float4x4 cameraView, cameraProj, cameraViewProj, cameraBillboardView;
	float4x4 cameraViewInv, cameraProjInv, cameraViewProjInv;
	float4x4 shadowView, shadowProj, shadowViewProj;
	float4x4 reflectionView, reflectionProj, reflectionViewProj;
	float4x4 orthoProj01;
	float4x4 mmDrawView, mmDrawProj, mmDrawViewProj;
	float4x4 mmDrawIMMView, mmDrawIMMProj, mmDrawIMMViewProj;
	float4x4 mmDrawDimView, mmDrawDimProj, mmDrawDimViewProj;
};
struct NativeDeferredLightParams {
	packed_float3 rndVec3;
	uint renderCaps;
	float4 timeInfo, viewGeometry, mapSize, mapHeight, fogColor, fogParams, sunDir;
	float4 sunAmbientModel, sunAmbientMap, sunDiffuseModel, sunDiffuseMap;
	float4 sunSpecularModel, sunSpecularMap, shadowDensity, windInfo;
	float2 mouseScreenPos;
	uint mouseStatus, mouseUnused;
	float4 mouseWorldPos;
	// Full original backing is retained by its publication. This is the exact
	// declaration visible to the original31657 shader, not an allocation size.
	float4 teamColor[255];
};
struct SUniformsBuffer {
	uint composite, unused2, unused3, unused4;
	float maxHealth, health, unused5, unused6;
	float4 drawPos, speed, userDefined[4];
};
struct Transform { float4 quat, trSc; };
static_assert(sizeof(NativeDeferredLightDrawUniforms)==448,"Deferred-light draw ABI");
static_assert(sizeof(NativeDeferredLightInstance)==116,"Original light instance stride");
static_assert(sizeof(NativeDeferredLightMatrices)==1664,"Original engine matrix declaration");
static_assert(sizeof(NativeDeferredLightParams)==4368,"Original LuaShader parameter declaration");
static_assert(sizeof(SUniformsBuffer)==128,"Original model uniform stride");
static_assert(sizeof(Transform)==32,"Original quaternion transform stride");
inline Transform NativeMakeTransform(float4 quat, float4 trSc) { return {quat, trSc}; }
inline float4x4 NativeMat4(float d) {
	return float4x4(float4(d,0,0,0),float4(0,d,0,0),float4(0,0,d,0),float4(0,0,0,d));
}
inline float4x4 NativeMat4(float4 a,float4 b,float4 c,float4 d) { return float4x4(a,b,c,d); }
inline float3x3 NativeMat3(float3 a,float3 b,float3 c) { return float3x3(a,b,c); }
inline float3x3 NativeMat3(float4x4 m) { return float3x3(m[0].xyz,m[1].xyz,m[2].xyz); }
template<typename T> inline T NativeMod(T x,T y) { return x-y*floor(x/y); }
template<typename T> inline auto NativeLessThan(T a,T b) { return a<b; }
template<typename T> inline auto NativeGreaterThan(T a,T b) { return a>b; }
template<typename T> inline T NativeMax(T a,T b) { return metal::max(a,b); }
inline float NativeMax(int a,float b) { return metal::max(float(a),b); }
inline float NativeMax(float a,int b) { return metal::max(a,float(b)); }
template<typename T> inline T NativeMin(T a,T b) { return metal::min(a,b); }
inline float NativeMin(int a,float b) { return metal::min(float(a),b); }
inline float NativeMin(float a,int b) { return metal::min(a,float(b)); }
template<typename T> inline T NativeClamp(T x,T a,T b) { return metal::clamp(x,a,b); }
inline float2 NativeClamp(float2 x,float a,float b) { return metal::clamp(x,a,b); }
inline float NativeClamp(float x,int a,int b) { return metal::clamp(x,float(a),float(b)); }
inline float NativeClamp(float x,int a,float b) { return metal::clamp(x,float(a),b); }
inline float NativeClamp(float x,float a,int b) { return metal::clamp(x,a,float(b)); }
template<typename T> inline T NativeStep(T edge,T x) { return metal::step(edge,x); }
inline float NativeStep(float edge,int x) { return metal::step(edge,float(x)); }
inline float NativeStep(int edge,float x) { return metal::step(float(edge),x); }
// GLSL dFdy is measured along the logical bottom-left framebuffer Y axis.
template<typename T> inline T NativeDFdy(T x) { return -dfdy(x); }

inline float2 NativeImageUV(float2 uv,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	if(u.transport.x&(1u<<slot)) uv.y=1.0-uv.y;
	return uv;
}
inline float3 NativeImageUV(float3 uv,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	if(u.transport.x&(1u<<slot)) uv.y=1.0-uv.y;
	return uv;
}
inline float NativeSourceChannel(float4 v,uint channel) {
	switch(channel) {
		case 0: return 0; case 1: return 1;
		case 0x1903: return v.r; case 0x1904: return v.g;
		case 0x1905: return v.b; case 0x1906: return v.a;
		default: return 0;
	}
}
inline float4 NativeLogicalRGBA(float4 value,uint,constant NativeDeferredLightDrawUniforms&) { return value; }
inline float4 NativeLogicalRGBA(float depth,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	uint mode=u.depthMode[slot/4][slot%4];
	return mode==0x1906u ? float4(1,1,1,depth) : mode==0x8049u ? float4(depth) : float4(depth,depth,depth,1);
}
inline float4 NativeSourceSwizzle(float4 v,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	uint4 swizzle=u.imageSwizzle[slot];
	return float4(NativeSourceChannel(v,swizzle.x),NativeSourceChannel(v,swizzle.y),
		NativeSourceChannel(v,swizzle.z),NativeSourceChannel(v,swizzle.w));
}
template<typename T> inline float4 NativeSampleLOD(T image,sampler state,float2 uv,float lod,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	return NativeSourceSwizzle(NativeLogicalRGBA(image.sample(state,NativeImageUV(uv,slot,u),level(lod)),slot,u),slot,u);
}
inline float4 NativeSampleLOD(texture3d<float> image,sampler state,float3 uv,float lod,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	return NativeSourceSwizzle(image.sample(state,NativeImageUV(uv,slot,u),level(lod)),slot,u);
}
inline float4 NativeGatherChannel(texture2d<float> image,sampler state,float2 uv,uint channel,uint,constant NativeDeferredLightDrawUniforms&) {
	switch(channel) {
		case 0: return float4(0); case 1: return float4(1);
		case 0x1903: return image.gather(state,uv,int2(0),component::x);
		case 0x1904: return image.gather(state,uv,int2(0),component::y);
		case 0x1905: return image.gather(state,uv,int2(0),component::z);
		case 0x1906: return image.gather(state,uv,int2(0),component::w);
		default: return float4(0);
	}
}
inline float4 NativeGatherChannel(depth2d<float> image,sampler state,float2 uv,uint channel,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	if(channel==0) return float4(0);
	if(channel==1) return float4(1);
	uint mode=u.depthMode[slot/4][slot%4];
	bool sampleDepth=mode==0x8049u || (mode==0x1906u ? channel==0x1906u : channel!=0x1906u);
	return sampleDepth ? image.gather(state,uv) : float4(1);
}
template<typename T> inline float4 NativeGather(T image,sampler state,float2 uv,int componentIndex,uint slot,constant NativeDeferredLightDrawUniforms& u) {
	float4 gathered=NativeGatherChannel(image,state,NativeImageUV(uv,slot,u),
		u.imageSwizzle[slot][componentIndex],slot,u);
	// A Y reflection reverses the footprint as well as its sample coordinates.
	// Restore GLSL's (0,1),(1,1),(1,0),(0,0) order in logical image space.
	return (u.transport.x&(1u<<slot)) ? gathered.wzyx : gathered;
}
inline int2 NativeImageSize(uint slot,int lod,constant NativeDeferredLightDrawUniforms& u) {
	return int2(max(uint2(1),u.imageSize[slot].xy>>uint(lod)));
}
inline bool NativeDeferredLightAlpha(float value,constant NativeDeferredLightDrawUniforms& u) {
	if(u.alpha.x==0) return true;
	float reference=u.alpha.y;
	switch(uint(u.alpha.z)) {
		case 0: return false; case 1: return value<reference;
		case 2: return value==reference; case 3: return value<=reference;
		case 4: return value>reference; case 5: return value!=reference;
		case 6: return value>=reference; default: return true;
	}
}

struct DeferredLightVarying {
float4 rasterPosition [[position]];
float4 v_worldPosRad [[user(locn0),flat]];
float4 v_worldPosRad2 [[user(locn1),flat]];
float4 v_lightcolor [[user(locn2),flat]];
float4 v_modelfactor_specular_scattering_lensflare [[user(locn3),flat]];
float4 v_otherparams [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
float4 v_position [[user(locn5), RECOIL_SAMPLE_PERSPECTIVE]];
float4 v_noiseoffset [[user(locn6), RECOIL_SAMPLE_PERSPECTIVE]];
float2 v_screenUV [[user(locn7),RECOIL_SAMPLE_NOPERSPECTIVE]];
float v_sourceVisibility [[user(locn8),flat]];
};
struct DeferredLightVertexProgram {
constant NativeDeferredLightMatrices* matrixBuffer;
constant NativeDeferredLightParams* paramsBuffer;
constant NativeDeferredLightDrawUniforms* drawUniforms;
float4 v_worldPosRad;
float4 v_worldPosRad2;
float4 v_lightcolor;
float4 v_modelfactor_specular_scattering_lensflare;
float4 v_otherparams;
float4 v_position;
float4 v_noiseoffset;
float2 v_screenUV;
float v_sourceVisibility;
float4 position,worldposrad,worldposrad2,lightcolor,modelfactor_specular_scattering_lensflare,otherparams,color2;
uint pieceIndex;uint4 instData;int gl_InstanceID;float4 gl_Position;
#if NATIVE_LIGHT_ATTACHED
#if USEQUATERNIONS == 0
device const float4x4* mat;
#else
device const Transform* transforms;
#endif
device const SUniformsBuffer* uni;
#endif
NativeMapDepthTexture mapDepths;sampler mapDepthsSampler;
float4 Sample_mapDepths(float2 uv,float lod){return NativeSampleLOD(mapDepths,mapDepthsSampler,uv,lod,0,*drawUniforms);}
float4 Gather_mapDepths(float2 uv,int componentIndex){return NativeGather(mapDepths,mapDepthsSampler,uv,componentIndex,0,*drawUniforms);}
int2 Size_mapDepths(int lod){return NativeImageSize(0,lod,*drawUniforms);}
NativeModelDepthTexture modelDepths;sampler modelDepthsSampler;
float4 Sample_modelDepths(float2 uv,float lod){return NativeSampleLOD(modelDepths,modelDepthsSampler,uv,lod,1,*drawUniforms);}
float4 Gather_modelDepths(float2 uv,int componentIndex){return NativeGather(modelDepths,modelDepthsSampler,uv,componentIndex,1,*drawUniforms);}
int2 Size_modelDepths(int lod){return NativeImageSize(1,lod,*drawUniforms);}




// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  
#line 5000



//layout locations 1 and 2 contain primitive specific garbage and should not be used














#if USEQUATERNIONS == 0
	
#else
	// Quaternion math functions




uint GetUnpackedValue(uint packedValue, uint byteNum) {
	return (packedValue >> (8u * byteNum)) & 0xFFu;
}

float4 MultiplyQuat(float4 a, float4 b)
{
    return float4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz));
}

float3 RotateByQuaternion(float4 q, float3 v) {
	return 2.0 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2.0 * q.w * cross(q.xyz, v);
}

float4 RotateByQuaternion(float4 q, float4 v) {
	return float4(RotateByQuaternion(q, v.xyz), v.w);
}

float4 InvertNormalizedQuaternion(float4 q) {
	return float4(-q.x, -q.y, -q.z, q.w);
}

float3 ApplyTransform(Transform tra, float3 v) {
	return RotateByQuaternion(tra.quat, v * tra.trSc.w) + tra.trSc.xyz;
}

float4 ApplyTransform(Transform tra, float4 v) {
	return float4(RotateByQuaternion(tra.quat, v.xyz * tra.trSc.w) + tra.trSc.xyz * v.w, v.w);
}

Transform ApplyTransform(Transform parentTra, Transform childTra) {
	return NativeMakeTransform(
		MultiplyQuat(parentTra.quat, childTra.quat),
		float4(
			parentTra.trSc.xyz + RotateByQuaternion(parentTra.quat, parentTra.trSc.w * childTra.trSc.xyz),
			parentTra.trSc.w * childTra.trSc.w
		)
	);
}

Transform InvertTransformAffine(Transform tra) {
	float4 invR = InvertNormalizedQuaternion(tra.quat);
	float invS = 1.0 / tra.trSc.w;
	return NativeMakeTransform(
		invR,
		float4(
			RotateByQuaternion(invR, -tra.trSc.xyz * invS),
			invS
		)
	);
}

float4x4 TransformToMatrix(Transform tra) {
	float qxx = tra.quat.x * tra.quat.x;
	float qyy = tra.quat.y * tra.quat.y;
	float qzz = tra.quat.z * tra.quat.z;
	float qxz = tra.quat.x * tra.quat.z;
	float qxy = tra.quat.x * tra.quat.y;
	float qyz = tra.quat.y * tra.quat.z;
	float qrx = tra.quat.w * tra.quat.x;
	float qry = tra.quat.w * tra.quat.y;
	float qrz = tra.quat.w * tra.quat.z;

	float3x3 rot = NativeMat3(
		float3(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qrz)      , 2.0 * (qxz - qry)      ),
		float3(2.0 * (qxy - qrz)      , 1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qrx)      ),
		float3(2.0 * (qxz + qry)      , 2.0 * (qyz - qrx)      , 1.0 - 2.0 * (qxx + qyy))
	);

	rot *= tra.trSc.w;

	return NativeMat4(
		float4(rot[0]      , 0.0),
		float4(rot[1]      , 0.0),
		float4(rot[2]      , 0.0),
		float4(tra.trSc.xyz, 1.0)
	);
}
float4 SLerp(float4 qa, float4 qb, float t) {
	// Calculate angle between them.
	float cosHalfTheta = dot(qa, qb);

	// Every rotation can be represented by two quaternions: (++++) or (----)
	// avoid taking the longer way: choose one representation
	float s = sign(cosHalfTheta);
	qb *= s;
	cosHalfTheta *= s;
	// now cosHalfTheta is >= 0.0

	// if qa and qb (or -qb originally) represent ~ the same rotation
	if (cosHalfTheta >= (1.0 - 0.005))
		return normalize(mix(qa, qb, t));

	// Interpolation of orthogonal rotations (i.e. cosHalfTheta ~ 0)
	// does not require special handling, however this usually represents
	// "physically impossible" 180 degree turns with infinite speed so perhaps
	// it can be handled in the following (currently disabled) special way
	#if 0
	if (cosHalfTheta <= 0.005)
		return mix(qa, qb, NativeStep(0.5, t));
	#endif

	float halfTheta = acos(cosHalfTheta);

	// both should be divided by sinHalfTheta (calculation skipped),
	// but it makes no sense to do it due to follow up normalization
	float ratioA = sin((1.0 - t) * halfTheta);
	float ratioB = sin((      t) * halfTheta);

	return qa * ratioA + qb * ratioB; // already normalized
}

float4 Lerp(float4 qa, float4 qb, float t) {
	// Ensure shortest path
	if (dot(qa, qb) < 0.0)
		qb = -qb;
	return normalize(mix(qa, qb, t)); // GLSL's mix() = (1 - t) * qa + t * qb
}

Transform SLerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = NativeClamp(a, 0.0, 1.0);
	return NativeMakeTransform(
		SLerp(t0.quat, t1.quat, a),
		mix(t0.trSc, t1.trSc, a)
	);
}


Transform Lerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = NativeClamp(a, 0.0, 1.0);
	return NativeMakeTransform(
		Lerp(t0.quat, t1.quat, a),
		mix(t0.trSc, t1.trSc, a)
	);
}



// This helper function gets the transform that gets you the model-space to world-space transform
#if NATIVE_LIGHT_ATTACHED
Transform GetModelWorldTransform(uint baseIndex)
{
	return Lerp(
		transforms[baseIndex + 0u],
		transforms[baseIndex + 1u],
		paramsBuffer->timeInfo.w
	);
}

// This helper function gets the transform that gets you the piece-space to model-space transform
Transform GetPieceModelTransform(uint baseIndex, uint pieceID)
{
	return Lerp(
		transforms[baseIndex + 2u * (1u + pieceID) + 0u],
		transforms[baseIndex + 2u * (1u + pieceID) + 1u],
		paramsBuffer->timeInfo.w
	);
}

// This helper function gets the transform that gets you the piece-space to world-space transform
Transform GetPieceWorldTransform(uint baseIndex, uint pieceID)
{
	Transform pieceToMModelTX =  GetPieceModelTransform(baseIndex, pieceID);

	Transform modelToWorldTX = GetModelWorldTransform(baseIndex);

	return ApplyTransform(modelToWorldTX, pieceToMModelTX);
}

Transform GetStaticPieceModelTransform(uint baseIndex, uint pieceID)
{
	return transforms[baseIndex + 1u * (pieceID) + 0u];
}
#endif


#endif






#define UNITID (uni[instData.y].composite >> 16)

#line 10000


// = 0; // 0 = point, 1 = beam, 2 = cone

// this uniform needs some extra. If it is 1, then the primitives should point in the -Z direction, and be moved and rotated with the unit itself
// If the unit is not being drawn, it must be switched off
// One should still be able to specify an offset for this.
// our overdraw additional offsets become a problem possibly for this




















#define SNORM2NORM(value) (value * 0.5 + 0.5)

// Soft visibility of a world position against the scene depth, in [0, 1]. A bilinear 2x2 depth comparison with a
// smooth band in view depth, so it changes continuously with sub-pixel camera motion instead of flipping.
float softVisibilityAtWorldPos(float3 worldPos){
    float4 clipPos = matrixBuffer->cameraViewProj * float4(worldPos, 1.0);
    float viewDepth = -(matrixBuffer->cameraView * float4(worldPos, 1.0)).z; // not clipPos.w, which is always 1 for an orthographic projection
    if (viewDepth <= 0.0) return 0.0; // behind the camera
    float2 uv = NativeClamp(SNORM2NORM(clipPos.xy / clipPos.w), 0.0, 1.0);
    float2 subTexel = fract(uv * float2(Size_mapDepths( 0)) - 0.5);
    float4 sceneDepths = NativeMin(Gather_mapDepths( uv, 0), Gather_modelDepths( uv, 0));
    #if (DEPTH_CLIP01 == 0)
        sceneDepths = sceneDepths * 2.0 - 1.0;
    #endif
    // to view depth: for perspective and orthographic projections the inverse's z and w rows do not depend on x and y
    sceneDepths = abs((matrixBuffer->cameraProjInv[2][2] * sceneDepths + matrixBuffer->cameraProjInv[3][2]) / (matrixBuffer->cameraProjInv[2][3] * sceneDepths + matrixBuffer->cameraProjInv[3][3]));
    float band = NativeMax(1.0, viewDepth * 0.003); // elmos, widening with distance as depth precision drops
    float4 visible = smoothstep(float4(-band), float4(band), sceneDepths - viewDepth + band);
    // textureGather order is (0,1) (1,1) (1,0) (0,0)
    return mix(mix(visible.w, visible.z, subTexel.x), mix(visible.x, visible.y, subTexel.x), subTexel.y);
}


void Run()
{
	float time = paramsBuffer->timeInfo.x + paramsBuffer->timeInfo.w;
	
	float lightRadius = worldposrad.w * drawUniforms->radiusMultiplier;
	v_worldPosRad = worldposrad ;
	v_worldPosRad.w = lightRadius;
	
    float4 v_color2 = color2;
	
	float4x4 placeInWorldMatrix = NativeMat4(1.0); // this is unity for non-unitID tied stuff
	#if USEQUATERNIONS == 1 
		Transform tx;
	#endif
	
	// Ok so here comes the fun part, where we if we have a unitID then fun things happen
	// v_worldposrad contains the incoming piece-level offset
	// v_worldPosRad should be after changing to unit-space
	// we have to transform BOTH the center of the light to piece-space
	// and the vertices of the light volume to piece-space
	// we need to go from light-space to world-space 
	float3 lightCenterPosition =  v_worldPosRad.xyz;
	v_lightcolor = lightcolor;
    float selfIllumMod = 1.0;
    
#if NATIVE_LIGHT_ATTACHED
	if (drawUniforms->attachedtounitID > 0){
		#if USEQUATERNIONS == 0 
			float4x4 worldMatrix = mat[instData.x];
			placeInWorldMatrix = worldMatrix;
			if (pieceIndex > 0u) {
				float4x4 pieceMatrix = mat[instData.x + pieceIndex];
				placeInWorldMatrix = placeInWorldMatrix * pieceMatrix;
			} 
		#else
			tx = GetModelWorldTransform(instData.x);
			if (pieceIndex > 0u){
                // Note the pieceIndex is Lua so, 1-based, so we need to subtract 1 to get the correct index
				Transform ty = GetPieceModelTransform(instData.x, pieceIndex - 1);
				tx = ApplyTransform(tx, ty);
			}
			placeInWorldMatrix = TransformToMatrix(tx);
		#endif
		//uint drawFlags = (instData.z & 0x0000100u);// >> 8 ; // hopefully this works
		//if (drawFlags == 0u)  placeInWorldMatrix = NativeMat4(0.0); // disable if drawflag is set to 0
		// disable if drawflag is set to 0, note that we are exploiting the fact that these should be drawn even if unit is transparent, or if unit only has its shadows drawn. 
		// This is good because the tolerance for distant shadows is much greater
		if ((uni[instData.y].composite & 0x00001fu) == 0u )  placeInWorldMatrix = NativeMat4(0.0); 


        uint teamIndex = (instData.z & 0x000000FFu); //leftmost ubyte is teamIndex
        float4 teamCol = paramsBuffer->teamColor[teamIndex];
        if (any(NativeLessThan(lightcolor.rgb, float3(-0.01)))) 
            v_lightcolor.rgb = teamCol.rgb;
        
        // WARNING: TEAMCOLOR AS COLOR2 REQUIRES AT LEAST -100 
        if (any(NativeLessThan(v_color2.rgb, float3(-99.00)))) 
            v_color2.rgb = teamCol.rgb;

               
        selfIllumMod = NativeMax(-0.2, sin(time * 2.0/30.0 + float(UNITID) * 0.1)) + 0.2;
    }
#endif
    // Note that elapsedframes contains the timeOffset as well
    float elapsedframes = time - otherparams.x;
    float lifetime = otherparams.y;
    float sustain = otherparams.z;
    if (lifetime > 0 ){ //lifetime alpha control
        if (sustain >1 ){ // sustain is positive, keep it up for sustain frames, then ramp it down
            v_lightcolor.a = NativeClamp( v_lightcolor.a * ( lifetime - elapsedframes)/(lifetime - sustain ) , 0.0, v_lightcolor.a);
           
        }else{ // sustain is <1, use exp falloff
            v_lightcolor.a = NativeClamp( v_lightcolor.a * exp( -sustain * (elapsedframes) * 100 ) , 0.0, v_lightcolor.a);
           
        }
    }
   
    v_worldPosRad2 = worldposrad2;


    v_modelfactor_specular_scattering_lensflare = modelfactor_specular_scattering_lensflare;
    v_sourceVisibility = 1.0;
    v_otherparams = otherparams;
   
    float4 worldPos = float4(1.0);
    #line 11000
    if (drawUniforms->pointbeamcone < 0.5){ // point
        //scale it and place it into the world
        //Make it a tiny bit bigger *(1.1) cause the blocky sphere is smaller than the actual radius
        // the -1 is for inverting it so we always see the back faces (great for occlusion testing!) (this should be exploited later on!
       
        // this is centered around the target positional offset, and scaled locally
        float3 lightVertexPosition = lightCenterPosition + -1 * position.xyz * lightRadius * 1.1;

		// Add animations
		if (worldposrad2.w == 0.0) { // Why was this 1.0 and not 0.0?
			// This allows deferred projectile lights to 'track' with frameOffset fractional times.
			lightCenterPosition += paramsBuffer->timeInfo.w * worldposrad2.xyz;
			lightVertexPosition += paramsBuffer->timeInfo.w * worldposrad2.xyz;
		} else {
			float3 lightWorldMovement = float3(0.0);


			// Negative thetas mean orbit, positive means linear movement
			if (worldposrad2.w < 0.0){
			// Note: worldposrad2.w is an excellent place to add orbit-style world-placement light animations
			    float xphaseoffset =  0.0; // half pi 1.570796326 
				if (worldposrad2.x == 0.0) xphaseoffset = 0.0; // dont offset Z phase no motion is expected on it
				lightWorldMovement = sin(elapsedframes * 0.2094395 * worldposrad2.xyz + float3(xphaseoffset, 0.0, 0.0 )) * (-1 * worldposrad2.w);

			}else{
			// for positive numbers, we can do linear movement, with acceleration in w
			// its pretty nasty, but hey, it works
				lightWorldMovement = worldposrad2.xyz * ( elapsedframes  +  elapsedframes * elapsedframes * (worldposrad2.w - 1.0) );
			}
			lightCenterPosition += lightWorldMovement;
			lightVertexPosition += lightWorldMovement;
		}  
       
        // transform the vertices to world-space
        lightVertexPosition = (placeInWorldMatrix * float4(lightVertexPosition, 1.0)).xyz;
       
        // transform the center to world-space
        lightCenterPosition = (placeInWorldMatrix * float4(lightCenterPosition, 1.0)).xyz;
       
       
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        if  (drawUniforms->attachedtounitID > 0.5) { // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
           
        }else{ // World-space point lights
            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = NativeClamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
           
 
        }

		v_worldPosRad.xyz = lightCenterPosition;
		if (modelfactor_specular_scattering_lensflare.w > 0.0) v_sourceVisibility = softVisibilityAtWorldPos(lightCenterPosition);
		v_position = float4( lightVertexPosition, 1.0);
		//v_position = vec4(ApplyTransform(tx, lightVertexPosition),1.0); // Doesn't work
	}
    #line 12000
    else if (drawUniforms->pointbeamcone < 1.5){ // beam
        // we will transform along this vector, where Y shall be the upvector
        // our null vector is +X
        float3 centertoend = lightCenterPosition - worldposrad2.xyz;
        float halfbeamlength = length(centertoend);
        // Scale the box to correct size (along beam is Y dir)
        worldPos.xyz = position.xyz * float3( lightRadius ,  NativeStep(position.y, 0) * halfbeamlength + lightRadius, lightRadius );
        //else{
            worldPos.xyz = position.xyz * float3( lightRadius ,  NativeStep(position.y, 0) * halfbeamlength + lightRadius, lightRadius );
            //worldPos.xyz = position.xyz * vec3( lightRadius , halfbeamlength + lightRadius, lightRadius );
            //worldPos.xyz += vec3(50);
            //}
       
        // TODO rotate this box
        float3 oldfw = float3(0,1,0); // The old forward direction is -y
        float3 newfw = normalize(centertoend); // the new forward direction shall be the normal that we want
        float3 newright = normalize(cross(newfw, oldfw)); // the new right direction shall be the vector perpendicular to old and new forward
        float3 newup = normalize(cross(newright, newfw)); // the new up direction shall be the vector perpendicular to new right and new forward
        // TODO: handle the two edge cases where newfw == (oldfw or -1*oldfw)
        float3x3 rotmat = NativeMat3( // assemble the rotation matrix
                newup,
                newfw,
                newright
            );
        worldPos.xyz = rotmat * worldPos.xyz;
       
        // so we now have our rotated box, we need to place it not at the center, but where the piece matrix tells us to
        // or where the lightcenterpos tells us to
       
               
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        // if the cone is not attached to the unit, exploit that direction allows us to smoothen anim
        if (drawUniforms->attachedtounitID < 0.5){ // World Space Light


            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = NativeClamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
        }else{  // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
        }


        // Place the box in the world
        worldPos.xyz += lightCenterPosition;

        v_worldPosRad2.xyz = (placeInWorldMatrix * float4(v_worldPosRad2.xyz, 1.0)).xyz;;
        v_worldPosRad.xyz = (placeInWorldMatrix * float4(lightCenterPosition.xyz, 1.0)).xyz;
        v_worldPosRad.xyz += (v_worldPosRad2.xyz - v_worldPosRad.xyz) * 0.5;
        v_position.xyz = (placeInWorldMatrix * float4(worldPos.xyz, 1.0)).xyz;
    }
    #line 13000
    else if (drawUniforms->pointbeamcone > 1.5){ // cone
        // input cone that has pointy end up, (y = 1), with radius =1, flat on Y=0 plane
        // make it so that cone tip is at 0 and the opening points to -y
        worldPos.xyz = position.xyz;
        worldPos.x *= -1.0; // flip the cone inside out
        worldPos.y = (worldPos.y*1.1 - 1.) * -1;
        //worldPos.y *= 1;
   
        worldPos.xz *= tan(worldposrad2.w); // Scale the flat of the cone by the half-angle of its opening
        worldPos.xyz *= lightRadius * 1.05; // scale it all by the height of the cone, and a bit of extra
       
        // Now our cone is opening forward towards  -y, but we want it to point into the worldposrad2.xyz
        float3 oldfw = float3(0, -1,0); // The old forward direction is -y
        float3 newfw = normalize(worldposrad2.xyz); // the new forward direction shall be the normal that we want
        float3 newright = normalize(cross(newfw, oldfw)); // the new right direction shall be the vector perpendicular to old and new forward
        float3 newup = normalize(cross(newright, newfw)); // the new up direction shall be the vector perpendicular to new right and new forward
        // TODO: handle the two edge cases where newfw == (oldfw or -1*oldfw)
        float3x3 rotmat = NativeMat3( // assemble the rotation matrix
                newup,
                newfw,
                newright
            );
           
       
        float colortime = v_color2.a; // Matches colortime in lightConf for point lights
        // if the cone is not attached to the unit, exploit that direction allows us to smoothen anim
        if (drawUniforms->attachedtounitID < 0.5){ // World Space Light
            lightCenterPosition += worldposrad2.xyz * paramsBuffer->timeInfo.w;


            if (colortime > 0.0){ // For positive colortime
                float colormod = 0;
                // If colortime > 1, then the color will gradually change to v_color2 in colortime number of frames  
                if (colortime > 1.0) {
                    colormod = NativeClamp(elapsedframes/colortime , 0.0, 1.0);
                // If 0 < colortime < 1, then the color will pulsate between original color and v_color2 at a rate of cos( lifetime * 2 * pi * colortime.
                } else {
                    colormod =  cos(elapsedframes * 6.2831853 * colortime ) * 0.5 + 0.5;
                }
                v_lightcolor.rgb = mix(v_lightcolor.rgb, v_color2.rgb, colormod);
            }
        }else{  // unit-attached point lights
            // for point lights, if the colortime is anything sane (>0), then modulate the light with it.
            if (colortime >0.5){
                v_lightcolor.rgb = mix( v_color2.rgb, v_lightcolor.rgb, cos((elapsedframes * 6.2831853) / colortime ) * 0.5 + 0.5);
            } else if (colortime <= -1.0){
                // for point lights, if the colortime is negative, then modulate the light with it
                // Increasing the colortime will make it so that the pulsing effect is lessened
                selfIllumMod = mix(1.0, selfIllumMod, -1.0 / colortime);
                // Pulse the light into the v_color2 via selfIllumMod
                v_lightcolor.rgb = mix(v_color2.rgb, v_lightcolor.rgb, selfIllumMod);
            }
        }
       
        // rotate the cone, and place it into local space
        worldPos.xyz = rotmat * worldPos.xyz + lightCenterPosition;

        // move the cone into piece or world space:
        worldPos.xyz = (placeInWorldMatrix * float4(worldPos.xyz, 1.0)).xyz;
       
        // set the center pos of the light:
        v_worldPosRad.xyz = (placeInWorldMatrix * float4(lightCenterPosition.xyz, 1.0)).xyz;;

        v_worldPosRad2.w = cos(worldposrad2.w); // pass through the cosine to avoid this calc later on
        v_worldPosRad2.xyz = normalize(worldposrad2.xyz); // normalize this here for sanity
       
        // Clear out the translation from the cone direction, and turn the cone according to the piece matrix
        v_worldPosRad2.xyz = NativeMat3(placeInWorldMatrix) * v_worldPosRad2.xyz;

        // Lens flare visibility. The apex usually sits on or inside the emitting piece, so also probe the first
        // stretch of the beam and take the most visible point. Seen from behind, the beam probes would show the
        // flare through the unit, so they fade out as the camera moves behind the source, and are skipped there.
        if (modelfactor_specular_scattering_lensflare.w > 0.0) {
            float sourcevisible = softVisibilityAtWorldPos(v_worldPosRad.xyz);
            float3 conedir = normalize(v_worldPosRad2.xyz);
            float beamweight = smoothstep(-0.2, 0.2, dot(conedir, normalize(matrixBuffer->cameraViewInv[3].xyz - v_worldPosRad.xyz)));
            if (sourcevisible < 1.0 && beamweight > 0.0) {
                float3 probestep = conedir * NativeMin(lightRadius * 0.25, 16.0) * 0.5;
                float beamvisible = NativeMax(softVisibilityAtWorldPos(v_worldPosRad.xyz + probestep), softVisibilityAtWorldPos(v_worldPosRad.xyz + probestep * 2.0));
                sourcevisible = NativeMax(sourcevisible, beamvisible * beamweight);
            }
            v_sourceVisibility = sourcevisible;
        }
       
        v_position =  worldPos;
    }
    #line 14000
   
    //  vec4 windInfo; // windx, windy, windz, windStrength
    v_noiseoffset = float4(drawUniforms->windX, 0, drawUniforms->windZ,0) * (-0.0156);
    v_noiseoffset.a = float(gl_InstanceID); // InstanceID is only avail in vertex shader, and we use this as a unique offset for noise sampling. 
    //v_noiseoffset = vec4(0.0);
    //v_noiseoffset.y = windX + windZ;
   
    gl_Position = matrixBuffer->cameraViewProj * v_position;
    v_screenUV = SNORM2NORM(gl_Position.xy / gl_Position.w);
   
    // pass everything on to fragment shader:

}



};
vertex DeferredLightVarying DeferredLightVertex(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const NativeDeferredLightInstance* instances [[buffer(1)]],constant NativeDeferredLightMatrices& matrices [[buffer(2)]],constant NativeDeferredLightParams& params [[buffer(3)]],constant NativeDeferredLightDrawUniforms& uniforms [[buffer(6)]]
#if NATIVE_LIGHT_ATTACHED
,device const SUniformsBuffer* modelUniforms [[buffer(5)]]
#if USEQUATERNIONS == 0
,device const float4x4* modelTransforms [[buffer(4)]]
#else
,device const Transform* modelTransforms [[buffer(4)]]
#endif
#endif
,NativeMapDepthTexture mapDepths [[texture(0)]],sampler mapDepthsSampler [[sampler(0)]],NativeModelDepthTexture modelDepths [[texture(1)]],sampler modelDepthsSampler [[sampler(1)]]){
DeferredLightVertexProgram program;
program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;
program.mapDepths=mapDepths;program.mapDepthsSampler=mapDepthsSampler;
program.modelDepths=modelDepths;program.modelDepthsSampler=modelDepthsSampler;
program.position=mesh[vertexIndex];
device const NativeDeferredLightInstance& row=instances[instance];
program.worldposrad=float4(row.worldposrad);
program.worldposrad2=float4(row.worldposrad2);
program.lightcolor=float4(row.lightcolor);
program.modelfactor_specular_scattering_lensflare=float4(row.modelfactor_specular_scattering_lensflare);
program.otherparams=float4(row.otherparams);
program.color2=float4(row.color2);
program.pieceIndex=row.pieceIndex;program.instData=uint4(row.instData);program.gl_InstanceID=int(instance-uniforms.transport.z);
#if NATIVE_LIGHT_ATTACHED
program.uni=modelUniforms;
#if USEQUATERNIONS == 0
program.mat=modelTransforms;
#else
program.transforms=modelTransforms;
#endif
#endif
program.Run();
DeferredLightVarying result;result.rasterPosition=program.gl_Position;
if(uniforms.transport.y&1u)result.rasterPosition.z=(result.rasterPosition.z+result.rasterPosition.w)*0.5f;
if(uniforms.transport.y&2u)result.rasterPosition.y=-result.rasterPosition.y;
result.v_worldPosRad=program.v_worldPosRad;
result.v_worldPosRad2=program.v_worldPosRad2;
result.v_lightcolor=program.v_lightcolor;
result.v_modelfactor_specular_scattering_lensflare=program.v_modelfactor_specular_scattering_lensflare;
result.v_otherparams=program.v_otherparams;
result.v_position=program.v_position;
result.v_noiseoffset=program.v_noiseoffset;
result.v_screenUV=program.v_screenUV;
result.v_sourceVisibility=program.v_sourceVisibility;
return result;
}
struct DeferredLightFragmentProgram {
constant NativeDeferredLightMatrices* matrixBuffer;
constant NativeDeferredLightParams* paramsBuffer;
constant NativeDeferredLightDrawUniforms* drawUniforms;
float4 v_worldPosRad;
float4 v_worldPosRad2;
float4 v_lightcolor;
float4 v_modelfactor_specular_scattering_lensflare;
float4 v_otherparams;
float4 v_position;
float4 v_noiseoffset;
float2 v_screenUV;
float v_sourceVisibility;
float4 gl_FragCoord,fragColor;
NativeMapDepthTexture mapDepths;sampler mapDepthsSampler;
float4 Sample_mapDepths(float2 uv,float lod){return NativeSampleLOD(mapDepths,mapDepthsSampler,uv,lod,0,*drawUniforms);}
NativeModelDepthTexture modelDepths;sampler modelDepthsSampler;
float4 Sample_modelDepths(float2 uv,float lod){return NativeSampleLOD(modelDepths,modelDepthsSampler,uv,lod,1,*drawUniforms);}
texture2d<float> mapNormals;sampler mapNormalsSampler;
float4 Sample_mapNormals(float2 uv,float lod){return NativeSampleLOD(mapNormals,mapNormalsSampler,uv,lod,2,*drawUniforms);}
texture2d<float> modelNormals;sampler modelNormalsSampler;
float4 Sample_modelNormals(float2 uv,float lod){return NativeSampleLOD(modelNormals,modelNormalsSampler,uv,lod,3,*drawUniforms);}
texture2d<float> mapDiffuse;sampler mapDiffuseSampler;
float4 Sample_mapDiffuse(float2 uv,float lod){return NativeSampleLOD(mapDiffuse,mapDiffuseSampler,uv,lod,6,*drawUniforms);}
texture2d<float> modelDiffuse;sampler modelDiffuseSampler;
float4 Sample_modelDiffuse(float2 uv,float lod){return NativeSampleLOD(modelDiffuse,modelDiffuseSampler,uv,lod,7,*drawUniforms);}
#if NATIVE_LIGHT_VOLUME
texture3d<float> noise3DCube;sampler noise3DCubeSampler;
float4 Sample_noise3DCube(float3 uv,float lod){return NativeSampleLOD(noise3DCube,noise3DCubeSampler,uv,lod,8,*drawUniforms);}
#endif
texture2d<float> blueNoise;sampler blueNoiseSampler;
float4 Sample_blueNoise(float2 uv,float lod){return NativeSampleLOD(blueNoise,blueNoiseSampler,uv,lod,9,*drawUniforms);}



// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Beherith (mysterme@gmail.com)
// This shader is part of the Beyond All Reason repository.  




#line 30000

















// = 0; // 0 = point, 1 = beam, 2 = cone







// Given a beam between beamStart and beamEnd, returns the pos on the beam closest to desired Point
float3 closestbeam(float3 point, float3 beamStart, float3 beamEnd){
	float3 beamvec = beamEnd -beamStart ;
	float3 pointbeam = point - beamStart;
	float c1 = dot(pointbeam, beamvec);
	float c2 = dot(beamvec, beamvec);
	if (c1 <=0 ) return beamStart;
	if (c2 <=c1) return beamEnd;
	return beamStart + (c1/c2) * beamvec;
}
//https://iquilezles.org/articles/distfunctions/

// Given a ray origin and a direction, returns the closes point on the ray in xyz and the distance in w
float4 closestlightlp_distance (float3 ro, float3 rd, float3 P){
	float t0 = dot(rd, P - ro) / dot(rd, rd);
	float3 intersectPoint = ro + t0 * rd;
	return float4(intersectPoint, length(P - intersectPoint));
}

// Lens flare effect
float LensFlareDistanceSqrt(float3 pointpos, float3 lightpos, float radius){
	float sqrtsum = dot(sqrt(abs(pointpos - lightpos)), float3(1.0));
	return (sqrtsum * sqrtsum) / radius;
}

float distToCapsuleSqr( float3 p, float3 a, float3 b, float r )
{
  float3 pa = p - a, ba = b - a;
  float h = NativeClamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
  pa =  pa - ba*h;
  return dot(pa, pa) ;
}

// marginally faster, about the cost as a single octave of perlin
float4 ray_to_capsule_distance_squared(float3 rayOrigin, float3 rayDirection, float3 cap1, float3 cap2){ // point1, dir1, beamstart, beamend
	// returns the squared distance of the ray and the line segment in w
	// returns the closest point on beam in xyz
	float rd_dot_rd_inv = 1.0 / dot(rayDirection, rayDirection);
	
	float t1 = dot(rayDirection, cap1 - rayOrigin) * rd_dot_rd_inv;
	float3 intersectPoint1 = rayOrigin + t1 * rayDirection;
	
	float t2 = dot(rayDirection, cap2 - rayOrigin) * rd_dot_rd_inv;
	float3 intersectPoint2 = rayOrigin + t2 * rayDirection;
	
	float3 cap2tocap1 = cap2 - cap1;

	float3 interSectToC2 = cap2 - intersectPoint2;
	float3 interSectToC1 = cap1 - intersectPoint1;	

	float angle1 = dot(cap2tocap1, interSectToC1);
	float angle2 = dot(cap2tocap1, interSectToC2);
	
	float3 connectornormal = cross(rayDirection, cap2tocap1); // n is the normal of the line connecting them
	float dd = dot(connectornormal, rayOrigin - cap1); // this is the angle between the connector normal and 
	float sqdistline = dd*dd / dot(connectornormal, connectornormal); // sqdistline; 

	float distcap2 = dot(interSectToC2, interSectToC2) ;
	float distcap1 = dot(interSectToC1, interSectToC1) ;  //sqdistends
	
	float3 closestpointbeam = cap1 + cap2tocap1 * sqrt(abs(distcap1 - sqdistline)/dot(cap2tocap1,cap2tocap1));
	float4 finalposanddist = mix(float4(cap1, distcap1) ,
		float4(cap2, distcap2) ,
		NativeStep(distcap2, distcap1));
		
	if (angle1 < 0 && angle2 > 0){ // this means that our ray is hitting between the caps
		finalposanddist.w = sqdistline;
		finalposanddist.xyz = closestpointbeam;
	}
	finalposanddist.w = sqrt(finalposanddist.w);
	return finalposanddist;
}
//http://www.realtimerendering.com/intersections.html

//https://iquilezles.org/articles/intersectors/
// capsule defined by extremes pa and pb, and radious ra
// Note that only ONE of the two spherical caps is checked for intersections,
// which is a nice optimization
float capIntersect( float3 ro, float3 rd, float3 pa, float3 pb, float ra )
{
    float3  ba = pb - pa;
    float3  oa = ro - pa;
    float baba = dot(ba,ba);
    float bard = dot(ba,rd);
    float baoa = dot(ba,oa);
    float rdoa = dot(rd,oa);
    float oaoa = dot(oa,oa);
    float a = baba      - bard*bard;
    float b = baba*rdoa - baoa*bard;
    float c = baba*oaoa - baoa*baoa - ra*ra*baba;
    float h = b*b - a*c;
    if( h >= 0.0 )
    {
        float t = (-b-sqrt(h))/a;
        float y = baoa + t*bard;
        // body
        if( y>0.0 && y<baba ) return t;
        // caps
        float3 oc = (y <= 0.0) ? oa : ro - pb;
        b = dot(rd,oc);
        c = dot(oc,oc) - ra*ra;
        h = b*b - c;
        if( h>0.0 ) return -b - sqrt(h);
    }
    return -1.0;
}

// capsule defined by extremes pa and pb, and radious ra
// Note that only ONE of the two spherical caps is checked for intersections,
// which is a nice optimization


//https://www.symbolab.com/solver/definite-integral-calculator/%5Cint_%7B0%7D%5E%7B1%7D%5Cleft(1-%5Cfrac%7B%5Cleft(A_%7B2%7D%2BB_%7B2%7D%5Ccdot%20x%5Cright)%7D%7Bh%5E%7B2%7D%7D%20%5Cright)%5E%7B%20%7D%5Ccdot%5Cleft(1-%5Cfrac%7B%5Csqrt%7B%5Cleft(A_%7B1%7D%2BB_%7B1%7D%5Ccdot%20x%5Cright)%5E%7B2%7D%7D%7D%7B%5Cfrac%7B%5Cleft(A_%7B2%7D%2BB_%7B2%7D%5Ccdot%20x%5Cright)%5Ccdot%20r%7D%7Bh%7D%7D%5Cright)%20dx?or=input

float dot2(float3 a){ return dot(a,a);}

// Defines the falloff function of scattered light one gets more distant from the light source
float scatterfalloff(float disttolight, float radius){
	float x = NativeClamp(1.0 - disttolight/radius, 0.0, 1.0);
	return (-0.5*x*x+x)/(0.5*x*x-x+1);
}

// Integratates the scattering from closes to eye pos to most distant pos
float integratescatterocclusion(float depthratio){
	float x = NativeClamp(depthratio, 0.0, 1.0);
	return (x*x)/ (2*x*x -2*x +1.0);
}

// cone defined by extremes pa and pb, and radious ra and rb
// Only one square root and one division is employed in the worst case. dot2(v) is dot(v,v)

// Intersects a cone with a ray, with an optional occluding fragment distance
// Returns in xy the close and far distance cone intersection distances
// Note that the close one will be negative if the if the ray origin is inside the cone
// in z returns Rayleigh scattered light amount 
// in w returns Mie scattered light amount

#line 30400
float4 halfconeIntersectScatter( float3  rayOrigin, float3 rayDirection, float3 coneTip, float3 coneDirection, float coneHeight, float bottomRadius, float fragmentdistance)
{
	float3  coneFlatEnd = coneTip + coneDirection * coneHeight;
	float3  TipToEnd = coneFlatEnd - coneTip;
	float3  TipToEye = rayOrigin - coneTip;
	float3  EndToEye = rayOrigin - coneFlatEnd;
	float ConeHeightSqr = dot(TipToEnd, TipToEnd); 
	float EyeDotEnd = dot(TipToEye, TipToEnd);
	float RayDotEnd = dot(rayDirection, TipToEnd);
	float RayDotEye = dot(rayDirection, TipToEye);
	float TipToEyeSqr = dot(TipToEye, TipToEye);
	float TipDotEnd = dot(EndToEye, TipToEnd); 
	float4 coneflatdistances = float4(-1.0);

	// Bottom Cap collision, note that we still need to check this if we want the exit point too!
	// a ray can only collide with the end once!
	float distToFlat = - TipDotEnd / RayDotEnd;                     // NOT delayed division
	if( dot2(EndToEye+rayDirection*distToFlat)<=(bottomRadius*bottomRadius) )
		coneflatdistances.zw = float2(distToFlat);
		// vec3 normal = TipToEnd*inversesqrt(ConeHeightSqr)
	
	// Test Cone Body intersections
	float CapeLengthSqr = ConeHeightSqr + bottomRadius * bottomRadius;
	float k2 = ConeHeightSqr * ConeHeightSqr - RayDotEnd * RayDotEnd * CapeLengthSqr;
	float k1 = ConeHeightSqr * ConeHeightSqr * RayDotEye - EyeDotEnd * RayDotEnd * CapeLengthSqr ;
	float k0 = ConeHeightSqr*ConeHeightSqr*TipToEyeSqr - EyeDotEnd * EyeDotEnd * CapeLengthSqr ;
	float h = k1*k1 - k2*k0;
	if( h<0.0 ) return float4(-1.0, -1.0, 0.0, 0.0); //no intersection as we are _above_ the cone tip
	float sqrth = sqrt(NativeMax(0,h));
	float CloseDistToCone = (-k1-sqrth)/k2;
	float FarDistToCone = (-k1+sqrth)/k2;
	float CloseY = EyeDotEnd + CloseDistToCone * RayDotEnd;
	float FarY = EyeDotEnd + FarDistToCone * RayDotEnd;
	
	// if we are within the cone from the front
	if( CloseY >=0.0 && CloseY <= ConeHeightSqr ) coneflatdistances.x = CloseDistToCone;
	else {
		coneflatdistances.x = NativeMax(coneflatdistances.x,coneflatdistances.z);//no intersection, as we are below the bottom of the cone
	}
	// if we are within the cone from the back
	if( FarY >=0.0 && FarY <= ConeHeightSqr ) coneflatdistances.y = FarDistToCone; //no intersection, as we are below the bottom of the cone
	else {
		coneflatdistances.y = NativeMax(coneflatdistances.y,coneflatdistances.z);
	}
	 // vec3 normal =normalize(ConeHeightSqr*(ConeHeightSqr*(TipToEye+CloseDistToCone*rayDirection))-TipToEnd*CapeLengthSqr*y)
	// Bail early if we are not on the cone
	if (abs(coneflatdistances.y - coneflatdistances.x) < 0.001) return float4(-1, -1, 0, 0);
	 
	// try to get some sort of falloff factor
	// calc angle between entry and exit
	float3 EntryPoint = rayDirection * coneflatdistances.x + rayOrigin;
	float3 ExitPoint =  rayDirection * coneflatdistances.y + rayOrigin;
	
	float3 stepVec = rayDirection * ( coneflatdistances.y  - coneflatdistances.x) / RAYMARCHSTEPS;
	float stepVecSqr = dot(stepVec, stepVec);
	
	float cosTheta = sqrt(ConeHeightSqr / CapeLengthSqr);
	float oneminuscosThetainv = 1.0 / (1.0 - cosTheta);
	float ConeHeightSqrInv = 1.0 / ConeHeightSqr;
	float rayleighScatterSum = 0;
	float miescattersum = 0;

	for (int i = 1; i < RAYMARCHSTEPS; i++){
		// step the ray forward 
		float3 marchPos = stepVec * i + EntryPoint;

		// Sample 3D noise if needed (noise should be 0.5 centered)
		#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
			float noise = Sample_noise3DCube( fract(marchPos * 0.005 + 0 / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0	).r * 2.0 ;
		#else
			float noise = 1.0;
		#endif
		
		// Calculate the distatten (which is squared)
		float distatten = NativeClamp((1.0 - dot(marchPos - coneTip, marchPos - coneTip) * ConeHeightSqrInv), 0.0, 1.0);
		
		float3 tiptomarchnorm = normalize(marchPos - coneTip);
		float falloffatten = NativeClamp(1.0 - (1.0-dot(coneDirection, tiptomarchnorm))* oneminuscosThetainv, 0.0,1.0);
		
		float rayleighhere = distatten*falloffatten * noise;
		rayleighScatterSum += rayleighhere;
		// as mie mainly scatters forward, we will need to dot raydirection with lightdirection
		float miefactor = NativeClamp(dot(-tiptomarchnorm, rayDirection) * 10.0 -9 , 0.0,1.0);
		//rayleighScatterSum += miefactor * falloffatten;
		miescattersum += rayleighhere * miefactor ;
	}
	
	// Simplest occlusion calculation just integrates scatter in between fragment distance and eye distance
	// cone close distance is negative if rayorigin is inside it
	float tofrag = integratescatterocclusion((fragmentdistance - coneflatdistances.x ) / (coneflatdistances.y - coneflatdistances.x));
	
	float fromeye = integratescatterocclusion( ( -coneflatdistances.x ) / (coneflatdistances.y - coneflatdistances.x));
	
	
	rayleighScatterSum = (tofrag - fromeye) * rayleighScatterSum / (RAYMARCHSTEPS - 1);
	miescattersum = (tofrag - fromeye) * miescattersum / (RAYMARCHSTEPS - 1);

	return float4(coneflatdistances.x, coneflatdistances.y, rayleighScatterSum, miescattersum);
	
}

// https://gist.github.com/wwwtyro/beecc31d65d1004f5a9d
float2 raySphereIntersect(float3 r0, float3 rd, float3 s0, float sr) {
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersection with sphere,
    //   or -1.0 if no intersection.
    float a = dot(rd, rd);
    float3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
	float disc = b * b - 4.0 * a* c;
    if (disc < 0.0) {
        return float2(-1.0, -1.0);
    }else{
		disc = sqrt(disc);
		return float2(-b - disc, -b + disc) / (2.0 * a);
	}
}
// This is the fast approx scattering using a mierayleighratio, where 1.0 = Rayleigh, ~0.1 = Mie
// TODO: handle the case where viewpos is inside the volume!
float SlowSphereRayMarchedScattering(float3 campos, float3 viewdirection, float3 lightposition, float lightradius, float fragmentdistance, float lightdistance, float mierayleighratio){
	float2 closeandfardistance = raySphereIntersect(campos, -viewdirection,  lightposition, lightradius * mierayleighratio);
	
	float noise = 1.0;
	#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
		float3 EntryPoint = (-viewdirection) * closeandfardistance.x + campos;
		//vec3 ExitPoint =  (viewdirection) * closeandfardistance.y + campos;
		float3 stepVec =  (-viewdirection) * ( closeandfardistance.y  -  closeandfardistance.x) / RAYMARCHSTEPS;
		for (int i = 1; i < RAYMARCHSTEPS; i++){
			float3 marchPos = stepVec * i + EntryPoint;
			noise += Sample_noise3DCube( fract(marchPos * 0.005 + i / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0).r * 2.0 ;
		}
		noise = noise / (RAYMARCHSTEPS -1);
	#endif
	
	float depthratio = NativeClamp(0.5 * (fragmentdistance - closeandfardistance.x) / (lightradius * mierayleighratio), 0.0, 1.0);
	float tofrag = integratescatterocclusion(depthratio);
	float fromeye = integratescatterocclusion( ( -closeandfardistance.x ) / (closeandfardistance.y - closeandfardistance.x));
	return scatterfalloff(lightdistance, lightradius * mierayleighratio) * (tofrag - fromeye) * noise;
}

float FastApproximateScattering(float3 campos, float3 viewdirection, float3 lightposition, float lightradius, float fragmentdistance, float lightdistance, float mierayleighratio){
	float2 closeandfardistance = raySphereIntersect(campos, -viewdirection,  lightposition, lightradius * mierayleighratio);
	float depthratio = NativeClamp(0.5 * (fragmentdistance - closeandfardistance.x) / (lightradius * mierayleighratio), 0.0, 1.0);
	float tofrag = integratescatterocclusion(depthratio);
	float fromeye = integratescatterocclusion( ( -closeandfardistance.x ) / (closeandfardistance.y - closeandfardistance.x));
	return scatterfalloff(lightdistance, lightradius * mierayleighratio) * (tofrag - fromeye);
}


// Works fine, but 16 FMA

// Works fine, but 16 FMA
float3 WorldToScreen(float3 worldCoords){ // returns screen UV and depth position
	float4 screenPosition = matrixBuffer->cameraViewProj * float4(worldCoords,1.0);
	return screenPosition.xyz / screenPosition.w;
}

// Interesting function, takes two depths in screen space, and given 
// nxp2 = 2 * nearPlane * farplane 
// fpn = nearPlane * farPlane
// fmn = nearPlane - farPlane
// return the delta in elmos between the two depths
float depthToDeltaElmos(float depthCloser, float depthFarther, float nxp2, float fpn, float fmn){
	return 36.0 * ( nxp2 / (fpn - (depthCloser* 2.0 - 1.0) * fmn) - nxp2 / (fpn - (depthFarther * 2.0 - 1.0) * fmn));
}

// Additional notes and reading:
//https://andrew-pham.blog/2019/10/03/volumetric-lighting/ ???

#ifdef SCREENSPACESHADOWS
	// See Shader Amortization using Pixel Quad Message Passing
	float2 quadVector = float2(0); // REQUIRED, contains the [-1,1] mappings
	// one-hot encoding of thread ID
	float4 threadMask = float4(0); // contains the thread ID in one-hot
	#define selfWeightFactor 0.07
	float4 selfWeights = float4(0.25) + float4(selfWeightFactor, selfWeightFactor/ -3.0, selfWeightFactor/ -3.0, selfWeightFactor/-3.0);

	float4 quadGetThreadMask(float2 qv){ 
		float4 threadMask =  NativeStep(float4(qv.xy,0,0),float4( 0,0,qv.xy));
		return threadMask.xzxz * threadMask.yyww;
	}

	float4 get_quad_vector_naive(float4 output_pixel_num_wrt_uvxy)
	{
		//  Requires:   Two measures of the current fragment's output pixel number
		//              in the range ([0, IN.output_size.x), [0, IN.output_size.y)):
		//              1.) output_pixel_num_wrt_uvxy.xy increase with uv coords.
		//              2.) output_pixel_num_wrt_uvxy.zw increase with screen xy.
		//  Returns:    Two measures of the fragment's position in its 2x2 quad:
		//              1.) The .xy components are its 2x2 placement with respect to
		//                  uv direction (the origin (0, 0) is at the top-left):
		//                  top-left     = (-1.0, -1.0) top-right    = ( 1.0, -1.0)
		//                  bottom-left  = (-1.0,  1.0) bottom-right = ( 1.0,  1.0)
		//                  You need this to arrange/weight shared texture samples.
		//              2.) The .zw components are its 2x2 placement with respect to
		//                  screen xy direction (IN.position); the origin varies.
		//                  quad_gather needs this measure to work correctly.
		//              Note: quad_vector.zw = quad_vector.xy * float2(
		//                      ddx(output_pixel_num_wrt_uvxy.x),
		//                      ddy(output_pixel_num_wrt_uvxy.y));
		//  Caveats:    This function assumes the GPU driver always starts 2x2 pixel
		//              quads at even pixel numbers.  This assumption can be wrong
		//              for odd output resolutions (nondeterministically so).
		float4 pixel_odd = fract(output_pixel_num_wrt_uvxy * 0.5) * 2.0;
		float4 quad_vector = pixel_odd * 2.0 - float4(1.0);
		return quad_vector;
	}

	float4 get_quad_vector(float4 output_pixel_num_wrt_uvxy)
	{
		//  Requires:   Same as get_quad_vector_naive() (see that first).
		//  Returns:    Same as get_quad_vector_naive() (see that first), but it's
		//              correct even if the 2x2 pixel quad starts at an odd pixel,
		//              which can occur at odd resolutions.
		float4 quad_vector_guess =
			get_quad_vector_naive(output_pixel_num_wrt_uvxy);
		//  If quad_vector_guess.zw doesn't increase with screen xy, we know
		//  the 2x2 pixel quad starts at an odd pixel:
		float2 odd_start_mirror = 0.5 * float2(dfdx(quad_vector_guess.z),
													NativeDFdy(quad_vector_guess.w));
		return quad_vector_guess * odd_start_mirror.xyxy;
	}


	float4 quadGather(float inputthis){
		float inputadjx = inputthis - dfdx(inputthis) * quadVector.x;
		float inputadjy = inputthis - NativeDFdy(inputthis) * quadVector.y;
		float inputdiag = inputadjx - NativeDFdy(inputadjx) * quadVector.y;
		return float4(inputthis, inputadjx, inputadjy, inputdiag);
	}


#endif

#line 31000
void Run()
{
	
	float mapdepth = Sample_mapDepths( v_screenUV, 0.0).x;
	float modeldepth = Sample_modelDepths( v_screenUV, 0.0).x;
	float worlddepth = NativeMin(mapdepth, modeldepth);
	float4 normals = float4(0, 1, 0, 0); // points up by default
	float4 targetcolor = float4(0); // target color of the surface
	float ismodel = 0;
	
	// Only query the textures if the backface of the volume is further than the world fragment
	if (gl_FragCoord.z > worlddepth) {
		if (modeldepth < mapdepth) { // We are processing a model fragment
			ismodel = 1;
			normals =  Sample_modelNormals( v_screenUV, 0.0) * 2.0 - 1.0;
			targetcolor = Sample_modelDiffuse( v_screenUV, 0.0);
			
		}else{
			normals =  Sample_mapNormals( v_screenUV, 0.0) * 2.0 - 1.0;
			targetcolor = Sample_mapDiffuse( v_screenUV, 0.0) * drawUniforms->nightFactor;
		}
	}
	normals.xyz = normalize(normals.xyz);
	
	
	float4 fragWorldPos =  float4( float3(v_screenUV.xy * 2.0 - 1.0, worlddepth),  1.0);
	fragWorldPos = matrixBuffer->cameraViewProjInv * fragWorldPos;
	fragWorldPos.xyz = fragWorldPos.xyz / fragWorldPos.w; // YAAAY this works!
	
	float3 camPos = matrixBuffer->cameraViewInv[3].xyz;
	float fragDistance = length(camPos - fragWorldPos.xyz);
	float3 viewDirection = (camPos - fragWorldPos.xyz) / fragDistance; // vector pointing in the direction of the eye ray
	
	float lightRadius = v_worldPosRad.w;
	float lightRadiusInv = 1.0 / lightRadius;
	float3 lightDirection = float3(0); // the normalized vector from the light source to the fragment
	float3 lightToWorld = float3(0); // The vector pointing from light source to fragment world pos
	float3 lightPosition = float3(0); // This is the position of the light illuminating the fragment
	float3 lightEmitPosition = float3(0); // == lightPosition, except for beams, where the lightEmitPosition for a ray is different than for a world fragment
	
	float raytolightdistance = 0;
	float4 closestpoint_dist = float4(0); // the point that is closest to the light source (xyz) and the distance to it (w)
	
	// Lighting components we wish to collect along the way:
	float attenuation = 0; // Just the distance from the light source (multiplied with falloff for cones
	float falloff = 1; // only cone light have this
	float lensFlare = 0;
	
	float scatteringRayleigh = 0; // the integration of the light reflected into the eye from the eye ray through the volume
	
	float scatteringMie = 0; // the integration of the light reflected into the eye from the eye ray through a size-reduced volume
	float diffuse = 0; // The amount of diffuse reflection from the world-hitting fragment
	float specular = 0; // The amount of specular reflection from the world-hitting fragment
	
	float shadowedness = 0; // how much the light source is screen-space shadowed

	#line 32000
	if (drawUniforms->pointbeamcone < 0.5){ //point
		lightPosition = v_worldPosRad.xyz;
		lightEmitPosition = lightPosition;
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		lightDirection = normalize(lightToWorld);
		
		attenuation = NativeClamp( 1.0 - length (lightToWorld) * lightRadiusInv, 0,1);
		
		// example of exponential attenuation instead of clamped,
		//Use a smoother distance-falloff formula such as an exponential or inverse-square:
		//float dist = length(lightToWorld);
		//attenuation = exp(-dist * 0.5);
		
		closestpoint_dist = closestlightlp_distance(camPos, viewDirection, lightPosition);
		
		// Both scattering components
		scatteringRayleigh = SlowSphereRayMarchedScattering(camPos, viewDirection, lightPosition, lightRadius, fragDistance, closestpoint_dist.w, 1.0 );
		
		scatteringMie = FastApproximateScattering(camPos, viewDirection, lightPosition, lightRadius, fragDistance, closestpoint_dist.w, MIERAYLEIGHRATIO);
		
		lensFlare = v_sourceVisibility * NativeClamp( LensFlareDistanceSqrt(closestpoint_dist.xyz, lightPosition,lightRadius) *(-10/(0.01+v_modelfactor_specular_scattering_lensflare.w)) + 1.0, 0, 1);

	#line 33000
	}else if (drawUniforms->pointbeamcone > 1.5){ // cone
		lightPosition = v_worldPosRad.xyz;
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		lightDirection = normalize(lightToWorld);
		float3 coneDirection = v_worldPosRad2.xyz;
		float lightCosTheta = v_worldPosRad2.w; // The cos of the half angle of the spot cone light
		float lightSinTheta = sqrt(NativeMax(0.0, 1.0 - lightCosTheta * lightCosTheta)); // The sin of the half angle of the spot cone light
		
		float lightandworldangle = dot(lightDirection, coneDirection);
		//falloff = smoothstep(lightCosTheta, 1.0, lightandworldangle) ; // this is softer, but attenuates too much
		falloff = NativeClamp(1.0 - (1.0-lightandworldangle)/ (1.0 - lightCosTheta), 0.0,1.0);

		attenuation = NativeClamp( 1.0 - length (lightToWorld) / lightRadius, 0,1) * falloff;
		
		lightEmitPosition = lightPosition;
		closestpoint_dist = closestlightlp_distance(camPos, viewDirection, lightPosition);
		
		float4 rayconedist = ray_to_capsule_distance_squared(camPos, viewDirection, lightPosition + coneDirection * lightRadius,lightPosition); // this now contains the point on ConeDirection 
		
		float localradius = NativeMax(0, lightSinTheta * length(rayconedist.xyz - lightPosition.xyz));
		float4 iCone = halfconeIntersectScatter(camPos , -viewDirection, lightPosition, coneDirection , lightRadius,  lightRadius * lightSinTheta, fragDistance);
		
		scatteringRayleigh = iCone.z * 0.33;
		scatteringMie = iCone.w;

		lensFlare = v_sourceVisibility * NativeClamp( LensFlareDistanceSqrt(closestpoint_dist.xyz, lightPosition,lightRadius) *(-15 / (0.01 +v_modelfactor_specular_scattering_lensflare.w)) +1, 0, 1);
		
		float lightandcameraangle = dot(coneDirection, viewDirection) + 1.0;
		//lensFlare *= smoothstep(lightCosTheta, 1.0, lightandcameraangle);
		lensFlare *= lightandcameraangle;
		//lensFlare = fract(localradius *0.02);
		
	#line 34000
	}else if (drawUniforms->pointbeamcone < 1.5){ // beam 
		float3 beamhalflength = v_worldPosRad2.xyz - v_worldPosRad.xyz; 
		float3 beamstart = v_worldPosRad.xyz - beamhalflength;
		float3 beamend = v_worldPosRad.xyz + beamhalflength;
		lightPosition = closestbeam(fragWorldPos.xyz, beamstart, beamend);
		
		lightToWorld = fragWorldPos.xyz - lightPosition;
		
		lightDirection = normalize(lightToWorld);
		attenuation = NativeClamp( 1.0 - length (lightToWorld) *lightRadiusInv, 0,1);
		
		
		closestpoint_dist = ray_to_capsule_distance_squared(camPos, viewDirection, beamstart, beamend);
		lightEmitPosition = closestpoint_dist.xyz;
		
		
		lensFlare = 0.0;

		// where the view ray enters and leaves the capsule
		float closedist =  capIntersect( camPos, -viewDirection, beamstart, beamend, lightRadius);
		float fardist = - capIntersect( camPos, viewDirection, beamstart, beamend, lightRadius);
		
		
		float3 EntryPoint = (-viewDirection) * closedist + camPos;
		
		float3 stepVec = - viewDirection * ( fardist  -  closedist) / RAYMARCHSTEPS;
		
		float rayleighScatterSum = 0;
		float miescattersum = 0;

		for (int i = 1; i < RAYMARCHSTEPS; i++){
			// step the ray forward 
			float3 marchPos = stepVec * i + EntryPoint;

			// Sample 3D noise if needed (noise should be 0.5 centered)
			#if (USE3DNOISE == 1 && RAYMARCHSTEPS >= 4)
				float noise = Sample_noise3DCube( fract(marchPos * 0.005 + i / RAYMARCHSTEPS + v_noiseoffset.xyz), 0.0).r * 2.0 ;
			#else
				float noise = 1.0;
			#endif
			
			float relativeclosenesstobeam = NativeClamp(distToCapsuleSqr(marchPos, beamstart, beamend, lightRadius) * lightRadiusInv * lightRadiusInv, 0.0, 1.0) ;
			
			rayleighScatterSum +=  (1.0 - relativeclosenesstobeam) * noise;
			
			miescattersum += (NativeMax(0, 0.1 - relativeclosenesstobeam) * noise  * 10.0);
		}
		
		// Simplest occlusion calculation just integrates scatter in between fragment distance and eye distance
		// cone close distance is negative if rayorigin is inside it
		float tofrag = integratescatterocclusion((fragDistance - closedist ) / (fardist - closedist));
		
		float fromeye = integratescatterocclusion( ( -closedist ) / (fardist - closedist));
		
		rayleighScatterSum = (tofrag - fromeye) * rayleighScatterSum / (RAYMARCHSTEPS - 1);
		miescattersum = (tofrag - fromeye) * miescattersum / (RAYMARCHSTEPS - 1);

		scatteringMie = (miescattersum *miescattersum);
		scatteringRayleigh = rayleighScatterSum;

	}
	#line 35000
	
	diffuse = NativeClamp(dot(-lightDirection, normals.xyz), 0.0, 1.0);
	
	
	// Blinn-Phong specular: half-vector instead of reflect(). Produces rounder
	// highlights that elongate naturally at grazing angles (more wet-metal/PBR look)
	// instead of Phong's teardrop-then-pop. Exponent is ~4x Phong to match the
	// equivalent highlight tightness.
	float3 halfVector = normalize(-lightDirection + viewDirection);
	specular = NativeMax(0.0, dot(halfVector, normals.xyz));
	specular = v_modelfactor_specular_scattering_lensflare.y * pow(specular, 32.0 * ( 1.0 + ismodel * v_modelfactor_specular_scattering_lensflare.x) ) * (1.0 + ismodel * v_modelfactor_specular_scattering_lensflare.x);
	// Core-weighted falloff:
	//  - attCore: quick drop right outside the center (reduces "blob" look)
	//  - attTailEase: smooth/eased approach to zero near the max radius
	// Blend keeps the soft radius edge while making the center visibly hotter.
	float attLinear = attenuation;
	float attCore = pow(attLinear, 2.5);
	float attTailEase = attLinear * attLinear * (3.0 - 2.0 * attLinear);
	attenuation = mix(attCore, attTailEase, 0.17);
	attenuation = NativeClamp(attenuation * (1.0 + 0.22 * attLinear * attLinear), 0.0, 1.0);

	//Give each light a unique blue noise sampling offset 
	float2 blueNoiseUV = (gl_FragCoord.xy + float(v_noiseoffset.a)*7.0)/64.0;
	float4 blueNoiseSample = Sample_blueNoise( blueNoiseUV, 0.0);

	// Do screen-space sampling for shadowing because you are a silly boy:
	float unoccluded = 1.0;
	#ifdef SCREENSPACESHADOWS
		// But only for point and cone lights:
		// also assume that only models will shadow
		if ((v_otherparams.w) > 0.01 && (drawUniforms->screenSpaceShadows > 0)) {
			const int sampleCount = drawUniforms->screenSpaceShadows;
			// Initialize the quad vectors, so we can do Pixel Quad Message Passing
			quadVector = get_quad_vector(float4(v_screenUV.xy, floor(gl_FragCoord.xy))).zw;
			threadMask = quadGetThreadMask(quadVector);

			// Split up a step evenly between the 4 threads in the quad
			float4 threadOffset = float4(0.125, 0.375, 0.625, 0.875);
			//vec4 threadOffset = vec4(0.5);

			// Generate a small, random offset to jiggle the samples a little bit, this provides a bit of noise to the shadowing
			//float randomOffset = rand(gl_FragCoord.xy * 0.013971639) * (- 0.25);
			float randomOffset = blueNoiseSample.a * (-0.25);
			// Collect occludedness in this variable
			float occludedness = 0.0;
			// Skip the march where attenuation is already 0, as unoccluded only scales blendedlights, which gets multiplied by attenuation anyway. Gated per quad, as the quadGather below needs all four lanes
			if (any(NativeGreaterThan(quadGather(attenuation), float4(0.0)))) {
			
				// Steps in screen space
							
				// Set up the raytracing variables
				float3 rayStartScreen = WorldToScreen(lightEmitPosition.xyz);
				float3 rayEndScreen = WorldToScreen(fragWorldPos.xyz);
				float3 rayStepScreen = (rayEndScreen - rayStartScreen) / (sampleCount);
				float rayStepWorldLength = length(lightEmitPosition.xyz - fragWorldPos.xyz) / sampleCount;
				float threadRandom = dot(threadMask, threadOffset) + randomOffset;
				
				// Get the near and far planes from the viewProjection matrix:
				float nearPlane = matrixBuffer->cameraProj[2][3] / (matrixBuffer->cameraProj[2][2] - 1.0);
				float farPlane = matrixBuffer->cameraProj[2][3] / (matrixBuffer->cameraProj[2][2] + 1.0);
				float nearplane_x_farplane_2 = 2.0 * nearPlane * farPlane;
				float farplane_minus_nearplane = farPlane - nearPlane;
				float farplane_plus_nearplane = farPlane + nearPlane;

				// float LinearDepthGL(float z_s, float nearPlane, float farPlane){
				//    float z_ndc   = z_s * 2.0 - 1.0;                // back-project to NDC
    			// return (2.0 * nearPlane * farPlane) /  (farPlane + nearPlane - z_ndc * (farPlane - nearPlane));}
				// same units as nearPlane & f
				// Use the code below to verify the behavior of the depth buffer, and the linearity of the depth buffer
				/*
					float lightY = lightEmitPosition.y;
					float groundY = fragWorldPos.y;
					//printf(lightY);
					//printf(groundY);
					float dheight = lightY - groundY;
					//printf(dheight);
					//printf(nearPlane);
					//printf(farPlane);

					// For Proper debugging, we need to actually verify the Z distance between the light origin and the depth of the fragment at the cursor. 
					float lightDistanceToCamera = 36.0 * nearplane_x_farplane_2 / (farplane_plus_nearplane - (rayStartScreen.z * 2.0 - 1.0) * farplane_minus_nearplane);

					//printf(lightDistanceToCamera);

					float fragDistance = 36.0 *  nearplane_x_farplane_2 / (farplane_plus_nearplane - (rayEndScreen.z * 2.0 - 1.0) * farplane_minus_nearplane);

					//printf(fragDistance);

					float elmoDistance = (lightDistanceToCamera - fragDistance); // Yup, 36.0 is the magic number that still provides decent linearity along the range. 
					//printf(elmoDistance);

					// Ok, elmodistance seems to be correct, now we need to simplify the above calculation
					float nxp2 = 2.0 * nearPlane * farPlane;
					float fmn  = farPlane - nearPlane;
					float fpn  = farPlane + nearPlane;

					float testDistance = depthToDeltaElmos(rayStartScreen.z, rayEndScreen.z, nearplane_x_farplane_2, farplane_plus_nearplane, farplane_minus_nearplane);
					//printf(testDistance);	
				*/

				for (int i = 0; i < sampleCount; i++){ 
					float stepSize =  (float(i) + threadRandom );

					// Calculate the current ray position in screenspace only!

					float3 rayScreenPos = rayStartScreen + rayStepScreen * stepSize; 
					// Convert the NDC to UV space, and clamping is not needed because enabling it brings bad artifacts
					rayScreenPos.xy = rayScreenPos.xy * 0.5 + 0.5;
					
					float rayScreenDepthSample = Sample_modelDepths( rayScreenPos.xy, 0.0).x;

					// Assume that any sample outside of the edges of the screen will not occlude
					if (any(NativeLessThan(rayScreenPos.xy, float2(0.001))) || any(NativeGreaterThan(rayScreenPos.xy, float2(1.0-0.001)))) rayScreenDepthSample = 1.0;
					
					// Since modeldepth is zero where there is no model, we need to convert this to 1.0
					if (rayScreenDepthSample < 0.01) rayScreenDepthSample = 1.0;

					// we need to soften this, based on the squared world-space distance between the ray point and the sampled depth:
					
					float sampleDistance = depthToDeltaElmos(rayScreenPos.z, rayScreenDepthSample, nearplane_x_farplane_2, farplane_plus_nearplane, farplane_minus_nearplane);

					// Occlude down to 48 elmos in depth. 
					#if 1
						//if (sampleDistance > 0.0 && sampleDistance < 48.0) {
						//	occludedness += 1.0; // This is a fully occluded sample
						//}
						// We need a function centerered around 24, where its 1, and then goes down with rayStepWorldLength.  
						#define OCCLUSIONWIDTH 32.0
						#define OCCLUSIONSLOPE 0.3
						occludedness += NativeClamp( OCCLUSIONSLOPE *( OCCLUSIONWIDTH - abs(sampleDistance  + 1.0/(OCCLUSIONSLOPE) -  OCCLUSIONWIDTH)), 0.0, 1.0  );


					#else
						float sampleOcclusionStrength = 0.0;
											// Assume that a ray that hits exactly occludes 0.5
						if (sampleDistance > 0 ) {
							// If the sample actually occludes, then softly occlude it based on the distance from the light
							if (sampleDistance > 48.0) // Very deep occluders dont occlude
								sampleOcclusionStrength = 1.0 -  NativeClamp((sampleDistance - 48.0) / (rayStepWorldLength * 0.05), 0.0, 1.0);
							else
								sampleOcclusionStrength = 0.5 + NativeClamp(sampleDistance / (rayStepWorldLength * 0.05), 0.0, 1.0) * 0.5;

						}else{
							// if the sample does not occlude, but is close to doing so, then softly occlude it based on the distance from the light	
							sampleOcclusionStrength = 0.5 - NativeClamp( -1 * sampleDistance / (rayStepWorldLength * 0.2), 0.0, 1.0) * 0.5;
						}
				
						occludedness += sampleOcclusionStrength * 1.0;

					#endif
					
				}
			}

			float prob = 0.56;

			//vec4 weights = vec4(prob*prob, prob*(1.0-prob), (1.0-prob)*prob, (1.0-prob)*(1.0-prob));
			float4 weights = float4(prob, 1.0 - prob, 1.0 - prob, prob) * 0.5;
			float4 gatheredunoccluded = quadGather(occludedness);
			float dotproduct = dot(gatheredunoccluded, float4(4* weights)); // This will multiply by 4, effectively
			//dotproduct = occludedness;
			unoccluded = 1.0 - smoothstep(0.0, 0.25 * v_otherparams.w * float(drawUniforms->screenSpaceShadows) , dotproduct);
			//unoccluded = 1.0;
			//printf(unoccluded);
		}	

		
	#endif
	fragColor.rgb = float3(
			(diffuse) * attenuation + lensFlare,
			//relativedistancetolight * relativedistancetolight * selfglowfalloff * sourceVisible + 
			scatteringMie + 
			(specular) * attenuation,
			 scatteringRayleigh
			);
			
	
	// light mixdown:
	targetcolor.rgb = NativeMax(float3(0.2), targetcolor.rgb); // we shouldn't let the targetcolor be fully black, or else we will have a bad time blending onto it.
	
	float mintarg = 0.4;
	float targetbrightness =dot(targetcolor.rgb, float3(0.375,0.5,0.125));
	
	targetcolor *= (1.0 -  NativeStep(mintarg,targetbrightness) * (targetbrightness -mintarg));
	// if brightness is > 0.5, start reducing it?
	
	
	// Sum up the additives of Rayleigh, Mie and LensFlare, colorize and alpha control them
	float3 additivelights = ((scatteringRayleigh + scatteringMie) * v_modelfactor_specular_scattering_lensflare.z + lensFlare) * v_lightcolor.rgb * v_lightcolor.w * 0.4  ;

	// Sum up diffuse+specular and colorize the light
	float3 blendedlights = (v_lightcolor.rgb * v_lightcolor.w) * (diffuse + specular) * unoccluded;

	// Modulate color with target color of the surface. 
	blendedlights = mix(blendedlights, blendedlights * targetcolor.rgb * 2.0, SURFACECOLORMODULATION);
	#if (VOIDWATER == 1) 
		if (fragWorldPos.y < 0) 
			blendedlights.rgb = float3(0.0);
	#endif
	// Calculate attenuation and blend more lights onto models
	blendedlights *= attenuation * 2.0 * (1.0 + 2.0 * ismodel * v_modelfactor_specular_scattering_lensflare.x);
	
	// Estimate how 'lit' our target fragment is going to be, once we additively blend all light factors and the target surface color
	float3 outlight_unclamped = targetcolor.rgb + blendedlights + additivelights;
	
	// The amount of light that will 'bleed', or overflow from that pixel is the sum of each channel that is over 1.0 according to our estimate
	float bleed = dot(NativeMax(float3(0.0), outlight_unclamped - float3(1.0)), float3(1.0));
	
	// Square the bleed because why not
	bleed = bleed * bleed;
	
	// bleeding makes the other channels brighter when we 'overflow' with lighting
	fragColor.rgb = (blendedlights*0.9  + additivelights*0.5) + float3(bleed)* BLEEDFACTOR; 
	
	fragColor.rgb *= drawUniforms->intensityMultiplier;

	// Add a half a bit of blue noise to the color to prevent banding, especially with contrast adaptive sharpening:
	// Which should kinda be color dependent anyway, so lets find the max of all the color, and use that to ensure we get half a bit on the output:
	//v_lightcolor.rgb
	fragColor.rgb -=  (blueNoiseSample.rgb - 0.5) * (1.0 / 384.0);
	//fragColor.rgb *= v_lightcolor.a;
	//fragColor.rgb = vec3(bleed);
	//fragColor.rgb = vec3(targetcolor.rgb + blendedlights + additivelights);
	//fragColor.rgb = outlight_unclamped;
	//fragColor.rgb = vec3(scatteringRayleigh);
	//fragColor.rgb = vec3(dot(fragColor.rgb, vec3(1.0, 0.5, 0.5)));
	//fragColor.rgb += 0.5;
	//fragColor.rgb += clamp(0.25* vec3(pow(1.0-rcdsqr / (v_worldPosRad.w * v_worldPosRad.w),16)), 0.0, 1.0);
	//fragColor.rgb = vec3(rcdsqr / (v_worldPosRad.w * v_worldPosRad.w));
	//fragColor.rgb = (fract(lightEmitPosition*0.02));
	fragColor.a = 1.0;
	//fragColor.rgb = vec3(targetcolor);
	//fragColor.rgb = vec3(attenuation);
	//fragColor.rgb = fract(v_lightcenter_gradient_height.www*0.1);
	
}

};
fragment float4 DeferredLightFragment(DeferredLightVarying input [[stage_in]],constant NativeDeferredLightMatrices& matrices [[buffer(2)]],constant NativeDeferredLightParams& params [[buffer(3)]],constant NativeDeferredLightDrawUniforms& uniforms [[buffer(6)]],NativeMapDepthTexture mapDepths [[texture(0)]],sampler mapDepthsSampler [[sampler(0)]],NativeModelDepthTexture modelDepths [[texture(1)]],sampler modelDepthsSampler [[sampler(1)]],texture2d<float> mapNormals [[texture(2)]],sampler mapNormalsSampler [[sampler(2)]],texture2d<float> modelNormals [[texture(3)]],sampler modelNormalsSampler [[sampler(3)]],texture2d<float> mapDiffuse [[texture(6)]],sampler mapDiffuseSampler [[sampler(6)]],texture2d<float> modelDiffuse [[texture(7)]],sampler modelDiffuseSampler [[sampler(7)]]
#if NATIVE_LIGHT_VOLUME
,texture3d<float> noise3DCube [[texture(8)]],sampler noise3DCubeSampler [[sampler(8)]]
#endif
,texture2d<float> blueNoise [[texture(9)]],sampler blueNoiseSampler [[sampler(9)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.rasterPosition,get_sample_position(recoilOriginalSampleID));
DeferredLightFragmentProgram program;
program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;
program.mapDepths=mapDepths;program.mapDepthsSampler=mapDepthsSampler;
program.modelDepths=modelDepths;program.modelDepthsSampler=modelDepthsSampler;
program.mapNormals=mapNormals;program.mapNormalsSampler=mapNormalsSampler;
program.modelNormals=modelNormals;program.modelNormalsSampler=modelNormalsSampler;
program.mapDiffuse=mapDiffuse;program.mapDiffuseSampler=mapDiffuseSampler;
program.modelDiffuse=modelDiffuse;program.modelDiffuseSampler=modelDiffuseSampler;
#if NATIVE_LIGHT_VOLUME
program.noise3DCube=noise3DCube;program.noise3DCubeSampler=noise3DCubeSampler;
#endif
program.blueNoise=blueNoise;program.blueNoiseSampler=blueNoiseSampler;
program.v_worldPosRad=input.v_worldPosRad;
program.v_worldPosRad2=input.v_worldPosRad2;
program.v_lightcolor=input.v_lightcolor;
program.v_modelfactor_specular_scattering_lensflare=input.v_modelfactor_specular_scattering_lensflare;
program.v_otherparams=input.v_otherparams;
program.v_position=input.v_position;
program.v_noiseoffset=input.v_noiseoffset;
program.v_screenUV=input.v_screenUV;
program.v_sourceVisibility=input.v_sourceVisibility;
program.gl_FragCoord=float4(float2(input.rasterPosition.x,uniforms.framebuffer.z-input.rasterPosition.y)*uniforms.framebuffer.xy,input.rasterPosition.zw);
program.Run();
if(!NativeDeferredLightAlpha(program.fragColor.a,uniforms))discard_fragment();
return program.fragColor;
}


)DLMETAL";
}
