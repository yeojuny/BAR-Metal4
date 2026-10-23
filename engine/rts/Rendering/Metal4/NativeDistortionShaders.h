/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Original31657 Distortion shader equations, including every selected effect type.
// Original GLSL is MIT, copyright2026 Beherith; only MSL syntax/ABI/image transport adapted.
namespace rendering::metal4::distortion {
inline constexpr char SHADER[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"DISTMSL(/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
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

struct NativeDistortionDrawUniforms {
	float pointbeamcone, attachedtounitID, windX, windZ;
	float radiusMultiplier, intensityMultiplier, overallStrength, pad;
	uint4 transport;
	float4 framebuffer;
	float4 alpha;
	uint4 imageSwizzle[10];
	uint4 depthMode[3];
	uint4 imageSize[10];
 float4 inverseScreenResolution;
};
struct NativeDistortionInstance {
	packed_float4 worldposrad, worldposrad2, baseparams;
	packed_float4 universalParams, lifeParams, effectParams;
	uint pieceIndex;
	packed_uint4 instData;
};
struct NativeDistortionMatrices {
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
struct NativeDistortionParams {
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
static_assert(sizeof(NativeDistortionDrawUniforms)==464,"Deferred-light draw ABI");
static_assert(sizeof(NativeDistortionInstance)==116,"Original light instance stride");
static_assert(sizeof(NativeDistortionMatrices)==1664,"Original engine matrix declaration");
static_assert(sizeof(NativeDistortionParams)==4368,"Original LuaShader parameter declaration");
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

inline float2 NativeImageUV(float2 uv,uint slot,constant NativeDistortionDrawUniforms& u) {
	if(u.transport.x&(1u<<slot)) uv.y=1.0-uv.y;
	return uv;
}
inline float3 NativeImageUV(float3 uv,uint slot,constant NativeDistortionDrawUniforms& u) {
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
inline float4 NativeLogicalRGBA(float4 value,uint,constant NativeDistortionDrawUniforms&) { return value; }
inline float4 NativeLogicalRGBA(float depth,uint slot,constant NativeDistortionDrawUniforms& u) {
	uint mode=u.depthMode[slot/4][slot%4];
	return mode==0x1906u ? float4(1,1,1,depth) : mode==0x8049u ? float4(depth) : float4(depth,depth,depth,1);
}
inline float4 NativeSourceSwizzle(float4 v,uint slot,constant NativeDistortionDrawUniforms& u) {
	uint4 swizzle=u.imageSwizzle[slot];
	return float4(NativeSourceChannel(v,swizzle.x),NativeSourceChannel(v,swizzle.y),
		NativeSourceChannel(v,swizzle.z),NativeSourceChannel(v,swizzle.w));
}
template<typename T> inline float4 NativeSampleLOD(T image,sampler state,float2 uv,float lod,uint slot,constant NativeDistortionDrawUniforms& u) {
	return NativeSourceSwizzle(NativeLogicalRGBA(image.sample(state,NativeImageUV(uv,slot,u),level(lod)),slot,u),slot,u);
}
inline float4 NativeSampleLOD(texture3d<float> image,sampler state,float3 uv,float lod,uint slot,constant NativeDistortionDrawUniforms& u) {
	return NativeSourceSwizzle(image.sample(state,NativeImageUV(uv,slot,u),level(lod)),slot,u);
}
inline float4 NativeGatherChannel(texture2d<float> image,sampler state,float2 uv,uint channel,uint,constant NativeDistortionDrawUniforms&) {
	switch(channel) {
		case 0: return float4(0); case 1: return float4(1);
		case 0x1903: return image.gather(state,uv,int2(0),component::x);
		case 0x1904: return image.gather(state,uv,int2(0),component::y);
		case 0x1905: return image.gather(state,uv,int2(0),component::z);
		case 0x1906: return image.gather(state,uv,int2(0),component::w);
		default: return float4(0);
	}
}
inline float4 NativeGatherChannel(depth2d<float> image,sampler state,float2 uv,uint channel,uint slot,constant NativeDistortionDrawUniforms& u) {
	if(channel==0) return float4(0);
	if(channel==1) return float4(1);
	uint mode=u.depthMode[slot/4][slot%4];
	bool sampleDepth=mode==0x8049u || (mode==0x1906u ? channel==0x1906u : channel!=0x1906u);
	return sampleDepth ? image.gather(state,uv) : float4(1);
}
template<typename T> inline float4 NativeGather(T image,sampler state,float2 uv,int componentIndex,uint slot,constant NativeDistortionDrawUniforms& u) {
	float4 gathered=NativeGatherChannel(image,state,NativeImageUV(uv,slot,u),
		u.imageSwizzle[slot][componentIndex],slot,u);
	// A Y reflection reverses the footprint as well as its sample coordinates.
	// Restore GLSL's (0,1),(1,1),(1,0),(0,0) order in logical image space.
	return (u.transport.x&(1u<<slot)) ? gathered.wzyx : gathered;
}
inline int2 NativeImageSize(uint slot,int lod,constant NativeDistortionDrawUniforms& u) {
	return int2(max(uint2(1),u.imageSize[slot].xy>>uint(lod)));
}
inline bool NativeDistortionAlpha(float value,constant NativeDistortionDrawUniforms& u) {
	if(u.alpha.x==0) return true;
	float reference=u.alpha.y;
	switch(uint(u.alpha.z)) {
		case 0: return false; case 1: return value<reference;
		case 2: return value==reference; case 3: return value<=reference;
		case 4: return value>reference; case 5: return value!=reference;
		case 6: return value>=reference; default: return true;
	}
}


struct DistortionVarying {
 float4 rasterPosition [[position]];
 float4 v_worldPosRad [[user(locn0),flat]];
 float4 v_worldPosRad2 [[user(locn1),flat]];
 float4 v_baseparams [[user(locn2),flat]];
 float4 v_universalParams [[user(locn3),flat]];
 float4 v_lifeParams [[user(locn4),flat]];
 float4 v_effectParams [[user(locn5),flat]];
#ifdef UNIFORMSBUFFERCOPY
 float4 v_unibuffercopy [[user(locn7),flat]];
#endif
 float2 v_screenUV [[user(locn6),RECOIL_SAMPLE_NOPERSPECTIVE]];
};
struct DistortionVertexProgram {
 constant NativeDistortionMatrices* matrixBuffer;
 constant NativeDistortionParams* paramsBuffer;
 constant NativeDistortionDrawUniforms* drawUniforms;
 float4 v_worldPosRad;
 float4 v_worldPosRad2;
 float4 v_baseparams;
 float4 v_universalParams;
 float4 v_lifeParams;
 float4 v_effectParams;
#ifdef UNIFORMSBUFFERCOPY
 float4 v_unibuffercopy;
#endif
 float4 worldposrad;
 float4 worldposrad2;
 float4 baseparams;
 float4 universalParams;
 float4 lifeParams;
 float4 effectParams;
 float4 position,gl_Position;uint pieceIndex;uint4 instData;float2 v_screenUV;
 #if NATIVE_MODEL_BUFFERS & 2
 device const SUniformsBuffer* uni;
#ifdef UNIFORMSBUFFERCOPY
 device const SUniformsBuffer* uniCopy;
#endif
 #endif
#if NATIVE_MODEL_BUFFERS & 1
#if USEQUATERNIONS == 0
 device const float4x4* mat;
#else
 device const Transform* transforms;
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
#endif // NATIVE_MODEL_BUFFERS & 1
#define YOFFSET baseparams.x
#define EFFECTSTRENGTH baseparams.y
#define STARTRADIUS baseparams.z
#define SPAWNFRAME lifeParams.x
#define LIFETIME   lifeParams.y
#define RAMPUP     lifeParams.z
#define DECAY      lifeParams.w
#define UNITID (uni[instData.y].composite >> 16)
#define SNORM2NORM(value) (value * 0.5 + 0.5)
void Run()
{
	float time = paramsBuffer->timeInfo.x + paramsBuffer->timeInfo.w;
	int effectType = int(round(effectParams.w));
	float elapsedframes = time - SPAWNFRAME;
	float lifeFraction = 1.0;
	if (LIFETIME > 1) lifeFraction = NativeClamp(elapsedframes / LIFETIME, 0.0, 1.0);

	float distortionRadius = worldposrad.w;

	// Modulate distortion radius over the lifetime of the distortion
	distortionRadius = STARTRADIUS + (distortionRadius - STARTRADIUS) * lifeFraction;

	// Output to the fragment shader
	v_worldPosRad = worldposrad ;
	v_worldPosRad.w = distortionRadius;
	v_worldPosRad2 = worldposrad2;
	v_lifeParams = lifeParams;

	float4 vertexPosition = float4(1.0);

	float4x4 placeInWorldMatrix = NativeMat4(1.0); // this is unity for non-unitID tied stuff
	#if USEQUATERNIONS == 1
		Transform tx;
	#endif

	// Ok so here comes the fun part, where we if we have a unitID then fun things happen
	// v_worldPosRad contains the incoming piece-level offset
	// v_worldPosRad should be after changing to unit-space
	// we have to transform BOTH the center of the distortion to piece-space
	// and the vertices of the distortion volume to piece-space
	// we need to go from distortion-space to world-space
	float3 distortionCenterPosition =  v_worldPosRad.xyz;
	v_baseparams = baseparams;
	#if NATIVE_MODEL_BUFFERS == 3
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


		#ifdef UNIFORMSBUFFERCOPY
			v_unibuffercopy = uni[instData.y].speed - uniCopy[instData.y].speed;
		#endif

	}

	#endif // Attached draws require both original publications.
	float4 worldPos = float4(1.0);

	if (drawUniforms->pointbeamcone < 0.5){ // point
		// Scale it and place it into the world
		// Make it a tiny bit bigger *(1.1) cause the blocky sphere is smaller than the actual radius
		// The -1 is for inverting it so we always see the back faces (great for occlusion testing!) (this should be exploited later on!

		// this is centered around the target positional offset, and scaled locally
		float3 distortionVertexPosition = distortionCenterPosition + -1 * position.xyz * distortionRadius * 1.15; // 1.15 is a magic number that makes the sphere actually fit inside the sphere-ish geometry

		// transform the vertices to world-space
		distortionVertexPosition = (placeInWorldMatrix * float4(distortionVertexPosition, 1.0)).xyz;

		// transform the center to world-space
		distortionCenterPosition = (placeInWorldMatrix * float4(distortionCenterPosition, 1.0)).xyz;


		// Projectile-attached distortions need their own positional smoothing based on the velocity of the projectile and timeOffset (paramsBuffer->timeInfo.w)
		if  (drawUniforms->attachedtounitID > 0.5) {
			// Distortions attached to unitID's need no positional correction
		}else{
			// Distortions attached to projectiles need to be smoothed out
			if (worldposrad2.w < 1.0) { // We indicate that this is a projectile by setting the w component of worldposrad2 to < 1
				distortionCenterPosition += paramsBuffer->timeInfo.w * worldposrad2.xyz;
				distortionVertexPosition += paramsBuffer->timeInfo.w * worldposrad2.xyz;
			}else{

			}
		}

		v_worldPosRad.xyz = distortionCenterPosition;
		vertexPosition = float4( distortionVertexPosition, 1.0);
	}

	else if (drawUniforms->pointbeamcone < 1.5){ // beam
		// we will transform along this vector, where Y shall be the upvector
		// our null vector is +X
		float3 centertoend = distortionCenterPosition - worldposrad2.xyz;
		float halfbeamlength = length(centertoend);

		// Scale the box to correct size (along beam is Y dir)
		worldPos.xyz = position.xyz * float3( distortionRadius , NativeStep(position.y, 0) * halfbeamlength + distortionRadius, distortionRadius );

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
		// or where the distortioncenterpos tells us to

		// Place the box in the world
		worldPos.xyz += distortionCenterPosition;

		// Copy the parameters to the fragment shader varyings and place the vertex
		v_worldPosRad2.xyz = (placeInWorldMatrix * float4(v_worldPosRad2.xyz, 1.0)).xyz;;
		v_worldPosRad.xyz = (placeInWorldMatrix * float4(distortionCenterPosition.xyz, 1.0)).xyz;
		v_worldPosRad.xyz += (v_worldPosRad2.xyz - v_worldPosRad.xyz) * 0.5;
		vertexPosition.xyz = (placeInWorldMatrix * float4(worldPos.xyz, 1.0)).xyz;

	}

	else if (drawUniforms->pointbeamcone > 1.5){ // cone
		// Input cone that has pointy end up, (y = 1), with radius =1, flat on Y=0 plane
		// Make it so that cone tip is at 0 and the opening points to -y
		worldPos.xyz = position.xyz;
		worldPos.x *= -1.0; // flip the cone inside out
		worldPos.y = (worldPos.y*1.1 - 1.) * -1;

		worldPos.xz *= tan(worldposrad2.w); // Scale the flat of the cone by the half-angle of its opening
		v_worldPosRad2.w = cos(worldposrad2.w); // pass through the cosine to avoid this calc later on
		v_worldPosRad2.xyz = normalize(worldposrad2.xyz); // normalize this here for sanity

		// if the cone is not attached to the unit, exploit that direction allows us to smoothen anim
		if (drawUniforms->attachedtounitID < 0.5){
			distortionCenterPosition += worldposrad2.xyz * paramsBuffer->timeInfo.w;
			// if its projectile slaved, then flip its direction
			v_worldPosRad2.xyz *= -1.0;
		}

		worldPos.xyz *= distortionRadius * 1.05; // scale it all by the height of the cone, and a bit of extra

		// Now our cone is opening forward towards  -y, but we want it to point into the worldposrad2.xyz
		float3 oldfw = float3(0.001, -1, 0.001); // The old forward direction is -y, plus a tiny bit to avoid singularity in vector cross products
		float3 newfw = normalize(v_worldPosRad2.xyz); // the new forward direction shall be the normal that we want
		float3 newright = normalize(cross(newfw, oldfw)); // the new right direction shall be the vector perpendicular to old and new forward
		float3 newup = normalize(cross(newright, newfw)); // the new up direction shall be the vector perpendicular to new right and new forward

		float3x3 rotmat = NativeMat3( // assemble the rotation matrix
				newup,
				newfw,
				newright
			);

		float3 rotOffset = rotmat * float3(0,YOFFSET,0);
		distortionCenterPosition += rotOffset;
		// rotate the cone, and place it into local space
		worldPos.xyz = rotmat * worldPos.xyz + distortionCenterPosition;

		// move the cone into piece or world space:
		worldPos.xyz = (placeInWorldMatrix * float4(worldPos.xyz, 1.0)).xyz;

		// set the center pos of the distortion:
		v_worldPosRad.xyz = (placeInWorldMatrix * float4(distortionCenterPosition.xyz, 1.0)).xyz;

		// Clear out the translation from the cone direction, and turn the cone according to the piece matrix
		v_worldPosRad2.xyz = NativeMat3(placeInWorldMatrix) * v_worldPosRad2.xyz;

		vertexPosition =  worldPos;
	}


	//-------------------------- BEGIN SHARED SECTION ---------------------
	// This section is shared between all distortion shapes

	#if NATIVE_MODEL_BUFFERS & 2
	if (effectType == 11){
		v_worldPosRad2 = uni[instData.y].speed;
	}

	#endif
	// Initialize the distortion strength multiplier
	v_baseparams.x = 1.0;
	// if the distortion is attached to a unit, and the lifeTime is 0, and the decay is nonzero, then modulate the strength with the units selfillummod
	#if NATIVE_MODEL_BUFFERS & 2
	if ((drawUniforms->attachedtounitID > 0.5) && (LIFETIME == 0) && (DECAY < 0)){
		float selfIllumMod = NativeMax(-0.2, sin(time * 2.0/30.0 + float(UNITID) * 0.1 - 0.5)) + 0.2;
		selfIllumMod *= selfIllumMod * (2.0 - selfIllumMod); //Almost Unit Identity
		// Almost
		selfIllumMod = mix(1.0, selfIllumMod, -1.0 / DECAY);
		v_baseparams.x *= selfIllumMod;
	}

	#endif
	// If a lifeParams.z rampup is specified, then
	// >1 : how many frames to linearly ramp up to full power
	// 0< z <1: use a power curve with exponent Z to ramp up
	// Note that rampup can also be used for infinite lifetime distortions to ramp up the distortion strength
	if (RAMPUP > 0.0){
		v_baseparams.x *= NativeClamp(elapsedframes / RAMPUP, 0.0, 1.0);
	}

	if (LIFETIME > 1){ // Decay only makes sense if lifetime is > 1
		// If a lifeParams.w decay is specified, then
		// >1 : how many frames to linearly decay to zero
		// 0< z <1: use a power curve with exponent Z to decay
		if (DECAY > 0.0){
			if (DECAY > 1) {
				// How much life it still has left:
				float decayfraction = (LIFETIME - elapsedframes) / DECAY;
				v_baseparams.x *= NativeClamp(decayfraction, 0.0, 1.0);
			}
			else {
				float lifeFraction = (LIFETIME - elapsedframes) / LIFETIME;
				v_baseparams.x *= NativeClamp(pow(lifeFraction, 10.0 * DECAY),0.0, 1.0);
			}
		}
	}

	// pass everything else on to fragment shader varyings
	v_universalParams = universalParams;
	v_effectParams = effectParams;
	gl_Position = matrixBuffer->cameraViewProj * vertexPosition;
	v_screenUV = SNORM2NORM(gl_Position.xy / gl_Position.w);
}
};
#undef YOFFSET
#undef EFFECTSTRENGTH
#undef STARTRADIUS
#undef SPAWNFRAME
#undef LIFETIME
#undef RAMPUP
#undef DECAY
#undef UNITID
#undef SNORM2NORM
vertex DistortionVarying DistortionVertex(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const NativeDistortionInstance* instances [[buffer(1)]],constant NativeDistortionMatrices& matrices [[buffer(2)]],constant NativeDistortionParams& params [[buffer(3)]],constant NativeDistortionDrawUniforms& uniforms [[buffer(6)]]
#if NATIVE_MODEL_BUFFERS & 2
 ,device const SUniformsBuffer* modelUniforms [[buffer(5)]]
#ifdef UNIFORMSBUFFERCOPY
 ,device const SUniformsBuffer* copiedUniforms [[buffer(7)]]
#endif
#endif
#if NATIVE_MODEL_BUFFERS & 1
#if USEQUATERNIONS == 0
 ,device const float4x4* modelTransforms [[buffer(4)]]
#else
 ,device const Transform* modelTransforms [[buffer(4)]]
#endif
#endif
){
 DistortionVertexProgram program;program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;program.position=mesh[vertexIndex];
 device const NativeDistortionInstance& row=instances[instance];
 program.worldposrad=float4(row.worldposrad);
 program.worldposrad2=float4(row.worldposrad2);
 program.baseparams=float4(row.baseparams);
 program.universalParams=float4(row.universalParams);
 program.lifeParams=float4(row.lifeParams);
 program.effectParams=float4(row.effectParams);
 program.pieceIndex=row.pieceIndex;program.instData=uint4(row.instData);
#if NATIVE_MODEL_BUFFERS & 2
 program.uni=modelUniforms;
#ifdef UNIFORMSBUFFERCOPY
 program.uniCopy=copiedUniforms;
#endif
#endif
#if NATIVE_MODEL_BUFFERS & 1
#if USEQUATERNIONS == 0
 program.mat=modelTransforms;
#else
 program.transforms=modelTransforms;
#endif
#endif
 program.Run();DistortionVarying result;result.rasterPosition=program.gl_Position;
 if(uniforms.transport.y&1u)result.rasterPosition.z=(result.rasterPosition.z+result.rasterPosition.w)*0.5f;
 if(uniforms.transport.y&2u)result.rasterPosition.y=-result.rasterPosition.y;
 result.v_worldPosRad=program.v_worldPosRad;
 result.v_worldPosRad2=program.v_worldPosRad2;
 result.v_baseparams=program.v_baseparams;
 result.v_universalParams=program.v_universalParams;
 result.v_lifeParams=program.v_lifeParams;
 result.v_effectParams=program.v_effectParams;
#ifdef UNIFORMSBUFFERCOPY
 result.v_unibuffercopy=program.v_unibuffercopy;
#endif
 result.v_screenUV=program.v_screenUV;return result;
}
struct DistortionFragmentProgram {
 constant NativeDistortionMatrices* matrixBuffer;
 constant NativeDistortionParams* paramsBuffer;
 constant NativeDistortionDrawUniforms* drawUniforms;
 float4 v_worldPosRad;
 float4 v_worldPosRad2;
 float4 v_baseparams;
 float4 v_universalParams;
 float4 v_lifeParams;
 float4 v_effectParams;
#ifdef UNIFORMSBUFFERCOPY
 float4 v_unibuffercopy;
#endif
 float2 v_screenUV;float4 fragColor;
 NativeMapDepthTexture mapDepths;sampler mapDepthsSampler;
 NativeModelDepthTexture modelDepths;sampler modelDepthsSampler;
 texture3d<float> noise3DCube;sampler noise3DCubeSampler;
 float4 Sample_mapDepths(float2 uv,float lod){return NativeSampleLOD(mapDepths,mapDepthsSampler,uv,lod,0,*drawUniforms);}
 float4 Sample_modelDepths(float2 uv,float lod){return NativeSampleLOD(modelDepths,modelDepthsSampler,uv,lod,1,*drawUniforms);}
 float4 Sample_noise3DCube(float3 uv,float lod){return NativeSampleLOD(noise3DCube,noise3DCubeSampler,uv,lod,2,*drawUniforms);}
#define LIFESTRENGTH v_baseparams.x
#define EFFECTSTRENGTH v_baseparams.y
#define STARTRADIUS	v_baseparams.z
#define NOISESTRENGTH v_universalParams.x
#define NOISESCALESPACE v_universalParams.y
#define DISTANCEFALLOFF v_universalParams.z
#define ONLYMODELMAP v_universalParams.w
#define SPAWNFRAME v_lifeParams.x
#define LIFETIME   v_lifeParams.y
#define RAMPUP     v_lifeParams.z
#define DECAY      v_lifeParams.w
#define EFFECTPARAM1 v_effectParams.x
#define EFFECTPARAM2 v_effectParams.y
#define WINDAFFECTED v_effectParams.z
#define EFFECTTYPE   v_effectParams.w
#define DEBUG(x) fragColor.rgba = float4( fract(x*0.999 + 0.0001), sqrt(floor(x) / 256.0),(x < 0? 1 : 0),1.0); return;
#define DEBUGPOS(x) fragColor.rgba = float4(fract(x * 0.1 + 0.0001),1.0); return;
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
float4 closestdistortionlp_distance (float3 ro, float3 rd, float3 P){
	float t0 = dot(rd, P - ro) / dot(rd, rd);
	float3 intersectPoint = ro + t0 * rd;
	return float4(intersectPoint, length(P - intersectPoint));
}

// Given two rays, returns the minimum distance between them
//https://math.stackexchange.com/questions/2213165/find-shortest-distance-between-lines-in-3d

// https://gist.github.com/wwwtyro/beecc31d65d1004f5a9d
float2 raySphereIntersect(float3 rayOrigin, float3 rayDirection, float3 sphereCenter, float sphereRadius) {
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersection with sphere in X, and the distance to the second intersection in Y
    //   or -1.0 if no intersection.
    float a = dot(rayDirection, rayDirection);
    float3 sphereCenter_to_rayOrigin = rayOrigin - sphereCenter;
    float b = 2.0 * dot(rayDirection, sphereCenter_to_rayOrigin);
    float c = dot(sphereCenter_to_rayOrigin, sphereCenter_to_rayOrigin) - (sphereRadius * sphereRadius);
	float disc = b * b - 4.0 * a* c;
    if (disc < 0.0) {
        return float2(-1.0, -1.0);
    }else{
		disc = sqrt(disc);
		return float2(-b - disc, -b + disc) / (2.0 * a);
	}
}

// Given a ray origin and a direction, returns the closes point on the ray in xyz and the distance in w

//http://www.realtimerendering.com/intersections.html


// The neat thing about this approach is that its quite generic in the sense of trivially being identical for spheres and beams
// Given a ray defined by rayOrigin and rayDirection, and a line segment defined with lineStart and lineEnd,
// Which point on the ray is closest to the line segment, and which point on the line segment is closest to the ray?
// Returns the closest point on the ray in XYZ and the distance to the line segment in W
// Also places the closest point on the line segment in the XYZ and the distance in W of out float4 lineClosestPoint
// Write a GLSL function to solve the above problem by continuing the GLSL function definition below:
// THIS IS THE ONLY CORRECT IMLPEMENTATION!!!!!!!
float4 ray_line_segment_closestpoint_on_ray_and_segment2(float3 rayOrigin, float3 rayDirection,
                                  float3 lineStart, float3 lineEnd,
                                  thread float4& closestPointOnSegment) {
    float3 lineStartToEnd = lineEnd - lineStart;       // Direction vector of segment
    float3 originToStart = lineStart - rayOrigin;

    float lineLenSQR = dot(lineStartToEnd, lineStartToEnd);                // Squared length of segment
    float dotRayLine = dot(rayDirection, lineStartToEnd);
    float d = dot(rayDirection, originToStart);
    float e = dot(lineStartToEnd, originToStart);

    float D = lineLenSQR - dotRayLine * dotRayLine;            // Denominator for s and t parameters
    float sN, sD = D;
    float tN, tD = D;

    // Check if lines are almost parallel
    if (abs(D) < 1e-8) {
        sN = 0.0;                       // Force s to zero
        sD = 1.0;                       // Prevent division by zero
        tN = d;
        tD = 1.0;
    } else {
        sN = (dotRayLine * d - e);
        tN = (lineLenSQR * d - dotRayLine * e);
    }

    // Compute the closest point parameter s on the segment [0,1]
    float s = sN / sD;
    s = NativeClamp(s, 0.0, 1.0);
	float t;
    // Recompute t for the closest point on the ray
    if (abs(D) < 1e-8) {
        t = 0.0;
    } else {
        t = (dotRayLine * s + d) ;
    }

    // Ensure t is non-negative (since the ray extends from the origin in the positive direction)
    t = NativeMax(t, 0.0);

    // Compute the closest points on the segment and the ray
    closestPointOnSegment.xyz = lineStart + s * lineStartToEnd;
    float3 closestPointOnRay = rayOrigin + t * rayDirection;

    // Compute the distance between the closest points
    float distance = length(closestPointOnSegment.xyz - closestPointOnRay);
	closestPointOnSegment.w = distance;

    return float4(closestPointOnRay, distance);
}

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


//Write me a glsl function that gives the distances to the intersection points between a ray, given a float3 rayPos and float3 rayDir and cone rounded on both ends given with float3 pointA, float3 pointB, float radiusA, float radiusB
float2 intersectRoundedCone(float3 rayPos, float3 rayDir, float3 pointA, float3 pointB, float radiusA, float radiusB) {
    // Compute the cone axis and its length
    float3 ba = pointB - pointA;
    float h = length(ba);
    float3 ba_norm = ba / h;  // Normalize the cone axis

    // Compute rate of change of radius along the cone axis
    float m0 = (radiusB - radiusA) / h;
    float n0 = radiusA;  // Starting radius at pointA

    // Vector from pointA to ray origin
    float3 oa = rayPos - pointA;

    // Projections onto the cone axis
    float dd = dot(rayDir, ba_norm);
    float oc = dot(oa, ba_norm);

    // Components perpendicular to the cone axis
    float3 w = rayDir - ba_norm * dd;
    float3 u = oa - ba_norm * oc;

    // Quadratic coefficients for intersection with the cone
    float m = m0 * dd;
    float n = m0 * oc + n0;

    float A = dot(w, w) - m * m;
    float B = 2.0 * (dot(w, u) - m * n);
    float C = dot(u, u) - n * n;

    // Solve the quadratic equation A*t^2 + B*t + C = 0
    float discriminant = B * B - 4.0 * A * C;
    float2 tCone = float2(-1.0);
    if (discriminant >= 0.0) {
        float sqrtDisc = sqrt(discriminant);
        float t0 = (-B - sqrtDisc) / (2.0 * A);
        float t1 = (-B + sqrtDisc) / (2.0 * A);

        // Check if intersection points are within the valid height of the cone
        float y0 = oc + t0 * dd;
        if (t0 >= 0.0 && y0 >= 0.0 && y0 <= h) {
            tCone[0] = t0;
        }

        float y1 = oc + t1 * dd;
        if (t1 >= 0.0 && y1 >= 0.0 && y1 <= h) {
            tCone[1] = t1;
        }
    }

    // Intersection with sphere at pointA
    float A_sphereA = dot(rayDir, rayDir);
    float B_sphereA = 2.0 * dot(oa, rayDir);
    float C_sphereA = dot(oa, oa) - radiusA * radiusA;

    discriminant = B_sphereA * B_sphereA - 4.0 * A_sphereA * C_sphereA;
    float2 tSphereA = float2(-1.0);
    if (discriminant >= 0.0) {
        float sqrtDisc = sqrt(discriminant);
        float t0 = (-B_sphereA - sqrtDisc) / (2.0 * A_sphereA);
        float t1 = (-B_sphereA + sqrtDisc) / (2.0 * A_sphereA);
        if (t0 >= 0.0) tSphereA[0] = t0;
        if (t1 >= 0.0) tSphereA[1] = t1;
    }

    // Intersection with sphere at pointB
    float3 ob = rayPos - pointB;
    float A_sphereB = dot(rayDir, rayDir);
    float B_sphereB = 2.0 * dot(ob, rayDir);
    float C_sphereB = dot(ob, ob) - radiusB * radiusB;

    discriminant = B_sphereB * B_sphereB - 4.0 * A_sphereB * C_sphereB;
    float2 tSphereB = float2(-1.0);
    if (discriminant >= 0.0) {
        float sqrtDisc = sqrt(discriminant);
        float t0 = (-B_sphereB - sqrtDisc) / (2.0 * A_sphereB);
        float t1 = (-B_sphereB + sqrtDisc) / (2.0 * A_sphereB);
        if (t0 >= 0.0) tSphereB[0] = t0;
        if (t1 >= 0.0) tSphereB[1] = t1;
    }

    // Collect all valid intersection distances
    float tMin = 1e20;
    float tMax = -1.0;
    float tValues[6] = {tCone[0], tCone[1], tSphereA[0], tSphereA[1], tSphereB[0], tSphereB[1]};

    for (int i = 0; i < 6; ++i) {
        float t = tValues[i];
        if (t >= 0.0) {
            tMin = NativeMin(tMin, t);
            tMax = NativeMax(tMax, t);
        }
    }

    if (tMin > tMax || tMin == 1e20) {
        // No valid intersection
        return float2(-1.0);
    } else {
        return float2(tMin, tMax);
    }
}

// https://iquilezles.org/articles/distfunctions
float dot2(float3 v ) { return dot(v,v); }


//  Value Noise 3D Deriv
//  Return value range of 0.0->1.0, with format float4( value, xderiv, yderiv, zderiv )
//


// shockwave UV Displacement
// Calculate how much the UV coordinates should be displaced at currentpos in world space of the screen copy texture to simulate a shockwave starting from position centerpos, with the camera looking at viewdirections.
// The maximum radius of the shockwave is given in shockwaveRadius, and the timeFraction goes from 0 to 1 to simulate the shockwave expanding over time.
// Ensure that the UV displacement is correctly rotated given the angle between the viewDirection and the direction of the shockwave starting from centerpos

// returns the Screen UV coordinates [0-1] and estimated depth of a given world position
float3 worldToScreenUVZ(float3 worldPosition){
	float4 screenPos = matrixBuffer->cameraViewProj * float4(worldPosition, 1.0);
	screenPos.xyz = screenPos.xyz / screenPos.w;
	screenPos.xyz = screenPos.xyz * 0.5 + 0.5;
	return screenPos.xyz;
}

// Parabola (try it in graphtoy.com) A nice choice to remap the 0..1 interval into 0..1, such that the corners are mapped to 0 and the center to 1. You can then rise the parabolar to a power k to control its shape.
// https://iquilezles.org/articles/functions/

// Power Curve This is a generalization of the Parabola() above. It also maps the 0..1 interval into 0..1 by keeping the corners mapped to 0. But in this generalziation you can control the shape one either side of the curve, which comes handy when creating leaves, eyes, and many other interesting shapes.
// https://iquilezles.org/articles/functions/
// Play with it here: https://www.desmos.com/calculator/2gecekm83w

// The precomputed scaling factor version
// Precompute K with the calculator here: https://www.desmos.com/calculator/2gecekm83w
float pcurve_k( float x, float a, float b, float k )
{
    return k*pow(x,a)*pow(1.0-x,b);
}

void Run()
{
	//-------------------------- BEGIN SHARED SECTION ---------------------
	int effectType = int(round(EFFECTTYPE));

	// TODO: Get the view vector before fetching texels for speedups, we can't really bail on all that many fragments...
	float mapdepth = Sample_mapDepths( v_screenUV, 0.0).x;
	float modeldepth = Sample_modelDepths( v_screenUV, 0.0).x;

	float worlddepth = NativeMin(mapdepth, modeldepth);
	// TODO: isn't modeldepth 0 where there is no model fucking up everything later on?

	float ismodel = 0;
	// Only query the textures if the backface of the volume is further than the world fragment

	// We are processing a model fragment
	if (modeldepth < mapdepth) {
		ismodel = 1;
	}

	float4 fragWorldPos =  float4( float3(v_screenUV.xy * 2.0 - 1.0, worlddepth),  1.0);

	// reconstruct view position from depth and camera view proj inv:

	fragWorldPos = matrixBuffer->cameraViewProjInv * fragWorldPos;
	fragWorldPos.xyz = fragWorldPos.xyz / fragWorldPos.w; // YAAAY this works!
	//DEBUGPOS(fragWorldPos.xyz * 0.1); // Debug fragment world position

	float3 camPos = matrixBuffer->cameraViewInv[3].xyz;

	float fragDistance = length(camPos - fragWorldPos.xyz);
	float3 viewDirection = (camPos - fragWorldPos.xyz) / fragDistance; // vector pointing in the direction of the eye ray


	float distortionRadius = v_worldPosRad.w;
	float distortionRadiusInv = 1.0 / distortionRadius;

	float3 distortionToWorld = float3(0); // The vector pointing from distortion source to fragment world pos
	float3 distortionPosition = float3(0); // This is the position of the distortion illuminating the fragment
	float4 distortionEmitPosition = float4(0); // == distortionPosition, except for beams, where the distortionEmitPosition for a ray is different than for a world fragment
	float3 EntryPoint = float3(0); // Point of entry into the distortion volume from the camera
	float3 ExitPoint =  float3(0); // point of exit from the distortion volume away from the camera
	float3 MidPoint = float3(0); // midpoint between entry and exit points
	float2 nearFarDistances = float2(0); // the distances to the nearest and farthest points on the distortion source

	float4 closestPointOnRay = float4(0); // the point on the ray that is closest to the distortion source (xyz) and the distance to it (w)

	// In frames of course, plus timeOffset
	float currentTime = paramsBuffer->timeInfo.x + paramsBuffer->timeInfo.w;

	// Distortioning components we wish to collect along the way:
	float distance_attenuation = 0; // Just the distance from the distortion source (multiplied with falloff for cones

	float relativeDensity = 1.0;
	//fragColor.rgba = float4(fract(fragWorldPos.xyz * 0.1),1.0); return; // Debug fragment world position

	// Fragments that add nothing are discarded (not written as zero): the widget runs an
	// occlusion query over this pass, and only when samples passed does it copy the screen
	// and run the full-screen combine pass. Additive blending of zero and a discard look the same.
	if (drawUniforms->pointbeamcone < 0.5){ //point
		distortionPosition = v_worldPosRad.xyz;
		distortionEmitPosition.xyz = distortionPosition;

		distortionToWorld = fragWorldPos.xyz - distortionPosition;
		closestPointOnRay = closestdistortionlp_distance(camPos, viewDirection, distortionPosition);
		distortionEmitPosition.w = closestPointOnRay.w;

		// Need to invert view direction as we need ray pointing from camera to sphere
		nearFarDistances = raySphereIntersect(camPos, -1 * viewDirection, distortionPosition, distortionRadius);
		EntryPoint = camPos + nearFarDistances.x * -viewDirection;
		ExitPoint = camPos + nearFarDistances.y * -viewDirection;
		MidPoint = (EntryPoint + ExitPoint) * 0.5;
		distance_attenuation = NativeClamp( 1.0 - length (distortionPosition - MidPoint) * distortionRadiusInv, 0, 1);
		relativeDensity = NativeClamp(length(EntryPoint- ExitPoint) / (2*distortionRadius), 0.0, 1.0);

	}else if (drawUniforms->pointbeamcone < 1.5){ // beam
		float3 beamHalfLength = v_worldPosRad2.xyz - v_worldPosRad.xyz;
		float3 beamStart = v_worldPosRad.xyz - beamHalfLength;
		float3 beamEnd = v_worldPosRad.xyz + beamHalfLength;
		distortionPosition = closestbeam(fragWorldPos.xyz, beamStart, beamEnd);

		distortionToWorld = fragWorldPos.xyz - distortionPosition;

		closestPointOnRay = ray_line_segment_closestpoint_on_ray_and_segment2(camPos, -viewDirection, beamStart, beamEnd, distortionEmitPosition);

		// Find the close and far distances to the beam volume
		nearFarDistances.x =  capIntersect( camPos, -viewDirection, beamStart, beamEnd, distortionRadius);
		nearFarDistances.y = - capIntersect( camPos, viewDirection, beamStart, beamEnd, distortionRadius);

		EntryPoint = (-viewDirection) * nearFarDistances.x + camPos;
		ExitPoint =  (-viewDirection) * nearFarDistances.y + camPos;
		MidPoint = (EntryPoint + ExitPoint) * 0.5;
		distance_attenuation =  NativeClamp( 1.0 - closestPointOnRay.w *distortionRadiusInv, 0,1);
		relativeDensity = NativeClamp(length(EntryPoint- ExitPoint) / (2*distortionRadius), 0.0, 1.0);

	}else if (drawUniforms->pointbeamcone > 1.5){ // cone
		distortionPosition = v_worldPosRad.xyz;

		distortionToWorld = fragWorldPos.xyz - distortionPosition;

		float3 coneDirection = normalize(v_worldPosRad2.xyz);

		float coneAngleCosine = v_worldPosRad2.w;
		float coneHalfAngleSine = sqrt(1.0 - coneAngleCosine * coneAngleCosine);

		//Cone maximizing sphere: http://mathcentral.uregina.ca/QQ/database/QQ.09.07/s/juan1.html
		float coneHeight = distortionRadius;
		float coneWidth = distortionRadius * coneHalfAngleSine;
		float coneSideLengthInv = rsqrt(coneHeight * coneHeight + coneWidth * coneWidth);
		float biggestradius = coneHeight * coneWidth * coneSideLengthInv / ( 1 + coneWidth * coneSideLengthInv);
		float3 endPoint = distortionPosition  + coneDirection * (distortionRadius - biggestradius);
		nearFarDistances = intersectRoundedCone(camPos, -viewDirection, distortionPosition, endPoint , biggestradius * 0.11, biggestradius) ;

		EntryPoint = camPos + nearFarDistances.x * -viewDirection;
		ExitPoint = camPos + nearFarDistances.y * -viewDirection;
		MidPoint = (EntryPoint + ExitPoint) * 0.5;

		// Calculate the closest point on the view ray to the cone
		closestPointOnRay = ray_line_segment_closestpoint_on_ray_and_segment2(camPos, -viewDirection, distortionPosition, endPoint, distortionEmitPosition);

		distance_attenuation = NativeClamp( 1.0 - length(MidPoint - distortionPosition)  / coneHeight, 0,1) * 1.0;
		float distortionAngleCosine  = dot(coneDirection, normalize(MidPoint - distortionPosition));
		float coneEdgeFactor = NativeClamp((distortionAngleCosine - coneAngleCosine) / (1.0 - coneAngleCosine), 0.0, 1.0);
		coneEdgeFactor = sqrt(	coneEdgeFactor) * 4;
		relativeDensity = NativeClamp(length(EntryPoint- ExitPoint) / (2*biggestradius), 0.0, 1.0);

		// Dampen cone distortion slightly when viewed perpendicular to cone axis
		// (perpendicular rays traverse more volume, making it appear disproportionately strong)
		float viewConeAlignment = abs(dot(viewDirection, coneDirection));
		relativeDensity *= mix(0.7, 1.0, viewConeAlignment);
	}

	// If the fragment is inside the volume, we need to calculate the volumetric fraction
	float volumetricFraction = 1.0; // The fraction of the ray that passes through the volume.
	if (fragDistance < nearFarDistances.y) {
		volumetricFraction = 1.0 - NativeClamp( abs(nearFarDistances.y - fragDistance) / abs(nearFarDistances.y - nearFarDistances.x), 0.0, 1.0);
	}
	if (fragDistance < nearFarDistances.x || nearFarDistances.x < 0.0) {
		discard_fragment(); // nothing to add
	}

	// Check if the volume is occluded, bail if yes!
	// if ((fragDistance < nearFarDistances.x) || (nearFarDistances.y - nearFarDistances.x < 1e-6)){ fragColor.rgba = float4(0.0); return; }
	// TODO: sample noise here, as most effects need the noise sample, and it should be a shared path along the branches


	//-------------------------- END SHARED SECTION ---------------------


	// ------------------------ BEGIN AIR SHOCKWAVE ----------------
	if (effectType == 1){
		// Create an air shockwave displacement based on the distance to the distortion source:
		// Bend the distortion beam as many times as it goes through the sphere!
			// Lifetime goes from 0 to 1
			float lifeStart = SPAWNFRAME;
			float lifeTime = LIFETIME;
			//float timeFraction = NativeClamp((currentTime - lifeStart) / lifeTime, 0.0, 1.0);
			float elapsedframes = currentTime - lifeStart;

			// From 0 to 1 over the lifetime of the airshockwave
			float timeFraction = fract((currentTime - lifeStart) / lifeTime);

			// which direction is the effect going in in world coords?, always points towards us!
			float3 shockDirection = normalize(EntryPoint.xyz - distortionEmitPosition.xyz);

			// Parabolic mapping of 0-1 to [0-1-0] with a parabola on timeFraction
			float parabolicStrength = pcurve_k(timeFraction, 0.5, 2.0, 3.5);

			// Falloff due to distance from camera
			float distanceToCameraFactor =  NativeClamp(300.0/ fragDistance, 0.0, 1.0);

			// Screen-space position of the center of the shockwave:
			float4 DistortionScreenPosition = matrixBuffer->cameraViewProj * float4(distortionEmitPosition.xyz, 1.0);
			DistortionScreenPosition.xyz = DistortionScreenPosition.xyz / DistortionScreenPosition.w;

			// the normal of the shockwave sphere
			// Get the noise sample:
			float3 noisePosition = fragWorldPos.xyz;

			if (NOISESCALESPACE < 0.0){
				noisePosition -= v_worldPosRad.xyz;
			}

			// Scale the noise position with the noise scale
			noisePosition *= abs(NOISESCALESPACE) * 0.03;


			// Add the time offset and wind offsets to the noise position
			float3 noiseOffset = float3(0.0); //float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));

			// Normalize the noise sample at noisePosition - noiseOffset
			float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0) )* 2.0 - 1.0;


			float refractiveIndex = EFFECTPARAM2 + (noiseSampleNorm.r + 0.5) * NOISESTRENGTH * 0.25 ;

			//Use Snell's law
			float cosTheta1 = dot(-viewDirection, shockDirection);
			float sinTheta1 = sqrt(1.0 - cosTheta1 * cosTheta1);
			float sinTheta2 = sinTheta1 / refractiveIndex;

			float rayBendElmos = 0;
			rayBendElmos = sin(asin(sinTheta1) - asin(sinTheta2));

			// At extremely oblique angles, the refraction can be so strong that the ray bends back towards the camera
			// This can be avoided, by softening the refraction at the edges
			// So shockWidth should reduce rayBendElmos when abs(cosTheta) is near 0
			float shockWidth = EFFECTPARAM1;
			float shockWaveEdgeSoften = pow(distance_attenuation, abs(shockWidth));
			rayBendElmos *= shockWaveEdgeSoften;

			// fragment we are checking is occluding the sphere
			if (fragDistance < nearFarDistances.x){
				fragColor.rgba = float4(0.0 );
				return;
			}

			// fragment is behind the sphere
			if ((fragDistance > nearFarDistances.y) && shockWidth > 0.0){
				rayBendElmos *= 2.0;
			}

			// The displacement is proportional to the distance of the fragment from the intersections of the sphere

			// screen-space direction of the shockwave
			float2 displacementScreen = normalize((DistortionScreenPosition.xy * 0.5 + 0.5) - v_screenUV);
			float overallStrength = 10 * rayBendElmos *  distanceToCameraFactor * parabolicStrength * LIFESTRENGTH;
			float2 displacementAmount = displacementScreen * overallStrength;

			fragColor.rgba = float4(displacementAmount * EFFECTSTRENGTH, 0.0, 1.0 );
	}


	//------------------------- BEGIN GROUND SHOCKWAVE -------------------------
	else if (effectType == 2){
		// Create a ground shockwave displacement that only displaces ground fragments based on the distance to the distortion source:
		// TODO: instead of using radius here, adjust radius in the vertex shader!

		// Get the noise sample:
		float3 noisePosition = fragWorldPos.xyz;

		if (NOISESCALESPACE < 0.0){
			noisePosition -= v_worldPosRad.xyz;
		}

		// Scale the noise position with the noise scale
		noisePosition *= abs(NOISESCALESPACE) * 0.03;

		// Add the time offset and wind offsets to the noise position
		//float3 noiseOffset = float3(0.0); //float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));
		float3 noiseOffset = float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, 0.1,  float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * ( float3(WINDAFFECTED, 5,  WINDAFFECTED));

		// Normalize the noise sample at noisePosition - noiseOffset
		float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0) )* 2.0 - 1.0;

		// Lifetime goes from 0 to 1
		float timeFraction = NativeClamp((currentTime - SPAWNFRAME) / LIFETIME, 0.0, 1.0);

		#define COMPRESSION 0.9
		float currentDist =  COMPRESSION * distortionRadius + 10 * NOISESTRENGTH * noiseSampleNorm.r;
		// TODO : Parameterize out width in elmos
		float width = EFFECTPARAM1;

		float groundDistanceToShockCenter = length(fragWorldPos.xyz - distortionPosition.xyz);

		// Effect strength and direction should be abs width'd
		// But it should be wider on the rarefaction side than the compression side
		// First term is the compression factor, second term is the rarefaction factor
		float compressionFactor = NativeMax((groundDistanceToShockCenter - currentDist) * COMPRESSION, (currentDist - groundDistanceToShockCenter) * (1.0 - COMPRESSION));

		float effectStrength =  NativeClamp( 1.0 - abs(compressionFactor/ width), 0.0, 1.0);
		effectStrength = pow(effectStrength, 3.0);

		// Parabolic mapping of 0-1 to [0-1-0] with a parabola on timeFraction
		float parabolicStrength = pcurve_k(timeFraction, 0.5, 2.0, 3.5);
		parabolicStrength= 1.0;

		// Falloff due to distance from camera
		// TODO: this should really not be based on fragDistance, but on the distance to the distortion source
		float distanceToCameraFactor =  NativeClamp(1000.0/ fragDistance, 0.0, 1.0);

		// Screen-space position of the center of the shockwave:

		float4 DistortionScreenPosition = matrixBuffer->cameraViewProj * float4(distortionEmitPosition.xyz, 1.0);
		DistortionScreenPosition.xyz = DistortionScreenPosition.xyz / DistortionScreenPosition.w;


		// screen-space direction of the shockwave
		float2 displacementScreen = normalize((DistortionScreenPosition.xy * 0.5 + 0.5) - v_screenUV);
		float overallStrength = effectStrength * distanceToCameraFactor * parabolicStrength * LIFESTRENGTH;

		float2 displacementAmount = displacementScreen * overallStrength;
		fragColor.rgba = float4(displacementAmount * EFFECTSTRENGTH, 0.0, 0.5 * NativeStep(0.005,overallStrength) );
	}


	//------------------------- AIRJET (3), GRAVITYLENS (4), FUSIONSPHERE (5), CLOAKDISTORTION (6) -------------------------
	// not implemented: write nothing rather than leaving fragColor undefined
	else if (effectType >= 3 && effectType <= 6){
		discard_fragment(); // nothing to add
	}


	//------------------------- BEGIN SHIELDSPHERE -------------------------
	else if (effectType == 7){
		float distortionAttenuation = 1.0; // start at max for the center


		// This should only highlight the very edge of the shield sphere

		float shieldEdgeFactor = NativeClamp(0.9 - abs(length(distortionEmitPosition.xyz - closestPointOnRay.xyz) - distortionRadius +10) * 0.10, 0.0, 1.0);
		//shieldEdgeFactor = pcurve_k(shieldEdgeFactor, 0.7, 100.0, 17.0);
		shieldEdgeFactor = smoothstep(0.1, 1.0, shieldEdgeFactor);
		if (fragDistance < ( length(distortionPosition - camPos) -distortionRadius)){
			discard_fragment(); // nothing to add
		}

		//D
		distortionAttenuation *= shieldEdgeFactor;
		//

		// TODO: Ensure that the distortion is proportionate to the amount of "Hot" volume the ray passes through.
		// Get the noise sample:
		float3 noisePosition = MidPoint.xyz;

		if (NOISESCALESPACE < 0.0){
			noisePosition -= v_worldPosRad.xyz;
		}

		// Scale the noise position with the noise scale
		noisePosition *= abs(NOISESCALESPACE) * 0.03;

		// Add the time offset and wind offsets to the noise position
		float3 noiseOffset = float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));

