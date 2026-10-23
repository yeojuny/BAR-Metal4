/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::outline {
inline constexpr char SHADER[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"OUTMETAL(/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include <metal_stdlib>
using namespace metal;
struct NativeOutlineDrawUniforms {
	float outlineWidth, quadPadding, zoomScale, resolutionScale;
	uint4 transport;
	float4 framebuffer, alpha;
	uint4 imageSwizzle[3];
	uint4 depthMode;
	uint4 imageSize[3];
};
struct NativeOutlineInstance {
	float4 bounds;
#if PER_UNIT == 1
	float4 color, shape, timing;
#endif
	uint4 instData;
};
struct NativeOutlineMatrices {
	float4x4 screenView;
	float4x4 screenProj;
	float4x4 screenViewProj;

	float4x4 cameraView;
	float4x4 cameraProj;
	float4x4 cameraViewProj;
	float4x4 cameraBillboardView;

	float4x4 cameraViewInv;
	float4x4 cameraProjInv;
	float4x4 cameraViewProjInv;

	float4x4 shadowView;
	float4x4 shadowProj;
	float4x4 shadowViewProj;

	float4x4 reflectionView;
	float4x4 reflectionProj;
	float4x4 reflectionViewProj;

	float4x4 orthoProj01;

	// transforms for [0] := Draw, [1] := DrawInMiniMap, [2] := Lua DrawInMiniMap
	float4x4 mmDrawView; //world to MM
	float4x4 mmDrawProj; //world to MM
	float4x4 mmDrawViewProj; //world to MM

	float4x4 mmDrawIMMView; //heightmap to MM
	float4x4 mmDrawIMMProj; //heightmap to MM
	float4x4 mmDrawIMMViewProj; //heightmap to MM

	float4x4 mmDrawDimView; //mm dims
	float4x4 mmDrawDimProj; //mm dims
	float4x4 mmDrawDimViewProj; //mm dims

};
struct NativeOutlineParams {
	packed_float3 rndVec3; //new every draw frame.
	uint renderCaps; //various render booleans

	float4 timeInfo; //gameFrame, drawSeconds, interpolated(unsynced)GameSeconds(synced), frameTimeOffset
	float4 viewGeometry; //vsx, vsy, vpx, vpy
	float4 mapSize; //xz, xzPO2
	float4 mapHeight; //height minCur, maxCur, minInit, maxInit

	float4 fogColor; //fog color
	float4 fogParams; //fog {start, end, 0.0, scale}

	float4 sunDir; // (sky != nullptr) ? sky->GetLight()->GetLightDir() : float4(/*map default*/ 0.0f, 0.447214f, 0.894427f, 1.0f);

	float4 sunAmbientModel;
	float4 sunAmbientMap;
	float4 sunDiffuseModel;
	float4 sunDiffuseMap;
	float4 sunSpecularModel; // float4{ sunLighting->modelSpecularColor.xyz, sunLighting->specularExponent };
	float4 sunSpecularMap; //  float4{ sunLighting->groundSpecularColor.xyz, sunLighting->specularExponent };

	float4 shadowDensity; //  float4{ sunLighting->groundShadowDensity, sunLighting->modelShadowDensity, 0.0, 0.0 };

	float4 windInfo; // windx, windy, windz, windStrength
	float2 mouseScreenPos; //x, y. Screen space.
	uint mouseStatus; // bits 0th to 32th: LMB, MMB, RMB, offscreen, mmbScroll, locked
	uint mouseUnused;
	float4 mouseWorldPos; //x,y,z; w=0 -- offmap. Ignores water, doesn't ignore units/features under the mouse cursor

	float4 teamColor[255]; //all team colors

};
struct SUniformsBuffer {
	uint composite; // u8 drawFlag; u8 unused1; u16 id;

	uint unused2;
	uint unused3;
	uint unused4;

	float maxHealth;
	float health;
	float unused5;
	float unused6;

	float4 speed;
	float4 userDefined[5]; // can't use float[20] because float in arrays occupies 4 * float space

};
struct Transform {
	float4 quat;
	float4 trSc;

};

static_assert(sizeof(NativeOutlineDrawUniforms)==176,"Outline draw ABI");
static_assert(sizeof(NativeOutlineInstance)==(PER_UNIT==1?80:32),"Original Outline instance stride");
static_assert(sizeof(NativeOutlineMatrices)==1664,"Original engine matrix declaration");
static_assert(sizeof(NativeOutlineParams)==4368,"Original LuaShader parameter declaration");
static_assert(sizeof(SUniformsBuffer)==128,"Original Outline model uniform stride");
static_assert(sizeof(Transform)==32,"Original quaternion stride");
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

inline float2 NativeImageUV(float2 uv,uint slot,constant NativeOutlineDrawUniforms& u) {
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
inline float4 NativeLogicalRGBA(float4 value,uint,constant NativeOutlineDrawUniforms&) { return value; }
inline float4 NativeLogicalRGBA(float depth,uint slot,constant NativeOutlineDrawUniforms& u) {
	uint mode=u.depthMode[slot];
	return mode==0x1906u ? float4(1,1,1,depth) : mode==0x8049u ? float4(depth) : float4(depth,depth,depth,1);
}
inline float4 NativeSourceSwizzle(float4 v,uint slot,constant NativeOutlineDrawUniforms& u) {
	uint4 swizzle=u.imageSwizzle[slot];
	return float4(NativeSourceChannel(v,swizzle.x),NativeSourceChannel(v,swizzle.y),
		NativeSourceChannel(v,swizzle.z),NativeSourceChannel(v,swizzle.w));
}
template<typename T> inline float4 NativeSampleLOD(T image,sampler state,float2 uv,float lod,uint slot,constant NativeOutlineDrawUniforms& u) {
	return NativeSourceSwizzle(NativeLogicalRGBA(image.sample(state,NativeImageUV(uv,slot,u),level(lod)),slot,u),slot,u);
}
inline float4 NativeGatherChannel(texture2d<float> image,sampler state,float2 uv,uint channel,uint,constant NativeOutlineDrawUniforms&) {
	switch(channel) {
		case 0: return float4(0); case 1: return float4(1);
		case 0x1903: return image.gather(state,uv,int2(0),component::x);
		case 0x1904: return image.gather(state,uv,int2(0),component::y);
		case 0x1905: return image.gather(state,uv,int2(0),component::z);
		case 0x1906: return image.gather(state,uv,int2(0),component::w);
		default: return float4(0);
	}
}
inline float4 NativeGatherChannel(depth2d<float> image,sampler state,float2 uv,uint channel,uint slot,constant NativeOutlineDrawUniforms& u) {
	if(channel==0) return float4(0);
	if(channel==1) return float4(1);
	uint mode=u.depthMode[slot];
	bool sampleDepth=mode==0x8049u || (mode==0x1906u ? channel==0x1906u : channel!=0x1906u);
	return sampleDepth ? image.gather(state,uv) : float4(1);
}
template<typename T> inline float4 NativeGather(T image,sampler state,float2 uv,int componentIndex,uint slot,constant NativeOutlineDrawUniforms& u) {
	float4 gathered=NativeGatherChannel(image,state,NativeImageUV(uv,slot,u),
		u.imageSwizzle[slot][componentIndex],slot,u);
	// A Y reflection reverses the footprint as well as its sample coordinates.
	// Restore GLSL's (0,1),(1,1),(1,0),(0,0) order in logical image space.
	return (u.transport.x&(1u<<slot)) ? gathered.wzyx : gathered;
}
inline int2 NativeImageSize(uint slot,int lod,constant NativeOutlineDrawUniforms& u) {
	return int2(max(uint2(1),u.imageSize[slot].xy>>uint(lod)));
}
inline bool NativeOutlineAlpha(float value,constant NativeOutlineDrawUniforms& u) {
	if(u.alpha.x==0) return true;
	float reference=u.alpha.y;
	switch(uint(u.alpha.z)) {
		case 0: return false; case 1: return value<reference;
		case 2: return value==reference; case 3: return value<=reference;
		case 4: return value>reference; case 5: return value!=reference;
		case 6: return value>=reference; default: return true;
	}
}


inline float3x3 NativeMat3(float a,float b,float c,float d,float e,float f,float g,float h,float i) {
	return float3x3(float3(a,b,c),float3(d,e,f),float3(g,h,i));
}
inline float4x4 NativeMat4(float a,float b,float c,float d,float e,float f,float g,float h,
	float i,float j,float k,float l,float m,float n,float o,float p) {
	return float4x4(float4(a,b,c,d),float4(e,f,g,h),float4(i,j,k,l),float4(m,n,o,p));
}
template<typename T> inline auto NativeEqual(T a,T b) { return a==b; }
template<typename T,typename U> inline T NativeMix(T a,T b,U value) { return metal::mix(a,b,value); }
inline float4 NativeMix(float4 a,float4 b,bool4 value) { return metal::select(a,b,value); }
template<typename T> inline float4 NativeTexelFetch(T image,int2 pixel,int lod,uint slot,constant NativeOutlineDrawUniforms& u) {
	// texelFetch bypasses filtering and wrap, including CLAMP_TO_BORDER.
	if(u.transport.x&(1u<<slot)) pixel.y=NativeImageSize(slot,lod,u).y-1-pixel.y;
	return NativeSourceSwizzle(NativeLogicalRGBA(image.read(uint2(pixel),uint(lod)),slot,u),slot,u);
}

constant float2 NEIGHBOURS[4]={float2(-1.0, 0.0), float2(1.0, 0.0), float2(0.0, -1.0), float2(0.0, 1.0)};
struct OutlineVarying {
float4 rasterPosition [[position]];
#if PER_UNIT == 1
float4 v_color [[user(locn0),flat]];
float v_width [[user(locn1),flat]];
float v_blur [[user(locn2),flat]];
uint v_id [[user(locn3),flat]];
#endif
};
struct OutlineVertexProgram {
constant NativeOutlineMatrices* matrixBuffer;
constant NativeOutlineParams* paramsBuffer;
constant NativeOutlineDrawUniforms* drawUniforms;
#if PER_UNIT == 1
float4 v_color;
float v_width;
float v_blur;
uint v_id;
#endif
float2 corner;float4 bounds;
#if PER_UNIT == 1
float4 color,shape,timing;
#endif
uint4 instData;float4 gl_Position;
device const SUniformsBuffer* uni;
#if USEQUATERNIONS == 0
device const float4x4* UnitPieces;
#else
device const Transform* transforms;
#endif





#line 5000





// glsl rotate convenience funcs: https://github.com/dmnsgn/glsl-rotate

float3x3 rotation3dX(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return NativeMat3(
		1.0, 0.0, 0.0,
		0.0, c, s,
		0.0, -s, c
	);
}

float3x3 rotation3dY(float a) {
	float s = sin(a);
	float c = cos(a);

  return NativeMat3(
    c, 0.0, -s,
    0.0, 1.0, 0.0,
    s, 0.0, c);
}

float3x3 rotation3dZ(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return NativeMat3(
		c, s, 0.0,
		-s, c, 0.0,
		0.0, 0.0, 1.0
	);
}

float4x4 scaleMat(float3 s) {
	return NativeMat4(
		s.x, 0.0, 0.0, 0.0,
		0.0, s.y, 0.0, 0.0,
		0.0, 0.0, s.z, 0.0,
		0.0, 0.0, 0.0, 1.0
	);
}

float4x4 translationMat(float3 t) {
	return NativeMat4(
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		t.x, t.y, t.z, 1.0
	);
}

float4x4 mat4mix(float4x4 a, float4x4 b, float alpha) {
	return (a * (1.0 - alpha) + b * alpha);
}

// Additional helper functions useful in Spring

float2 heightmapUVatWorldPos(float2 worldpos){
	float2 inverseMapSize = float2(1.0) / paramsBuffer->mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	float2 heightmaptexel = float2(8.0, 8.0);
	worldpos +=  float2(-8.0, -8.0) * (worldpos * inverseMapSize) + float2(4.0, 4.0) ;
	float2 uvhm = NativeClamp(worldpos, heightmaptexel, paramsBuffer->mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;
	return uvhm;
}

// This does 'mirror' style tiling of UVs like the way the map edge extension works
float2 heightmapUVatWorldPosMirrored(float2 worldpos) {
	float2 inverseMapSize = float2(1.0) / paramsBuffer->mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	float2 heightmaptexel = float2(8.0, 8.0);
	worldpos +=  float2(-8.0, -8.0) * (worldpos * inverseMapSize) + float2(4.0, 4.0) ;
	float2 uvhm = worldpos * inverseMapSize;

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
float SphereInViewSignedDistance(float3 centerWS,  float radiusWS)
{
    // 1.  centre → clip space
    float4 clipC = matrixBuffer->cameraViewProj * float4(centerWS, 1.0);
    if (clipC.w <= 0.0)           // behind the eye? treat as inside
        return -1.0;

    // 2.  NDC centre (one perspective divide, reused later)
    float2  ndc   = clipC.xy / clipC.w;        // = clipC.xy / clipC.w

    // 3.  Project world-space +X displacement
    //     M * (p + (r,0,0,0)) = (M*p) + r*M*Xcol
    float4 deltaClip = radiusWS * matrixBuffer->cameraViewProj[0];   // first column of matrix
    float4 clipX     = clipC + deltaClip;

    // 4.  Radius in NDC (second perspective divide only once)
    float ndcRadius = length((clipX.xy / clipX.w) - ndc);

    // 5.  Four signed distances, keep the worst (Chebyshev / max)
    float dLeft   = -(ndc.x + 1.0) - ndcRadius;
    float dRight  =  (ndc.x - 1.0) - ndcRadius;
    float dBottom = -(ndc.y + 1.0) - ndcRadius;
    float dTop    =  (ndc.y - 1.0) - ndcRadius;

    return NativeMax(NativeMax(dLeft, dRight), NativeMax(dBottom, dTop));
}



// Note that this function does not check the Z or depth of the clip space, but in regular springrts top-down views, this isn't needed either.
// the radius to cameradist ratio is a good proxy for visibility in the XY plane
bool isSphereVisibleXY(float4 wP, float wR){ //worldPos, worldRadius
	float3 ToCamera = wP.xyz - matrixBuffer->cameraViewInv[3].xyz; // vector from worldpos to camera
	float isqrtDistRatio = wR * rsqrt(dot(ToCamera, ToCamera)); // calculate the relative screen-space size of it
	float4 cWPpos = matrixBuffer->cameraViewProj * wP; // transform the worldpos into clip space
	float2 clipVec = cWPpos.ww * (1.0 + isqrtDistRatio); // normalize the clip tolerance
	return any(NativeGreaterThan(abs(cWPpos.xy), clipVec)); // check if the clip space coords lie outside of the tolerance relaxed [-1.0, 1.0] space
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



#if USEQUATERNIONS == 1
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
		return normalize(NativeMix(qa, qb, t));

	// Interpolation of orthogonal rotations (i.e. cosHalfTheta ~ 0)
	// does not require special handling, however this usually represents
	// "physically impossible" 180 degree turns with infinite speed so perhaps
	// it can be handled in the following (currently disabled) special way
	#if 0
	if (cosHalfTheta <= 0.005)
		return NativeMix(qa, qb, NativeStep(0.5, t));
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
	return normalize(NativeMix(qa, qb, t)); // GLSL's mix() = (1 - t) * qa + t * qb
}

Transform SLerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = NativeClamp(a, 0.0, 1.0);
	return NativeMakeTransform(
		SLerp(t0.quat, t1.quat, a),
		NativeMix(t0.trSc, t1.trSc, a)
	);
}


Transform Lerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = NativeClamp(a, 0.0, 1.0);
	return NativeMakeTransform(
		Lerp(t0.quat, t1.quat, a),
		NativeMix(t0.trSc, t1.trSc, a)
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


#if PER_UNIT == 1




#else

#endif





#if USEQUATERNIONS == 0
	
#endif

#line 10000

#if PER_UNIT == 1



	
#else

#endif

void Run()
{
	#if USEQUATERNIONS == 0
		float4x4 modelMatrix = UnitPieces[instData.x];
	#else
		float4x4 modelMatrix = TransformToMatrix(GetModelWorldTransform(instData.x));
	#endif

	#if PER_UNIT == 1
		float sizeScale = drawUniforms->resolutionScale * NativeMix(1.0, drawUniforms->zoomScale, shape.z);
		float width = NativeClamp(shape.x * sizeScale, 0.0, float(WIDTH_MAX));
		float blur = NativeClamp(shape.y * sizeScale, 0.0, float(WIDTH_MAX));
		float alpha = color.a;
		if (timing.z > 0.0) {
			alpha *= NativeClamp((paramsBuffer->timeInfo.y - timing.x) / timing.z, 0.0, 1.0);
			if (timing.y >= 0.0) {
				alpha *= 1.0 - NativeClamp((paramsBuffer->timeInfo.y - timing.y) / timing.z, 0.0, 1.0);
			}
		}
		if (timing.w > 0.0) {
			alpha *= 0.6 + 0.4 * cos(paramsBuffer->timeInfo.y * 6.2831853 / timing.w);
		}
		v_color = float4(color.rgb, alpha);
		v_width = width;
		v_blur = blur;
		v_id = uint(shape.w + 0.5);
		float padding = ceil(width + blur + 1.0) + 1.0; // the fragment shader's reach, plus one
	#else
		float alpha = 1.0;
		float padding = drawUniforms->quadPadding;
	#endif

	// drawFlag bits 0-1: drawn as a model, not as an icon. A zero position means the unit entered LOS but has
	// not been drawn yet, so its matrix is not uploaded.
	if (alpha <= 0.0 || (uni[instData.y].composite & 3u) == 0u || dot(modelMatrix[3].xyz, modelMatrix[3].xyz) < 1.0) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0); // all four corners here: a degenerate quad outside the view
		return;
	}

	float3 center = (modelMatrix * float4(bounds.xyz, 1.0)).xyz;
	float3 worldPos = center + (matrixBuffer->cameraViewInv[0].xyz * corner.x + matrixBuffer->cameraViewInv[1].xyz * corner.y) * bounds.w;
	gl_Position = matrixBuffer->cameraViewProj * float4(worldPos, 1.0);
	gl_Position.xy += corner * (2.0 * padding / paramsBuffer->viewGeometry.xy) * gl_Position.w;
}

};
vertex OutlineVarying OutlineVertex(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],device const float2* corners [[buffer(0)]],device const NativeOutlineInstance* instances [[buffer(1)]],constant NativeOutlineMatrices& matrices [[buffer(2)]],constant NativeOutlineParams& params [[buffer(3)]],constant NativeOutlineDrawUniforms& uniforms [[buffer(6)]],device const SUniformsBuffer* modelUniforms [[buffer(5)]]
#if USEQUATERNIONS == 0
,device const float4x4* modelTransforms [[buffer(4)]]
#else
,device const Transform* modelTransforms [[buffer(4)]]
#endif
){
OutlineVertexProgram program;program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;
device const NativeOutlineInstance& row=instances[instance];program.corner=corners[vertexIndex];program.bounds=row.bounds;program.instData=row.instData;program.uni=modelUniforms;
#if PER_UNIT == 1
program.color=row.color;program.shape=row.shape;program.timing=row.timing;
#endif
#if USEQUATERNIONS == 0
program.UnitPieces=modelTransforms;
#else
program.transforms=modelTransforms;
#endif
program.Run();OutlineVarying result;result.rasterPosition=program.gl_Position;
if(uniforms.transport.y&1u)result.rasterPosition.z=(result.rasterPosition.z+result.rasterPosition.w)*0.5f;
if(uniforms.transport.y&2u)result.rasterPosition.y=-result.rasterPosition.y;
#if PER_UNIT == 1
result.v_color=program.v_color;
result.v_width=program.v_width;
result.v_blur=program.v_blur;
result.v_id=program.v_id;
#endif
return result;
}
struct OutlineFragmentProgram {
constant NativeOutlineMatrices* matrixBuffer;
constant NativeOutlineParams* paramsBuffer;
constant NativeOutlineDrawUniforms* drawUniforms;
#if PER_UNIT == 1
float4 v_color;
float v_width;
float v_blur;
uint v_id;
#endif
float4 gl_FragCoord,fragColor;
depth2d<float> mapDepths;sampler mapDepthsSampler;
float4 Sample_mapDepths(float2 uv,float lod){return NativeSampleLOD(mapDepths,mapDepthsSampler,uv,lod,0,*drawUniforms);}
float4 Gather_mapDepths(float2 uv,int componentIndex){return NativeGather(mapDepths,mapDepthsSampler,uv,componentIndex,0,*drawUniforms);}
float4 Fetch_mapDepths(int2 pixel,int lod){return NativeTexelFetch(mapDepths,pixel,lod,0,*drawUniforms);}
depth2d<float> modelDepths;sampler modelDepthsSampler;
float4 Sample_modelDepths(float2 uv,float lod){return NativeSampleLOD(modelDepths,modelDepthsSampler,uv,lod,1,*drawUniforms);}
float4 Gather_modelDepths(float2 uv,int componentIndex){return NativeGather(modelDepths,modelDepthsSampler,uv,componentIndex,1,*drawUniforms);}
float4 Fetch_modelDepths(int2 pixel,int lod){return NativeTexelFetch(modelDepths,pixel,lod,1,*drawUniforms);}
texture2d<float> modelMisc;sampler modelMiscSampler;
float4 Sample_modelMisc(float2 uv,float lod){return NativeSampleLOD(modelMisc,modelMiscSampler,uv,lod,2,*drawUniforms);}
float4 Gather_modelMisc(float2 uv,int componentIndex){return NativeGather(modelMisc,modelMiscSampler,uv,componentIndex,2,*drawUniforms);}
float4 Fetch_modelMisc(int2 pixel,int lod){return NativeTexelFetch(modelMisc,pixel,lod,2,*drawUniforms);}




#line 20000





// glsl rotate convenience funcs: https://github.com/dmnsgn/glsl-rotate

float3x3 rotation3dX(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return NativeMat3(
		1.0, 0.0, 0.0,
		0.0, c, s,
		0.0, -s, c
	);
}

float3x3 rotation3dY(float a) {
	float s = sin(a);
	float c = cos(a);

  return NativeMat3(
    c, 0.0, -s,
    0.0, 1.0, 0.0,
    s, 0.0, c);
}

float3x3 rotation3dZ(float angle) {
	float s = sin(angle);
	float c = cos(angle);

	return NativeMat3(
		c, s, 0.0,
		-s, c, 0.0,
		0.0, 0.0, 1.0
	);
}

float4x4 scaleMat(float3 s) {
	return NativeMat4(
		s.x, 0.0, 0.0, 0.0,
		0.0, s.y, 0.0, 0.0,
		0.0, 0.0, s.z, 0.0,
		0.0, 0.0, 0.0, 1.0
	);
}

float4x4 translationMat(float3 t) {
	return NativeMat4(
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		t.x, t.y, t.z, 1.0
	);
}

float4x4 mat4mix(float4x4 a, float4x4 b, float alpha) {
	return (a * (1.0 - alpha) + b * alpha);
}

// Additional helper functions useful in Spring

float2 heightmapUVatWorldPos(float2 worldpos){
	float2 inverseMapSize = float2(1.0) / paramsBuffer->mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	float2 heightmaptexel = float2(8.0, 8.0);
	worldpos +=  float2(-8.0, -8.0) * (worldpos * inverseMapSize) + float2(4.0, 4.0) ;
	float2 uvhm = NativeClamp(worldpos, heightmaptexel, paramsBuffer->mapSize.xy - heightmaptexel);
	uvhm = uvhm	* inverseMapSize;
	return uvhm;
}

// This does 'mirror' style tiling of UVs like the way the map edge extension works
float2 heightmapUVatWorldPosMirrored(float2 worldpos) {
	float2 inverseMapSize = float2(1.0) / paramsBuffer->mapSize.xy;
	// Some texel magic to make the heightmap tex perfectly align:
	float2 heightmaptexel = float2(8.0, 8.0);
	worldpos +=  float2(-8.0, -8.0) * (worldpos * inverseMapSize) + float2(4.0, 4.0) ;
	float2 uvhm = worldpos * inverseMapSize;

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
float SphereInViewSignedDistance(float3 centerWS,  float radiusWS)
{
    // 1.  centre → clip space
    float4 clipC = matrixBuffer->cameraViewProj * float4(centerWS, 1.0);
    if (clipC.w <= 0.0)           // behind the eye? treat as inside
        return -1.0;

    // 2.  NDC centre (one perspective divide, reused later)
    float2  ndc   = clipC.xy / clipC.w;        // = clipC.xy / clipC.w

    // 3.  Project world-space +X displacement
    //     M * (p + (r,0,0,0)) = (M*p) + r*M*Xcol
    float4 deltaClip = radiusWS * matrixBuffer->cameraViewProj[0];   // first column of matrix
    float4 clipX     = clipC + deltaClip;

    // 4.  Radius in NDC (second perspective divide only once)
    float ndcRadius = length((clipX.xy / clipX.w) - ndc);

    // 5.  Four signed distances, keep the worst (Chebyshev / max)
    float dLeft   = -(ndc.x + 1.0) - ndcRadius;
    float dRight  =  (ndc.x - 1.0) - ndcRadius;
    float dBottom = -(ndc.y + 1.0) - ndcRadius;
    float dTop    =  (ndc.y - 1.0) - ndcRadius;

    return NativeMax(NativeMax(dLeft, dRight), NativeMax(dBottom, dTop));
}



// Note that this function does not check the Z or depth of the clip space, but in regular springrts top-down views, this isn't needed either.
// the radius to cameradist ratio is a good proxy for visibility in the XY plane
bool isSphereVisibleXY(float4 wP, float wR){ //worldPos, worldRadius
	float3 ToCamera = wP.xyz - matrixBuffer->cameraViewInv[3].xyz; // vector from worldpos to camera
	float isqrtDistRatio = wR * rsqrt(dot(ToCamera, ToCamera)); // calculate the relative screen-space size of it
	float4 cWPpos = matrixBuffer->cameraViewProj * wP; // transform the worldpos into clip space
	float2 clipVec = cWPpos.ww * (1.0 + isqrtDistRatio); // normalize the clip tolerance
	return any(NativeGreaterThan(abs(cWPpos.xy), clipVec)); // check if the clip space coords lie outside of the tolerance relaxed [-1.0, 1.0] space
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




#if PER_UNIT == 1
	
	#define OUTLINE_WIDTH v_width
#else

	#define OUTLINE_WIDTH drawUniforms->outlineWidth
#endif





bool4 and4(bool4 a, bool4 b) {
	return bool4(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w);
}

// The pixels the outline goes around: this unit's, or any unit's, and how the outline fades with the distance
#if PER_UNIT == 1
	// solid for `width` pixels around the member pixels, then fading out over `blur` more; the last pixel antialiases
	float Reach(float width) {
		return width + v_blur + 1.0;
	}

	int SearchRadius(float width, float reach) {
		return int(ceil(reach));
	}

	float EdgeAlpha(float width) {
		return NativeClamp(width - 1.0, 0.0, 1.0);
	}

	float OuterAlpha(float dist, float width, float reach) {
		float t = NativeClamp((reach - dist) / (v_blur + 1.0), 0.0, 1.0);
		return v_blur > 0.0 ? t * t * (3.0 - 2.0 * t) : t;
	}

	bool IsMember(float4 misc) {
		return all(NativeEqual(uint2(misc.rb * 255.0 + 0.5), uint2(v_id & 255u, v_id >> 8u)));
	}

	bool4 GatherMembers(float2 uv) {
		bool4 members = NativeEqual(uint4(Gather_modelMisc( uv, 0) * 255.0 + 0.5), uint4(v_id & 255u));
		if (any(members)) {
			members = and4(members, NativeEqual(uint4(Gather_modelMisc( uv, 2) * 255.0 + 0.5), uint4(v_id >> 8u)));
		}
		return members;
	}
#else
	// Zero-K's falloff
	float Reach(float width) {
		return width * OUTLINE_REACH;
	}

	int SearchRadius(float width, float reach) {
		return NativeMin(int(ceil(reach)), int(ceil(width)) + 3);
	}

	float EdgeAlpha(float width) {
		return 1.0 - 1.5 / width;
	}

	float OuterAlpha(float dist, float width, float reach) {
		return 1.0 - pow(dist / reach, float(1 + int(width / 2.0)));
	}

	bool IsMember(float4 misc) {
		return misc.g > 0.5;
	}

	bool4 GatherMembers(float2 uv) {
		return NativeGreaterThan(Gather_modelMisc( uv, 1), float4(0.5));
	}
#endif

// A member pixel counts as visible where the unit is in front of the terrain.
void Run()
{
	float width = OUTLINE_WIDTH;
	float2 texelSize = 1.0 / paramsBuffer->viewGeometry.xy;
	float2 texel = floor(gl_FragCoord.xy - paramsBuffer->viewGeometry.zw);
	int2 itexel = int2(texel);

	float alpha;
	if (IsMember(Fetch_modelMisc( itexel, 0)) && Fetch_mapDepths( itexel, 0).x > Fetch_modelDepths( itexel, 0).x) {
		// Visible member pixel: outline it where a neighbouring member pixel is behind the terrain
		float edge = 0.0;
		for (int i = 0; i < 4; i++) {
			float2 uv = (texel + 0.5 + NEIGHBOURS[i]) * texelSize;
			if (IsMember(Sample_modelMisc( uv, 0.0)) && Sample_mapDepths( uv, 0.0).x <= Sample_modelDepths( uv, 0.0).x) {
				edge = 1.0;
			}
		}
		alpha = edge * EdgeAlpha(width);
	} else {
		// Anything else: fade with the distance to the nearest visible member pixel within reach
		float reach = Reach(width);
		float reachSq = reach * reach;
		int radius = SearchRadius(width, reach);
		float nearestSq = reachSq; // no hit: alpha 0

		// textureGather reads a 2x2 block per tap, in the order (0, 1) (1, 1) (1, 0) (0, 0).
		// Blocks overhang the window by one texel on the high side: push those out of reach.
		for (int y = -radius; y <= radius; y += 2) {
			for (int x = -radius; x <= radius; x += 2) {
				float4 dx = float4(x, x + 1, x + 1, x);
				float4 dy = float4(y + 1, y + 1, y, y);
				float4 distSq = NativeMix(dx * dx + dy * dy, float4(1e9), NativeGreaterThan(NativeMax(dx, dy), float4(radius)));
				if (!any(NativeLessThan(distSq, float4(reachSq)))) continue;

				float2 uv = (texel + float2(x, y) + 1.0) * texelSize;
				bool4 isMember = GatherMembers(uv);
				if (!any(isMember)) continue;

				bool4 inFront = NativeGreaterThan(Gather_mapDepths( uv, 0), Gather_modelDepths( uv, 0));
				float4 hitSq = NativeMix(float4(nearestSq), distSq, and4(isMember, inFront));
				nearestSq = NativeMin(nearestSq, NativeMin(NativeMin(hitSq.x, hitSq.y), NativeMin(hitSq.z, hitSq.w)));
			}
		}
		// Hits at or beyond reach give alpha <= 0, same as no hit
		alpha = OuterAlpha(sqrt(nearestSq), width, reach);
	}

	#if PER_UNIT == 1
		alpha = NativeMax(alpha, 0.0) * v_color.a;
		if (alpha <= 0.0) discard_fragment();
		fragColor = float4(v_color.rgb, alpha);
	#else
		fragColor = float4(0.0, 0.0, 0.0, alpha);
	#endif
}

};
fragment float4 OutlineFragment(OutlineVarying input [[stage_in]],constant NativeOutlineMatrices& matrices [[buffer(2)]],constant NativeOutlineParams& params [[buffer(3)]],constant NativeOutlineDrawUniforms& uniforms [[buffer(6)]],depth2d<float> mapDepths [[texture(0)]],sampler mapDepthsSampler [[sampler(0)]],depth2d<float> modelDepths [[texture(1)]],sampler modelDepthsSampler [[sampler(1)]],texture2d<float> modelMisc [[texture(2)]],sampler modelMiscSampler [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.rasterPosition,get_sample_position(recoilOriginalSampleID));
OutlineFragmentProgram program;program.matrixBuffer=&matrices;program.paramsBuffer=&params;program.drawUniforms=&uniforms;
program.mapDepths=mapDepths;program.mapDepthsSampler=mapDepthsSampler;
program.modelDepths=modelDepths;program.modelDepthsSampler=modelDepthsSampler;
program.modelMisc=modelMisc;program.modelMiscSampler=modelMiscSampler;
#if PER_UNIT == 1
program.v_color=input.v_color;
program.v_width=input.v_width;
program.v_blur=input.v_blur;
program.v_id=input.v_id;
#endif
program.gl_FragCoord=float4(float2(input.rasterPosition.x,uniforms.framebuffer.z-input.rasterPosition.y)*uniforms.framebuffer.xy,input.rasterPosition.zw);
program.Run();
if(!NativeOutlineAlpha(program.fragColor.a,uniforms))discard_fragment();
return program.fragColor;
}


// glClear(STENCIL_BUFFER_BIT) has no user shader. This transport covers exactly
// the target/scissor while native stencil masks retain all unwritten bits.
vertex float4 OutlineClearVertex(uint index [[vertex_id]]) {
	return float4(index==1?3.0f:-1.0f,index==2?3.0f:-1.0f,0.0f,1.0f);
}
fragment float4 OutlineClearFragment( RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) { return float4(0); }
)OUTMETAL";
}