		// Normalize the noise sample at noisePosition - noiseOffset
		float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0) )* 2.0 - 1.0;


		// modulate the effect strength with the distance to the heat source:
		float distanceToCameraFactor =  NativeClamp(300.0/ length(camPos.xyz - MidPoint.xyz), 0.0, 1.0);
		noiseSampleNorm *= distanceToCameraFactor * LIFESTRENGTH;

		// Modulate alpha with the distortionAttenuation
		noiseSampleNorm *= NOISESTRENGTH;
		fragColor.rgba = float4(noiseSampleNorm.ra * EFFECTSTRENGTH, 0.0,  distortionAttenuation);
	}

	//------------------------- BEGIN MAGNIFIER -------------------------
	else if (effectType == 8){
		// Calculate the view-space vector that points from from distortion center to the view ray
		float3 magnifierDir = normalize(MidPoint - distortionPosition);
		float3 magnifierNormal = normalize(magnifierDir);

		float3 DistortionScreenPosition = worldToScreenUVZ(distortionEmitPosition.xyz);

		float3 MagnifierScreenPosition = worldToScreenUVZ(MidPoint);

		float3 dirInCameraSpace2 = normalize(MagnifierScreenPosition - DistortionScreenPosition);
		float relativeDistance = NativeClamp(1.0 - length(MidPoint - distortionEmitPosition.xyz) / distortionRadius, 0.0, 1.0);
		dirInCameraSpace2 *= 1.0 - sqrt(relativeDistance);
		dirInCameraSpace2 *= -3.0;

		float distanceToCameraFactor =  NativeClamp(300.0/ length(camPos - distortionEmitPosition.xyz), 0.0, 1.0);
		dirInCameraSpace2 *= distanceToCameraFactor * EFFECTPARAM1;
		//printf(EFFECTPARAM1);
		fragColor.rgba = float4(dirInCameraSpace2.xy * EFFECTSTRENGTH, 0.0, 1.0);
	}

	//------------------------- BEGIN Motion Blur -------------------------
	else if (effectType == 11){
		float3 unittravelDirection = normalize(v_worldPosRad2.xyz);

		float3 unitUV = worldToScreenUVZ(v_worldPosRad.xyz);
		float3 unitUV2 = worldToScreenUVZ(v_worldPosRad.xyz + v_worldPosRad2.xyz);
		float3 travelDirectionScreen = unitUV2 - unitUV;
		if (length(travelDirectionScreen) < 0.001) { discard_fragment(); }
		travelDirectionScreen = normalize(travelDirectionScreen);

		float travelSpeedMult = NativeClamp((v_worldPosRad2.w - EFFECTPARAM1) * EFFECTPARAM2, 0.0, 1.0);

		fragColor.rgba = float4(travelDirectionScreen.xy * EFFECTSTRENGTH, -10.0, 1.0);

		if (fragDistance > nearFarDistances.y){
			discard_fragment(); // nothing to add
		}
	}

	//------------------------- BEGIN TACHYON BEAM -------------------------
	else if (effectType == 12){
		float distortionAttenuation = 1.0; // start at max for the center


		// This should only highlight the very edge of the shield sphere

		float shieldEdgeFactor = NativeClamp(1.0 - abs(length(distortionEmitPosition.xyz - closestPointOnRay.xyz) - distortionRadius +6) * 0.12, 0.0, 1.0);
		//shieldEdgeFactor = pcurve_k(shieldEdgeFactor, 0.1, 100.0, 75.0);
		shieldEdgeFactor = smoothstep(0.2, 1.5, shieldEdgeFactor);
		if (fragDistance < ( length(distortionPosition - camPos) -distortionRadius)){
			discard_fragment(); // nothing to add
		}

		//D
		distortionAttenuation *= shieldEdgeFactor;
		//

		// TODO: Ensure that the distortion is proportionate to the amount of "Hot" volume the ray passes through.
		// Get the noise sample:
		float3 noisePosition = MidPoint.xyz;

		if (NOISESCALESPACE < 0.0){
			noisePosition -= v_worldPosRad.xyz;
		}

		// Scale the noise position with the noise scale
		noisePosition *= abs(NOISESCALESPACE) * 0.035;

		// Add the time offset and wind offsets to the noise position
		float3 noiseOffset = float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));

		// Normalize the noise sample at noisePosition - noiseOffset
		float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0) )* 2.0 - 1.0;


		// modulate the effect strength with the distance to the heat source:
		float distanceToCameraFactor =  NativeClamp(300.0/ length(camPos.xyz - MidPoint.xyz), 0.0, 1.0);
		noiseSampleNorm *= distanceToCameraFactor * LIFESTRENGTH;

		// Modulate alpha with the distortionAttenuation
		noiseSampleNorm *= NOISESTRENGTH;
		fragColor.rgba = float4(noiseSampleNorm.ra * EFFECTSTRENGTH, 0.0,  distortionAttenuation);
	}

// ------------------------ BEGIN FILLED DISTORTION CIRCLE (Updated ONLYMODELMAP & Y-Offset) ----------------
else if (effectType == 13) { // Keeping type 13
    // Simulates an expanding circle using noise displacement within a static max radius.
    // Expansion controlled by timeFraction/STARTRADIUS. Intensity affected by RAMPUP and DISTANCEFALLOFF.
    // Optionally restricted to model/map geometry via ONLYMODELMAP. Includes Y-offset for noise.

    // --- Lifetime & Timing ---
    float lifeStart = SPAWNFRAME;
    float lifeTime = LIFETIME;
    // Calculate elapsed time fraction (0 to 1 over lifetime), clamped.
    float timeFraction = NativeClamp((currentTime - lifeStart) / lifeTime, 0.0, 1.0);

    // Ramp-up factor based on elapsed time vs RAMPUP uniform (0 to 1 over ramp-up duration)
    float elapsed = currentTime - lifeStart;
    float rampFactor = 1.0;
    if (RAMPUP > 0.0) {
        rampFactor = NativeClamp(elapsed / RAMPUP, 0.0, 1.0);
    }

    // --- Check Max Radius & Calculate Distance ---
    // Use distortionRadius as the static maximum world-space radius for the effect volume.
    float maxRadius = distortionRadius; // Make sure distortionRadius is correctly defined/passed

    // Calculate the world-space distance from the fragment to the emission center
    float distWorld = distance(fragWorldPos.xyz, distortionEmitPosition.xyz);

    // If fragment is outside the maximum possible radius, exit early.
    if (distWorld > maxRadius || maxRadius <= 0.0) { // Added check for maxRadius > 0
        discard_fragment(); // nothing to add
    }

    // --- ONLYMODELMAP Check ---
    // Apply filtering based on ONLYMODELMAP setting compared against model depths.
    // ONLYMODELMAP > 0.5: Show only on map (discard model fragments)
    // ONLYMODELMAP < -0.5: Show only on models (discard map fragments)
    // Otherwise: Show on both
    if (abs(ONLYMODELMAP) > 0.5) { // Check if filtering is active
        // Sample the model depth map at the fragment's screen UV
        float modelDepthValue = Sample_modelDepths( v_screenUV, 0.0).r;

        // --- CRITICAL: Depth Linearization ---
        // Ensure fragDistance and modelDepthValue are comparable (linear depth).
        // Replace 'LinearizeDepth' with your actual depth linearization function.
        // float linearModelDepth = LinearizeDepth(modelDepthValue, cameraNearPlane, cameraFarPlane);
        float linearModelDepth = modelDepthValue; // Placeholder: Assumes comparable linear depth
        // --- End Depth Linearization Note ---

        // Define epsilon for depth comparison tolerance
        float depthEpsilon = 0.1; // Adjust based on world scale and depth precision

        if (ONLYMODELMAP > 0.5) { // Only show on MAP (discard if fragment IS model)
            // Discard if the fragment's depth is very close to the model's depth
            if (abs(fragDistance - linearModelDepth) < depthEpsilon) {
                discard_fragment(); // nothing to add // Fragment is part of the model surface, discard
            }
        } else { // ONLYMODELMAP < -0.5: Only show on MODEL (discard if fragment IS map/background)
            // Discard if the fragment's depth is significantly further than the model's depth
            // (This logic seemed to work according to the user, keeping it)
            if (fragDistance > linearModelDepth + depthEpsilon) {
                discard_fragment(); // nothing to add // Fragment is behind the model's surface, discard
            }
        }
    }
    // --- END ONLYMODELMAP Check ---


    // --- Intensity based on Simulated Expansion (Time) ---
    // Use the existing STARTRADIUS define (v_baseparams.z), assumed percentage (0.0 to 1.0).
    // Intensity starts increasing *after* timeFraction passes STARTRADIUS.
    float expansionIntensity = smoothstep(STARTRADIUS, 1.0, timeFraction); // Using STARTRADIUS define

    // --- Intensity based on Distance Falloff ---
    // Calculate normalized distance (0 at center, 1 at edge)
    float normalizedDist = distWorld / maxRadius; // Already checked maxRadius > 0
    // Base distance attenuation (1.0 at center, smoothly falling to 0.0 at edge)
    float baseDistAtten = 1.0 - smoothstep(0.0, 1.0, normalizedDist);
    // Apply the DISTANCEFALLOFF logic (similar to effectType 0)
    float distanceFalloffFactor = baseDistAtten;
    if (DISTANCEFALLOFF >= 0.0) {
        // Positive DISTANCEFALLOFF acts as an exponent, shaping the falloff curve
        distanceFalloffFactor = pow(baseDistAtten, DISTANCEFALLOFF);
    } else {
        // Negative DISTANCEFALLOFF uses a p-curve to make center weak and edge stronger (inverted effect)
        float dist_from_edge = baseDistAtten;
        float dist_from_center = 1.0 - dist_from_edge;
        distanceFalloffFactor = pow(dist_from_center, -DISTANCEFALLOFF) * dist_from_edge;
        distanceFalloffFactor = NativeClamp(distanceFalloffFactor, 0.0, 1.0);
    }

    // --- Combine Attenuations & Check ---
    // Combine time-based expansion, ramp-up, and distance-based falloff
    float distortionAttenuation = expansionIntensity * rampFactor * distanceFalloffFactor;

    // If the effect hasn't expanded/ramped up enough, or is fully attenuated by distance, exit.
    if (distortionAttenuation <= 0.0) { // Check combined attenuation
        discard_fragment(); // nothing to add
    }

    // --- Noise Calculation (Adapted from effectType 0 / previous version) ---
    float3 noisePosition = MidPoint.xyz; // Using MidPoint like heat distortion

    // <<< ADDED Y-OFFSET >>>
    noisePosition += float3(0.0, 5.0, 0.0); // Apply 5-unit Y offset before scaling/offsetting

    if (NOISESCALESPACE < 0.0) {
        noisePosition -= v_worldPosRad.xyz; // Apply offset if needed
    }
    noisePosition *= abs(NOISESCALESPACE) * 0.03; // Apply noise scale
    float3 noiseOffset = float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));
    float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0)) * 2.0 - 1.0; // Range [-1, 1]
    float distanceToCameraFactor = NativeClamp(300.0 / length(camPos.xyz - MidPoint.xyz), 0.0, 1.0);
    noiseSampleNorm *= distanceToCameraFactor * LIFESTRENGTH * NOISESTRENGTH;
    // --- End Noise Calculation ---

    // --- Occlusion Check (Standard Scene Depth) ---
    // This check remains relevant even with ONLYMODELMAP, ensuring the effect
    // doesn't draw in front of closer scene objects that might occlude the effect volume.
    if (fragDistance < nearFarDistances.x) {
        discard_fragment(); // nothing to add
    }
    // --- End Occlusion Check ---

    // --- Final Displacement & Output ---
    float2 displacementAmount = noiseSampleNorm.ra * EFFECTSTRENGTH;

    // Output: rg = displacement, a = combined attenuation
    fragColor.rgba = float4(displacementAmount, 0.0, distortionAttenuation);
}
// ------------------------ END FILLED DISTORTION CIRCLE (Updated ONLYMODELMAP & Y-Offset) ------------------

	//------------------------- BEGIN HEAT DISTORTION -------------------------
	// Debugging check attenuations for each distortion source type:
	// Draw attenuation as a color Green

	else if (effectType == 0){

		float distortionAttenuation = 1.0; // start at max for the center

		// Attenuate with distance, distance attenuation is inverse of distance
        distortionAttenuation *= distance_attenuation;

        // plug in custom distortion falloff power
        if (DISTANCEFALLOFF >= 0.0){
            distortionAttenuation = pow(distortionAttenuation, DISTANCEFALLOFF);
        } else
		{ // use a p-curve for negative values of DISTANCEFALLOFF to have no distortion at the center
            float dist = 1.0 - distortionAttenuation;
            // This is a parabolic curve that goes from 0 to 1 at the center
            distortionAttenuation = pow(dist,-1 * DISTANCEFALLOFF)*(1.0-dist);
        }

		// Multiply the relative volume density:
		distortionAttenuation *= relativeDensity;

		#if (HEAT_LEGACY_DOUBLE_FALLOFF == 1)
			// The original code applied the distance attenuation and the DISTANCEFALLOFF power a
			// second time here. Every heat effect was tuned against that, so it stays the default;
			// gfx_distortion_gl4.lua can switch it off to get the single, documented falloff.
			distortionAttenuation *= distance_attenuation;
			distortionAttenuation = pow(distortionAttenuation, DISTANCEFALLOFF);
		#endif


		// Take into account relative distance of ray point closest to distortion source to the distortion source
		distortionAttenuation *= volumetricFraction;

		// if the fragment is closer to the camera than the distortion source plus its radius, we can completely skip the distortion
		if (fragDistance < ( length(distortionPosition - camPos) -distortionRadius)){
			discard_fragment(); // nothing to add
		}

		// --- ADD THE RAMPUP LOGIC HERE ---
		float elapsed = currentTime - SPAWNFRAME; // how many frames (or time units) since spawn
		// For safety, if RAMPUP <= 0, this means "immediate" or avoid dividing by zero
		float rampFactor = 1.0;
		if (RAMPUP > 0.0) {
			rampFactor = NativeClamp(elapsed / RAMPUP, 0.0, 1.0);
		}

		// Now multiply the distortionAttenuation by rampFactor for the fade-in
		distortionAttenuation *= rampFactor;

		// TODO: Ensure that the distortion is proportionate to the amount of "Hot" volume the ray passes through.
		// Get the noise sample:
		float3 noisePosition = MidPoint.xyz;

		if (NOISESCALESPACE < 0.0){
			noisePosition -= v_worldPosRad.xyz;
		}

		// Scale the noise position with the noise scale
		noisePosition *= abs(NOISESCALESPACE) * 0.03;

		// Add the time offset and wind offsets to the noise position
		float3 noiseOffset = float3(float2(drawUniforms->windX,drawUniforms->windZ).x * 0.1, currentTime * 0.01, float2(drawUniforms->windX,drawUniforms->windZ).y * 0.1 ) * (1.0 + float3(WINDAFFECTED, EFFECTPARAM1, WINDAFFECTED));

		// Normalize the noise sample at noisePosition - noiseOffset
		float4 noiseSampleNorm = (Sample_noise3DCube( noisePosition - noiseOffset, 0.0) )* 2.0 - 1.0;

		// modulate the effect strength with the distance to the heat source:
		float distanceToCameraFactor =  NativeClamp(300.0/ length(camPos.xyz - MidPoint.xyz), 0.0, 1.0);
		noiseSampleNorm *= distanceToCameraFactor * LIFESTRENGTH;

		// Modulate alpha with the distortionAttenuation
		noiseSampleNorm *= NOISESTRENGTH;

		fragColor.rgba = float4(float3(noiseSampleNorm.ra, 0.0) * EFFECTSTRENGTH ,  distortionAttenuation);

	}else{
		fragColor.rgba = float4(1.0);// default fallthrough, should never EVER appear
	}

	// is a model fragment and we only want to affect map
	if ((ismodel > 0.5) && (ONLYMODELMAP > 0.5)) discard_fragment();

	// is map fragment and we only want to to affect models
	if ((ismodel < 0.5) && (ONLYMODELMAP < -0.5)) discard_fragment();

}

};
#undef LIFESTRENGTH
#undef EFFECTSTRENGTH
#undef STARTRADIUS
#undef NOISESTRENGTH
#undef NOISESCALESPACE
#undef DISTANCEFALLOFF
#undef ONLYMODELMAP
#undef SPAWNFRAME
#undef LIFETIME
#undef RAMPUP
#undef DECAY
#undef EFFECTPARAM1
#undef EFFECTPARAM2
#undef WINDAFFECTED
#undef EFFECTTYPE
#undef DEBUG
#undef DEBUGPOS
fragment float4 DistortionFragment(DistortionVarying input [[stage_in]],constant NativeDistortionMatrices& matrices [[buffer(2)]],constant NativeDistortionParams& params [[buffer(3)]],constant NativeDistortionDrawUniforms& uniforms [[buffer(6)]],NativeMapDepthTexture mapDepths [[texture(0)]],sampler mapDepthsSampler [[sampler(0)]],NativeModelDepthTexture modelDepths [[texture(1)]],sampler modelDepthsSampler [[sampler(1)]],texture3d<float> noise3DCube [[texture(2)]],sampler noise3DCubeSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.rasterPosition,get_sample_position(recoilOriginalSampleID));
 DistortionFragmentProgram program;program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;
 program.mapDepths=mapDepths;program.mapDepthsSampler=mapDepthsSampler;program.modelDepths=modelDepths;program.modelDepthsSampler=modelDepthsSampler;program.noise3DCube=noise3DCube;program.noise3DCubeSampler=noise3DCubeSampler;
 program.v_worldPosRad=input.v_worldPosRad;
 program.v_worldPosRad2=input.v_worldPosRad2;
 program.v_baseparams=input.v_baseparams;
 program.v_universalParams=input.v_universalParams;
 program.v_lifeParams=input.v_lifeParams;
 program.v_effectParams=input.v_effectParams;
#ifdef UNIFORMSBUFFERCOPY
 program.v_unibuffercopy=input.v_unibuffercopy;
#endif
 program.v_screenUV=input.v_screenUV;program.Run();if(!NativeDistortionAlpha(program.fragColor.a,uniforms))discard_fragment();return program.fragColor;
}
struct DistortionQuad {float4 position [[position]];float2 uv [[user(locn0), RECOIL_SAMPLE_PERSPECTIVE]];};
vertex DistortionQuad DistortionCombineVertex(uint id [[vertex_id]],device const float4* vertices [[buffer(0)]]){
 float4 v=vertices[id];return {float4(v.xy,0,1),v.zw};
}
struct DistortionCombineProgram {
 constant NativeDistortionDrawUniforms* drawUniforms;
 texture2d<float> screenCopyTexture;sampler screenCopySampler;
 texture2d<float> distortionTexture;sampler distortionSampler;
 float2 UV;float4 outputColor;
 float4 Screen(float2 uv){return NativeSampleLOD(screenCopyTexture,screenCopySampler,uv,0,0,*drawUniforms);}
 float4 Offsets(float2 uv){return NativeSampleLOD(distortionTexture,distortionSampler,uv,0,1,*drawUniforms);}
float3 colormap_jet(float x){
    float3 color = float3(0.0);
    float3 black = float3(0.0);
    float3 red = float3(1.0, 0.0, 0.0);
    float3 yellow = float3(1.0, 1.0, 0.0);
    float3 white = float3(1.0);
    float3 Blue = float3(0.0, 0.0, 1.0);
    if (x < 0.125) {
        color = black + (red - black) * (x - 0.0) / (0.125 - 0.0);
    } else if (x < 0.375) {
        color = red + (yellow - red) * (x - 0.125) / (0.375 - 0.125);
    } else if (x < 0.625) {
        color = yellow + (white - yellow) * (x - 0.375) / (0.625 - 0.375);
    } else if (x < 0.875) {
        color = white + (Blue - white) * (x - 0.625) / (0.875 - 0.625);
    } else {
        color = mix(Blue, black, 0.1* NativeClamp((x - 0.875) / (1.0 - 0.875),0,1));
    }
    return color;
}


float2 softClampScreen(float2 UV){
    return NativeClamp(UV, 0.5 * drawUniforms->inverseScreenResolution.xy, 1.0 - 0.5 * drawUniforms->inverseScreenResolution.xy);
}
#define DEBUG(x) float4( fract(x*0.999 + 0.0001), sqrt(floor(x) / 256.0),(x < 0? 1 : 0),1.0);
//__DEFINES__
void Run() {

    // As of yet, distortion coords are still stored as centered around 0.5, so we need to shift them to 0.0
    float4 distortion = Offsets( UV);
    distortion.rgb = distortion.rgb;
    distortion.rg = (1536.0 * distortion.rg) * drawUniforms->inverseScreenResolution.xy;
    if (length(distortion.rg) < 0.01) {
        // Bail early if no real distortion is present; a discard skips the blend's framebuffer read and write
        discard_fragment();
    }
    // Declare the UV sets and final screen color
    float2 offsetUV1;
    float2 offsetUV2;
    float2 offsetUV3;
    float4 outputRGBA = float4(0.0);


    // Regular distortion
    if (distortion.b > -1.0 ) {
        float2 distortionXY = distortion.rg * drawUniforms->overallStrength * 0.01;
        offsetUV1 = softClampScreen(UV + distortionXY);
        offsetUV2 = softClampScreen(UV + distortionXY / CHROMATIC_ABERRATION);
        offsetUV3 = softClampScreen(UV + distortionXY * CHROMATIC_ABERRATION);
    }else{
    // Motion blur
        float2 blurdirection = distortion.rg * 0.8;
        offsetUV1 = softClampScreen(UV - 2 * drawUniforms->inverseScreenResolution.xy * blurdirection);
        offsetUV2 = softClampScreen(UV + 2 * drawUniforms->inverseScreenResolution.xy * blurdirection);
        offsetUV3 = softClampScreen(UV + 4 * drawUniforms->inverseScreenResolution.xy * blurdirection);
    }


    float3 sample1 = Screen( offsetUV1).rgb;
    float3 sample2 = Screen( offsetUV2).rgb;
    float3 sample3 = Screen( offsetUV3).rgb;


    if (distortion.b > -1.0 ) { // Regular distortion
        outputRGBA.g = sample1.g;
        outputRGBA.r = sample2.r;
        outputRGBA.b = sample3.b;
        outputRGBA.rgb += 1.0 / 255.0;
        outputRGBA.a = 1.0;
    }else{ // Motion Blur
        outputRGBA.rgb = (sample1.rgb + sample2.rgb + sample3.rgb) / 3.0;
        outputRGBA.a = 0.7;
        //outputRGBA = float4( sample1, 1.0);

    }

    #if (DEBUGCOMBINER == 0)

        outputColor = outputRGBA ;

    #else
        if (UV.x > 0.66){ // right half?
            if (UV.y > 0.75){ // top right
                if (distortion.b < -0.01 )
                outputColor = float4(float3(distortion.rg, 0.0) * 0.5 + 0.5, 0.7);
                else outputColor = float4(outputRGBA.rgb, 0.0);
            }else{ // bottom right just straight up debug out the actual distortion RGB texture
                //outputColor = float4(distortion.rg * 0.5 + 0.5, -1 * distortion.b, 1.0);
                float distmag = length(distortion.rg/ (1536.0 * drawUniforms->inverseScreenResolution.xy));
                outputColor = float4(colormap_jet(distmag * 0.5), NativeStep(0.00001,distortion.a));
            }
        }else{ // left half
            outputColor = outputRGBA;
        }
    #endif


}
};
fragment float4 DistortionCombineFragment(DistortionQuad input [[stage_in]],constant NativeDistortionDrawUniforms& uniforms [[buffer(6)]],texture2d<float> screenCopy [[texture(0)]],sampler screenSampler [[sampler(0)]],texture2d<float> distortion [[texture(1)]],sampler distortionSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 DistortionCombineProgram p;p.drawUniforms=&uniforms;p.screenCopyTexture=screenCopy;p.screenCopySampler=screenSampler;p.distortionTexture=distortion;p.distortionSampler=distortionSampler;p.UV=input.uv;p.Run();if(!NativeDistortionAlpha(p.outputColor.a,uniforms))discard_fragment();return p.outputColor;
}
struct DistortionTransfer {float4 uv;float4 clear;};
vertex DistortionQuad DistortionTransferVertex(uint id [[vertex_id]]){
 const float2 v[6]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,-1),float2(1,1),float2(-1,1)};
 return {float4(v[id],0,1),v[id]*0.5+0.5};
}
fragment float4 DistortionCopyFragment(DistortionQuad input [[stage_in]],constant DistortionTransfer& u [[buffer(0)]],texture2d<float> screen [[texture(0)]],sampler state [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=u.uv.xy+input.uv*u.uv.zw;uv.y=1.0-uv.y;return screen.sample(state,uv);
}
fragment float4 DistortionClearFragment(constant DistortionTransfer& u [[buffer(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){return u.clear;}
)DISTMSL";
}
