/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string>
namespace rendering::metal4 {
inline constexpr const char* NativeGroundDecalShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct DecalInstance { float4 forcedHeight,posT,posB,uvMain,uvNorm,createParams1,createParams2,createParams3,createParams4;uint4 createParams5;float4 userDefined[2];};
struct DecalUniforms {float4x4 viewProjection,inverseViewProjection,shadow;float4 mapDims,mapDimsPO2,groundAmbient,groundDiffuse,sunDirection,waterMin,waterBase,waterAbsorb,frameScreen,fragmentToLogical;uint4 flags;};
struct DecalVarying {float4 position [[position]];float4 p0 [[flat]],p1 [[flat]],p2 [[flat]],p3 [[flat]],p4 [[flat]];float4 uvMain [[flat]],uvNorm [[flat]],data1 [[RECOIL_SAMPLE_PERSPECTIVE]],data2 [[flat]],data3 [[flat]],data4 [[flat]];float3 r0 [[flat]],r1 [[flat]],r2 [[flat]];};
constant float3 CUBE_VERT[36] = {
	//RIGHT
	float3( 1.0, -1.0, -1.0),
	float3( 1.0, -1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3( 1.0,  1.0, -1.0),
	float3( 1.0, -1.0, -1.0),
	//LEFT
	float3(-1.0, -1.0,  1.0),
	float3(-1.0, -1.0, -1.0),
	float3(-1.0,  1.0, -1.0),
	float3(-1.0,  1.0, -1.0),
	float3(-1.0,  1.0,  1.0),
	float3(-1.0, -1.0,  1.0),
	//TOP
	float3(-1.0,  1.0, -1.0),
	float3( 1.0,  1.0, -1.0),
	float3( 1.0,  1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3(-1.0,  1.0,  1.0),
	float3(-1.0,  1.0, -1.0),
	//BOTTOM
	float3(-1.0, -1.0, -1.0),
	float3(-1.0, -1.0,  1.0),
	float3( 1.0, -1.0, -1.0),
	float3( 1.0, -1.0, -1.0),
	float3(-1.0, -1.0,  1.0),
	float3( 1.0, -1.0,  1.0),
	//FRONT
	float3(-1.0, -1.0,  1.0),
	float3(-1.0,  1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3( 1.0,  1.0,  1.0),
	float3( 1.0, -1.0,  1.0),
	float3(-1.0, -1.0,  1.0),
	//BACK
	float3(-1.0,  1.0, -1.0),
	float3(-1.0, -1.0, -1.0),
	float3( 1.0, -1.0, -1.0),
	float3( 1.0, -1.0, -1.0),
	float3( 1.0,  1.0, -1.0),
	float3(-1.0,  1.0, -1.0)
};

struct DecalVertexProgram {
texture2d<float> heightTex,groundNormalTex;sampler heightSampler,normalSampler;
float4 mapDims;float curAdjustedFrame;uint flips;
float4 forcedHeight,posT,posB,uvMain,uvNorm,createParams1,createParams2,createParams3,createParams4;uint4 createParams5;
float4 gl_Position,vTranformedPos[5],vuvMain,vuvNorm,vData1,vData2,vData3,vData4;float3x3 vRotMat;float4x4 gl_ModelViewProjectionMatrix;
float2 ImageUV(float2 uv,uint bit){return float2(uv.x,(flips&(1u<<bit))?1-uv.y:uv.y);}
/////////////////////////////////////////////////////////

#define posTL posT.xy
#define posTR posT.zw
#define posBR posB.xy
#define posBL posB.zw

#define refHeight          forcedHeight.x
#define minHeight          forcedHeight.y
#define maxHeight          forcedHeight.z
#define forceHeightMode    forcedHeight.w

#define alpha              createParams1.x
#define alphaFalloff       createParams1.y
#define glow               createParams1.z
#define glowFalloff        createParams1.w

#define rot                createParams2.x
#define height             createParams2.y
#define dotElimExp         createParams2.z
#define cmAlphaMult        createParams2.w

#define createFrameMin     createParams3.x
#define createFrameMax     createParams3.y
#define uvWrapDistance     createParams3.z
#define uvTraveledDistance createParams3.w

#define forcedNormal       createParams4.xyz
#define visMult            createParams4.w

#define decalType          ((createParams5.x >> 8u * 0u) & 0xFu)
#define decalId            ((createParams5.x >> 8u * 1u) & 0xFFFu)
#define texTint            float4( \
							float((createParams5.y >> 8u * 0u) & 0xFFu) / 255.0,\
							float((createParams5.y >> 8u * 1u) & 0xFFu) / 255.0,\
							float((createParams5.y >> 8u * 2u) & 0xFFu) / 255.0,\
							float((createParams5.y >> 8u * 3u) & 0xFFu) / 255.0 \
						   )
#define glowTintMin        float4( \
							float((createParams5.z >> 8u * 0u) & 0xFFu) / 255.0,\
							float((createParams5.z >> 8u * 1u) & 0xFFu) / 255.0,\
							float((createParams5.z >> 8u * 2u) & 0xFFu) / 255.0,\
							float((createParams5.z >> 8u * 3u) & 0xFFu) / 255.0 \
						   )
#define glowTintMax        float4( \
							float((createParams5.w >> 8u * 0u) & 0xFFu) / 255.0,\
							float((createParams5.w >> 8u * 1u) & 0xFFu) / 255.0,\
							float((createParams5.w >> 8u * 2u) & 0xFFu) / 255.0,\
							float((createParams5.w >> 8u * 3u) & 0xFFu) / 255.0 \
						   )

/////////////////////////////////////////////////////////

#define midPoint          vTranformedPos[0]

#define vAlpha            vData1.x
#define vGlow             vData1.y
#define vDotElimExp       vData1.z
// vData1.w - empty
#define vHeight           vData2.x
#define vUVWrapDist       vData2.y
#define vUVOffset         vData2.z
#define vDecalType        vData2.w

#define vTintColor        vData3
#define vGlowColor        vData4

/////////////////////////////////////////////////////////

#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)

#line 100148

const float2 HM_TEXEL = float2(8.0, 8.0);
float HeightAtWorldPos(float2 wxz) {
	// Some texel magic to make the heightmap tex perfectly align:
	wxz +=  -HM_TEXEL * (wxz * mapDims.zw) + 0.5 * HM_TEXEL;

	float2 uvhm = clamp(wxz, HM_TEXEL, mapDims.xy - HM_TEXEL);
	uvhm *= mapDims.zw;

	return heightTex.sample(heightSampler, ImageUV(uvhm,0), level(0)).x;
}

float3 GetFragmentNormal(float2 wxz) {
	float3 normal;
	normal.xz = groundNormalTex.sample(normalSampler, ImageUV(wxz * mapDims.zw,1), level(0)).rg;
	normal.y  = sqrt(1.0 - dot(normal.xz, normal.xz));
	return normal;
}

// see the engine's rotateByUpVector()
float3 RotateByNormalVector(float3 p, float3 newUpDir, float3 rotAxis) {
	#define px (p.x)
	#define py (p.y)
	#define pz (p.z)

	#define yx (newUpDir.x)
	#define yy (newUpDir.y)
	#define yz (newUpDir.z)

	#define nx (rotAxis.x)
	#define ny (rotAxis.y)
	#define nz (rotAxis.z)

	float3 cm = float3(
		yy,
		sqrt(yx * yx + yz * yz),
		(nx * px + ny * py + nz * pz)* (1.0 - yy)
	);

	return float3(
		dot(cm, float3(px, (ny * pz - nz * py), nx)),
		dot(cm, float3(py, (nz * px - nx * pz), ny)),
		dot(cm, float3(pz, (nx * py - ny * px), nz))
	);

	#undef px
	#undef py
	#undef pz

	#undef yx
	#undef yy
	#undef yz

	#undef nx
	#undef ny
	#undef nz
}

const float DECAL_EXPLOSION = 2.0;

// uv ==> L, T, R, B
void Run(uint sourceVertex) {
	float3 relPos = CUBE_VERT[sourceVertex];

	// alpha
	float thisVertexCreateFrame = (float(relPos.x)>0.0?createFrameMax:createFrameMin);
	vAlpha         = alpha - (curAdjustedFrame - thisVertexCreateFrame) * alphaFalloff;
	vGlow          = glow  - (curAdjustedFrame - thisVertexCreateFrame) *  glowFalloff;
	float alphaMax = alpha - (curAdjustedFrame -        createFrameMax) * alphaFalloff;
	alphaMax *= visMult;
	vAlpha   *= visMult;

	vDecalType = float(decalType); //copy type only, don't care about ID

	#if 1
	if (alphaMax <= 0.0 || vDecalType <= 0.0) {
		vTranformedPos[0] = float4(0);
		vTranformedPos[1] = float4(0);
		vTranformedPos[2] = float4(0);
		vTranformedPos[3] = float4(0);
		vTranformedPos[4] = float4(0);

		gl_Position = float4(2.0, 2.0, 2.0, 1.0); //place outside of [-1;1]^3 NDC, basically cull out from the further rendering
		return;
	}
	#endif

	midPoint.xz = (posTL + posTR + posBL + posBR) * 0.25;

	// mid-point height
	midPoint.y = HeightAtWorldPos(midPoint.xz);

	// conditionally force relative height
	midPoint.y = mix(midPoint.y, refHeight + clamp(midPoint.y - refHeight, minHeight, maxHeight), float(forceHeightMode == 1.0));
	// conditionally force absolute height
	midPoint.y = mix(midPoint.y, clamp(midPoint.y, minHeight, maxHeight), float(forceHeightMode == 2.0));

	float sa = sin(rot);
	float ca = cos(rot);

	// groundNormal
	float3 groundNormal = float3(0);
	if (dot(forcedNormal.xyz, forcedNormal.xyz) == 0.0) {
		float2x2 rotMat2d = float2x2(float2(ca,-sa),float2(sa,ca));

		groundNormal += 2.0 * GetFragmentNormal(midPoint.xz);
		groundNormal += 1.0 * GetFragmentNormal(rotMat2d * (posTL - midPoint.xz) + midPoint.xz);
		groundNormal += 1.0 * GetFragmentNormal(rotMat2d * (posTR - midPoint.xz) + midPoint.xz);
		groundNormal += 1.0 * GetFragmentNormal(rotMat2d * (posBL - midPoint.xz) + midPoint.xz);
		groundNormal += 1.0 * GetFragmentNormal(rotMat2d * (posBR - midPoint.xz) + midPoint.xz);
		groundNormal  = normalize(groundNormal);
		//groundNormal  = GetFragmentNormal(midPoint.xz);
	} else {
		groundNormal = forcedNormal.xyz; //expected to be normalized by the CPU code
	}

	// get 2D orthonormal system
	float3 xDir = float3(ca, 0.0, sa);

	// don't rotate almost vertical cubes
	if (1.0 - groundNormal.y > 0.05) {
		// rotAxis is cross(Upvector, N), but Upvector is known to be (0, 1, 0), so simplify
		float3 rotAxis = normalize(float3(groundNormal.z, 0.0, -groundNormal.x));
		xDir = RotateByNormalVector(xDir, groundNormal, rotAxis);
	}
	float3 zDir = normalize(cross(xDir, groundNormal));  // ex. (0,0,1)x(0,1,0)=(0,0,1) - righthanded coord system

	// orthonormal system
	vRotMat = float3x3(xDir, groundNormal, zDir);

	//vTranformedPos[0] is handled by midPoint manipulations above

	// top cap (BL, TL, TR, BR)
	vTranformedPos[1].xyz = vRotMat * (float3(posB.z, 0.0, posB.w) - float3(midPoint.x, 0.0, midPoint.z)) + midPoint.xyz;
	vTranformedPos[2].xyz = vRotMat * (float3(posT.x, 0.0, posT.y) - float3(midPoint.x, 0.0, midPoint.z)) + midPoint.xyz;
	vTranformedPos[3].xyz = vRotMat * (float3(posT.z, 0.0, posT.w) - float3(midPoint.x, 0.0, midPoint.z)) + midPoint.xyz;
	vTranformedPos[4].xyz = vRotMat * (float3(posB.x, 0.0, posB.y) - float3(midPoint.x, 0.0, midPoint.z)) + midPoint.xyz;

	// distances
	vTranformedPos[1].w = distance(posBL, posBR) * 0.5;
	vTranformedPos[2].w = distance(posTL, posBL) * 0.5;
	vTranformedPos[3].w = distance(posTR, posTL) * 0.5;
	vTranformedPos[4].w = distance(posBR, posTR) * 0.5;

	// distance from the center (only relevant for the explosion cube)
	midPoint.w = sqrt(vTranformedPos[1].w * vTranformedPos[1].w + vTranformedPos[3].w * vTranformedPos[2].w + height * height);


	vuvMain = uvMain;
	vuvNorm = uvNorm;

	float4 testResults = float4(
		float(all(float2(-1.0,  1.0) == relPos.xz)),
		float(all(float2(-1.0, -1.0) == relPos.xz)),
		float(all(float2( 1.0, -1.0) == relPos.xz)),
		float(all(float2( 1.0,  1.0) == relPos.xz))
	);

	float3 worldPos = float3(0);
	worldPos.xyz += testResults.x * vTranformedPos[1].xyz;
	worldPos.xyz += testResults.y * vTranformedPos[2].xyz;
	worldPos.xyz += testResults.z * vTranformedPos[3].xyz;
	worldPos.xyz += testResults.w * vTranformedPos[4].xyz;

	vTintColor = texTint;
	vGlowColor = mix(glowTintMin, glowTintMax, vAlpha * cmAlphaMult);

	// emulate explosion fade in for the first 6 frames, asjusted by the initial alpha (less fadein for already weak scars)
	vAlpha *= mix(1.0, smoothstep(0.0, 6.0 * alpha, curAdjustedFrame - thisVertexCreateFrame), float(vDecalType == DECAL_EXPLOSION));
	
	// vDotElimExp
	vDotElimExp = dotElimExp;

	// effect's height
	vHeight = height;

	// store uvWrapDistance
	vUVWrapDist = uvWrapDistance;

	// store traveled distance
	vUVOffset = uvTraveledDistance;

	worldPos += relPos.y * height * groundNormal;

	gl_Position = gl_ModelViewProjectionMatrix * float4(worldPos, 1.0);
}

};
#undef posTL
#undef posTR
#undef posBR
#undef posBL
#undef refHeight
#undef minHeight
#undef maxHeight
#undef forceHeightMode
#undef alpha
#undef alphaFalloff
#undef glow
#undef glowFalloff
#undef rot
#undef height
#undef dotElimExp
#undef cmAlphaMult
#undef createFrameMin
#undef createFrameMax
#undef uvWrapDistance
#undef uvTraveledDistance
#undef forcedNormal
#undef visMult
#undef decalType
#undef decalId
#undef texTint
#undef glowTintMin
#undef glowTintMax
#undef midPoint
#undef vAlpha
#undef vGlow
#undef vDotElimExp
#undef vHeight
#undef vUVWrapDist
#undef vUVOffset
#undef vDecalType
#undef vTintColor
#undef vGlowColor
#undef NORM2SNORM
#undef SNORM2NORM
#undef px
#undef py
#undef pz
#undef yx
#undef yy
#undef yz
#undef nx
#undef ny
#undef nz
vertex DecalVarying GroundDecalVertex(uint vid [[vertex_id]],uint iid [[instance_id]],device const DecalInstance* instances [[buffer(0)]],constant DecalUniforms& u [[buffer(1)]],texture2d<float> heightTex [[texture(0)]],texture2d<float> normalTex [[texture(1)]],sampler heightSampler [[sampler(0)]],sampler normalSampler [[sampler(1)]]) {
 const auto d=instances[iid];DecalVertexProgram p;
 p.heightTex=heightTex;p.groundNormalTex=normalTex;p.heightSampler=heightSampler;p.normalSampler=normalSampler;p.mapDims=u.mapDims;p.curAdjustedFrame=u.frameScreen.x;p.flips=u.flags.w;
 p.forcedHeight=d.forcedHeight;p.posT=d.posT;p.posB=d.posB;p.uvMain=d.uvMain;p.uvNorm=d.uvNorm;p.createParams1=d.createParams1;p.createParams2=d.createParams2;p.createParams3=d.createParams3;p.createParams4=d.createParams4;p.createParams5=d.createParams5;
 p.gl_ModelViewProjectionMatrix=u.viewProjection;
 // GL uses the last provoking vertex; Metal uses the first. Cyclic rotation
 // preserves each original triangle and its orientation, including the
 // flat glow color which varies with the original track creation endpoint.
 p.Run((vid/3)*3+(vid%3+2)%3);
 DecalVarying o;o.position=p.gl_Position;if(!u.flags.x)o.position.z=(o.position.z+o.position.w)*.5f;
 o.p0=p.vTranformedPos[0];o.p1=p.vTranformedPos[1];o.p2=p.vTranformedPos[2];o.p3=p.vTranformedPos[3];o.p4=p.vTranformedPos[4];o.uvMain=p.vuvMain;o.uvNorm=p.vuvNorm;o.data1=p.vData1;o.data2=p.vData2;o.data3=p.vData3;o.data4=p.vData4;o.r0=p.vRotMat[0];o.r1=p.vRotMat[1];o.r2=p.vRotMat[2];return o;
}
struct DecalFragmentProgram {
#ifdef HIGH_QUALITY
depth2d_ms<float> depthTex;
#else
depth2d<float> depthTex;
#endif
texture2d<float> groundNormalTex,miniMapTex;texture2d_array<float> atlasTex;
sampler normalSampler,minimapSampler,atlasSampler;
#if NATIVE_GROUND_DECAL_SHADOWS
texture2d<float> shadowColorTex;depth2d<float> shadowTex;
sampler shadowColorSampler,shadowSampler;
#endif
uint4 flags;float4 mapDims,mapDimsPO2,groundAmbientColor;float3 groundDiffuseColor,waterMinColor,waterBaseColor,waterAbsorbColor,sunDir;float2 screenSizeInverse;float4x4 shadowMatrix,gl_ModelViewProjectionMatrixInverse;float curAdjustedFrame;float4 gl_FragCoord;uint gl_SampleID;
float4 vTranformedPos[5],vuvMain,vuvNorm,vData1,vData2,vData3,vData4;float3x3 vRotMat;float4 fragColor;
float2 ImageUV(float2 uv,uint bit){return float2(uv.x,(flags.w&(1u<<bit))?1-uv.y:uv.y);}
uint2 DepthPixel(){uint2 p=uint2(gl_FragCoord.xy);if(flags.w&(1u<<5))p.y=depthTex.get_height()-1-p.y;return p;}
#define uvMainTL vuvMain.xy
#define uvMainTR vuvMain.zy
#define uvMainBR vuvMain.zw
#define uvMainBL vuvMain.xw

#define uvNormTL vuvNorm.xy
#define uvNormTR vuvNorm.zy
#define uvNormBR vuvNorm.zw
#define uvNormBL vuvNorm.xw

/////////////////////////////////////////////////////////

#define midPoint          vTranformedPos[0]

#define vAlpha            vData1.x
#define vGlow             vData1.y
#define vDotElimExp       vData1.z
// vData1.w - empty
#define vHeight           vData2.x
#define vUVWrapDist       vData2.y
#define vUVOffset         vData2.z
#define vDecalType        vData2.w

#define vTintColor        vData3
#define vGlowColor        vData4

/////////////////////////////////////////////////////////

#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)

#line 200084

float3 GetTriangleBarycentric(float3 p, float3 p0, float3 p1, float3 p2) {
    float3 v0 = p2 - p0;
    float3 v1 = p1 - p0;
    float3 v2 = p - p0;

    float dot00 = dot(v0, v0);
    float dot01 = dot(v0, v1);
    float dot02 = dot(v0, v2);
    float dot11 = dot(v1, v1);
    float dot12 = dot(v1, v2);

    float invDenom = 1.0 / (dot00 * dot11 - dot01 * dot01);

    float s = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float t = (dot00 * dot12 - dot01 * dot02) * invDenom;
    float q = 1.0 - s - t;
    return float3(s, t, q);
}

// https://www.shadertoy.com/view/MslSDl
float3 BlackBody(float t)
{
	float u = (0.860117757 + 1.54118254e-4 * t + 1.28641212e-7 * t * t)
		/ (1.0 + 8.42420235e-4 * t + 7.08145163e-7 * t * t);

	float v = (0.317398726 + 4.22806245e-5 * t + 4.20481691e-8 * t * t)
		/ (1.0 - 2.89741816e-5 * t + 1.61456053e-7 * t * t);

	float x = 3.0 * u / (2.0 * u - 8.0 * v + 4.0);
	float y = 2.0 * v / (2.0 * u - 8.0 * v + 4.0);
	float z = 1.0 - x - y;

	float Y = 1.0;
	float X = (Y/y) * x;
	float Z = (Y/y) * z;

	float3x3 XYZtosRGB = float3x3(float3(3.2404542,-1.5371385,-0.4985314),float3(-0.9692660,1.8760108,0.0415560),float3(0.0556434,-0.2040259,1.0572252));

	float3 RGB = float3(X,Y,Z) * XYZtosRGB;
	return RGB * pow(0.0004 * t, 4.0);
}

float3 GetFragmentNormal(float2 wxz) {
	float3 normal;
	normal.xz = groundNormalTex.sample(normalSampler, ImageUV(wxz * mapDims.zw,1), level(0)).rg;
	normal.y  = sqrt(1.0 - dot(normal.xz, normal.xz));
	return normal;
}

// Shadow mapping functions (HighQ)

//const float goldenAngle = PI * (3.0 - sqrt(5.0));
const float goldenAngle = 2.3999632297286533222315555066336;
//const float PI = acos(0.0) * 2.0;
const float PI = 3.1415926535897932384626433832795;

// http://blog.marmakoide.org/?p=1
float2 SpiralSNorm(int i, int N) {
	float theta = float(i) * goldenAngle;
	float r = sqrt(float(i)) / sqrt(float(N));
	return float2 (r * cos(theta), r * sin(theta));
}

float hash12L(float2 p) {
	const float HASHSCALE1 = 0.1031;
	float3 p3  = fract(float3(p.xyx) * HASHSCALE1);
	p3 += dot(p3, p3.yzx + 19.19);
	return fract((p3.x + p3.y) * p3.z);
}
// Derivatives of light-space depth with respect to texture2D coordinates
float2 DepthGradient(float3 xyz) {
	float2 dZduv = float2(0.0, 0.0);

	float3 dUVZdx = dfdx(xyz);
	float3 dUVZdy = dfdy(xyz);

	dZduv.x  = dUVZdy.y * dUVZdx.z;
	dZduv.x -= dUVZdx.y * dUVZdy.z;

	dZduv.y  = dUVZdx.x * dUVZdy.z;
	dZduv.y -= dUVZdy.x * dUVZdx.z;

	float det = (dUVZdx.x * dUVZdy.y) - (dUVZdx.y * dUVZdy.x);

	return dZduv / det;
}

float BiasedZ(float z0, float2 dZduv, float2 offset) {
	//return z0 + dot(dZduv, offset);
	return z0;
}

float3 GetShadowColor(float3 worldPos, float NdotL) {
#if NATIVE_GROUND_DECAL_SHADOWS
if(flags.y) {
	float4 shadowPos = shadowMatrix * float4(worldPos, 1.0);
	shadowPos.xy += float2(0.5);
	shadowPos /= shadowPos.w;

	float3 shadowColor = shadowColorTex.sample(shadowColorSampler, ImageUV(shadowPos.xy,4)).rgb;
	#ifndef HIGH_QUALITY
		float shadowFactor = shadowTex.sample_compare(shadowSampler, ImageUV(shadowPos.xy,3), shadowPos.z);
	#else
		const int shadowSamples = 3;
		const float samplingRandomness = 0.4;
		const float samplingDistance = 1.0;

		float2 dZduv = DepthGradient(shadowPos.xyz);

		float rndRotAngle = NORM2SNORM(hash12L(gl_FragCoord.xy)) * PI / 2.0 * samplingRandomness;

		float2 vSinCos = float2(sin(rndRotAngle), cos(rndRotAngle));
		float2x2 rotMat2d = float2x2(float2(vSinCos.y,-vSinCos.x),float2(vSinCos.x,vSinCos.y));

		float2 filterSize = float2(samplingDistance / float2(shadowTex.get_width(),shadowTex.get_height()));

		float shadowFactor = 0.0;
		for (int i = 0; i < shadowSamples; ++i) {
			// SpiralSNorm return low discrepancy sampling float2
			float2 offset = (rotMat2d * SpiralSNorm( i, shadowSamples )) * filterSize;

			float3 shadowSamplingCoord = float3(shadowPos.xy, 0.0) + float3(offset, BiasedZ(shadowPos.z, dZduv, offset));
			shadowSamplingCoord.xy += offset;

			shadowFactor += shadowTex.sample_compare(shadowSampler, ImageUV(shadowSamplingCoord.xy,3), shadowSamplingCoord.z);
		}
		shadowFactor /= float(shadowSamples);
	#endif
	shadowFactor = min(shadowFactor, smoothstep(0.0, 0.35, NdotL));

	return mix(float3(1.0), shadowFactor * shadowColor, groundAmbientColor.w);
}
#endif
return float3(1.0);
}


// Calculate out of the fragment in screen space the view space position.
float3 GetWorldPos(float2 texCoord, float sampledDepth) {
	float4 projPosition = float4(0.0, 0.0, 0.0, 1.0);

	//texture space [0;1] to NDC space [-1;1]
	if(flags.x) {
		//don't transform depth as it's in the same [0;1] space
		projPosition.xyz = float3(NORM2SNORM(texCoord), sampledDepth);
	} else {
		projPosition.xyz = NORM2SNORM(float3(texCoord, sampledDepth));
	}

	float4 pos = gl_ModelViewProjectionMatrixInverse * projPosition;

	return pos.xyz / pos.w;
}

float3 GetFragmentWorldPos() {
	#ifdef HIGH_QUALITY
		float depthZO = depthTex.read(DepthPixel(), gl_SampleID);
	#else
		float depthZO = depthTex.read(DepthPixel());
	#endif

	return GetWorldPos(gl_FragCoord.xy * screenSizeInverse, depthZO);
}

float3 RotateByNormalVector(float3 p, float3 newUpDir, float3 rotAxis) {
	#define px (p.x)
	#define py (p.y)
	#define pz (p.z)

	#define yx (newUpDir.x)
	#define yy (newUpDir.y)
	#define yz (newUpDir.z)

	#define nx (rotAxis.x)
	#define ny (rotAxis.y)
	#define nz (rotAxis.z)

	float3 cm = float3(
		yy,
		sqrt(yx * yx + yz * yz),
		(nx * px + ny * py + nz * pz)* (1.0 - yy)
	);

	return float3(
		dot(cm, float3(px, (ny * pz - nz * py), nx)),
		dot(cm, float3(py, (nz * px - nx * pz), ny)),
		dot(cm, float3(pz, (nx * py - ny * px), nz))
	);

	#undef px
	#undef py
	#undef pz

	#undef yx
	#undef yy
	#undef yz

	#undef nx
	#undef ny
	#undef nz
}

const float3 all0 = float3(0.0);
const float3 all1 = float3(1.0);
// returns if the fragment is within the quadrilateral
bool ProjectOntoPlane(float3 worldPos, float3 BL, float3 TL, float3 TR, float3 BR, float3 projPlane, float u, thread float4& relUV) {
	float3 worldPosProj = worldPos - dot(worldPos - midPoint.xyz, projPlane) * projPlane;
	{
		float3 bc = GetTriangleBarycentric(worldPosProj, BL, TL, TR);
		if (all(bc>=all0) && all(bc<=all1)) {
			relUV = bc.x * float4(0, u, 0, 1) + bc.y * float4(0, 0, 0, 0) + bc.z * float4(1, 0, 1, 0);
			return true;
		}
	}
	{
		float3 bc = GetTriangleBarycentric(worldPosProj, TR, BR, BL);
		if (all(bc>=all0) && all(bc<=all1)) {
			relUV = bc.x * float4(1, 0, 1, 0) + bc.y * float4(1, u, 1, 1) + bc.z * float4(0, u, 0, 1);
			return true;
		}
	}
	return false;
}


float4 GetColorByRelUV(float2 uvTL, float2 uvBL, float2 uvTR, float2 uvBR, float4 relUV) {
	float layer = floor(uvTL.x);
	uvTL.x = fract(uvTL.x);
	uvBL.x = fract(uvBL.x);

	float2 uv = mix(
		mix(uvTL, uvBL, relUV.x),
		mix(uvTR, uvBR, relUV.x),
	relUV.y);

	return atlasTex.sample(atlasSampler, uv, uint(layer));
}



bool IsInEllipsoid(float3 xyz, float3 abc) {
#if 1
	float A = xyz.x * abc.y * abc.z;
	float B = xyz.y * abc.x * abc.z;
	float C = xyz.z * abc.x * abc.y;
	float D = abc.x * abc.y * abc.z;

	return A * A + B * B + C * C <= D * D;
#else
	return ((xyz.x*xyz.x) / (abc.x*abc.x) + (xyz.y*xyz.y) / (abc.y*abc.y) + (xyz.z*xyz.z) / (abc.z*abc.z) <= 1.0);
#endif
}

float EllipsoidRedunction(float3 xyz, float3 abc) {
	float s = (xyz.x*xyz.x) / (abc.x*abc.x) + (xyz.y*xyz.y) / (abc.y*abc.y) + (xyz.z*xyz.z) / (abc.z*abc.z);
	return 2.0 - max(s, 1.0);
}

const float DECAL_EXPLOSION = 2.0;

const float SMF_INTENSITY_MULT = 210.0 / 255.0;
const float SMF_SHALLOW_WATER_DEPTH     = 10.0;
const float SMF_SHALLOW_WATER_DEPTH_INV = 1.0 / SMF_SHALLOW_WATER_DEPTH;

const float EPS = 3e-3;

void Run() {
	float3 worldPos = GetFragmentWorldPos();

	float4 uvBL = float4(uvMainBL, uvNormBL);
	float4 uvTL = float4(uvMainTL, uvNormTL);
	float4 uvTR = float4(uvMainTR, uvNormTR);
	float4 uvBR = float4(uvMainBR, uvNormBR);

	float u = 1.0;
	if (vUVWrapDist > 0.0) {
		u = distance((vTranformedPos[1].xyz + vTranformedPos[2].xyz) * 0.5, (vTranformedPos[3].xyz + vTranformedPos[4].xyz) * 0.5) / vUVWrapDist;
	}

	float4 relUV;
	if (!ProjectOntoPlane(worldPos, vTranformedPos[1].xyz, vTranformedPos[2].xyz, vTranformedPos[3].xyz, vTranformedPos[4].xyz, vRotMat[1], u, relUV)) {
		fragColor = float4(0.0);
		return;
	}

	if (vUVWrapDist > 0.0) {
		relUV.y = fract(vUVOffset / vUVWrapDist + relUV.y);
	}

	float alpha = clamp(vAlpha, 0.0, 1.0);
	float glow  = clamp(vGlow , 0.0, 1.0);

	float4 mainCol = GetColorByRelUV(uvTL.xy, uvBL.xy, uvTR.xy, uvBR.xy, relUV);
	float4 normVal = GetColorByRelUV(uvTL.zw, uvBL.zw, uvTR.zw, uvBR.zw, relUV);

	mainCol *= 2.0 * vTintColor;

	float3 mapDiffuse = miniMapTex.sample(minimapSampler, ImageUV(worldPos.xz * mapDims.zw,2), level(0)).rgb;
	#if 0
		const float3 LUMA = float3(0.2125, 0.7154, 0.0721);
		float3 mapDecalMix = mix(mainCol.rgb, mapDiffuse.rgb, smoothstep(0.0, 0.6, dot(mainCol.rgb, LUMA)));
	#else
		float3 mapDecalMix = 2.0 * mainCol.rgb * mapDiffuse.rgb;
	#endif
	mainCol.rgb = mix(mainCol.rgb, mapDecalMix, float(vDecalType == DECAL_EXPLOSION)); //only apply mapDecalMix for explosions

	float3 N = GetFragmentNormal(worldPos.xz);
	float3 T = normalize(vRotMat[0] - N * dot(vRotMat[0],  N));
	float3 B = normalize(cross(N, T)); // ex. (0,0,1)x(1,0,0)=(0,1,0) - righthanded coord system

	float3x3 TBN = float3x3(T, B, N);

	float3 decalNormal = normalize(TBN * NORM2SNORM(normVal.xyz));
	float NdotL = max(dot(sunDir, decalNormal), 0.0);
	float3 diffuseTerm = NdotL * groundDiffuseColor;

	float3 lightCol = diffuseTerm * GetShadowColor(worldPos.xyz, dot(sunDir, N)) + groundAmbientColor.rgb;

	fragColor.rgb = mainCol.rgb * lightCol;
	
	if (vGlowColor.a == 0.0) {
		// overglow
		glow += smoothstep(0.75, 1.0, glow) * 0.2 * abs(sin(0.02 * curAdjustedFrame));

		// distance based glow adjustment
		float relDistance = distance(worldPos.xyz, midPoint.xyz) / midPoint.w;

		relDistance = smoothstep(0.9, 0.1, relDistance);
		glow *= pow(relDistance, 7.0); // artistic choice to keep the glow centered
		glow *= smoothstep(-SMF_SHALLOW_WATER_DEPTH, 0.0, worldPos.y);

		float t = mix(1.0, 3700.0, glow);
		fragColor.rgb += BlackBody(normVal.w * t) * glow;
	} else {
		// alpha mixed
		fragColor.rgb  = mix(fragColor.rgb, vGlowColor.rgb * normVal.w * glow, vGlowColor.a);
		// alpha added
		//fragColor.rgb += vGlowColor.rgb * normVal.w * glow * vGlowColor.a;
	}

	// adaptation from SMFFragProg.glsl
	if (flags.z && worldPos.y <= 0.0 && vDecalType == DECAL_EXPLOSION) {
		float3 waterShadeInt = waterBaseColor;

		float waterShadeAlpha  = -worldPos.y * SMF_SHALLOW_WATER_DEPTH_INV;
		float waterShadeDecay  = 0.2 + (waterShadeAlpha * 0.1);
		float vertexStepHeight = min(1023.0, -worldPos.y);
		float waterLightInt    = min(NdotL * 2.0 + 0.4, 1.0);

		// vertex below shallow water depth --> alpha=1
		// vertex above shallow water depth --> alpha=waterShadeAlpha
		waterShadeAlpha = min(1.0, waterShadeAlpha + float(worldPos.y <= -SMF_SHALLOW_WATER_DEPTH));

		waterShadeInt -= (waterAbsorbColor * vertexStepHeight);
		waterShadeInt  = max(waterMinColor, waterShadeInt);
		waterShadeInt *= float3(SMF_INTENSITY_MULT * waterLightInt);

		// make shadowed areas darker over deeper water
		waterShadeInt *= (1.0 - waterShadeDecay);

		// if depth is greater than _SHALLOW_ depth, select waterShadeInt
		// otherwise interpolate between groundShadeInt and waterShadeInt
		// (both are already cosine-weighted)
		fragColor.rgb = mix(fragColor.rgb, fragColor.rgb * waterShadeInt, waterShadeAlpha);
	}

	fragColor.a = mainCol.a * alpha;

	float3 rotWorldPos = transpose(vRotMat) * (worldPos - midPoint.xyz);
	//fragColor.xyz=float3(1);
	float3 ellipseAxes = float3(vTranformedPos[1].w, vHeight, vTranformedPos[2].w);
	fragColor.a *= mix(1.0, EllipsoidRedunction(rotWorldPos, ellipseAxes), float(vDecalType == DECAL_EXPLOSION));
	// artistic adjustments

	fragColor  *= mix(1.0, pow(max(dot(vRotMat[1], N), 0.0), vDotElimExp), float(vDotElimExp > 0.0));

	fragColor.a *=
		smoothstep(0.0, EPS, relUV.x) * (1.0 - smoothstep(1.0 - EPS, 1.0, relUV.x)) *
		smoothstep(0.0, EPS, relUV.y) * (1.0 - smoothstep(1.0 - EPS, 1.0, relUV.y));
}

};
#undef uvMainTL
#undef uvMainTR
#undef uvMainBR
#undef uvMainBL
#undef uvNormTL
#undef uvNormTR
#undef uvNormBR
#undef uvNormBL
#undef midPoint
#undef vAlpha
#undef vGlow
#undef vDotElimExp
#undef vHeight
#undef vUVWrapDist
#undef vUVOffset
#undef vDecalType
#undef vTintColor
#undef vGlowColor
#undef NORM2SNORM
#undef SNORM2NORM
#undef px
#undef py
#undef pz
#undef yx
#undef yy
#undef yz
#undef nx
#undef ny
#undef nz
fragment float4 GroundDecalFragment(DecalVarying i [[stage_in]],constant DecalUniforms& u [[buffer(1)]],
texture2d<float> normalTex [[texture(1)]],texture2d<float> minimap [[texture(2)]],
#if NATIVE_GROUND_DECAL_SHADOWS
depth2d<float> shadowDepth [[texture(3)]],texture2d<float> shadowColor [[texture(4)]],
#endif
#ifdef HIGH_QUALITY
depth2d_ms<float> terrainDepth [[texture(5)]],uint sampleID [[sample_id]],
#else
depth2d<float> terrainDepth [[texture(5)]] RECOIL_SAMPLE_ID_ARGUMENT,
#endif
texture2d_array<float> atlas [[texture(6)]],sampler normalSampler [[sampler(1)]],sampler minimapSampler [[sampler(2)]],
#if NATIVE_GROUND_DECAL_SHADOWS
sampler shadowSampler [[sampler(3)]],sampler shadowColorSampler [[sampler(4)]],
#endif
sampler atlasSampler [[sampler(5)]] RECOIL_SAMPLE_POSITION_ARGUMENT) {
#ifdef HIGH_QUALITY
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(sampleID));
#else
	RECOIL_SAMPLE_FIX_POSITION(i.position,get_sample_position(recoilOriginalSampleID));
#endif
DecalFragmentProgram p;p.depthTex=terrainDepth;p.groundNormalTex=normalTex;p.miniMapTex=minimap;p.atlasTex=atlas;
p.normalSampler=normalSampler;p.minimapSampler=minimapSampler;p.atlasSampler=atlasSampler;
#if NATIVE_GROUND_DECAL_SHADOWS
p.shadowTex=shadowDepth;p.shadowColorTex=shadowColor;p.shadowSampler=shadowSampler;p.shadowColorSampler=shadowColorSampler;
#endif
 p.flags=u.flags;p.mapDims=u.mapDims;p.mapDimsPO2=u.mapDimsPO2;p.groundAmbientColor=u.groundAmbient;p.groundDiffuseColor=u.groundDiffuse.xyz;p.waterMinColor=u.waterMin.xyz;p.waterBaseColor=u.waterBase.xyz;p.waterAbsorbColor=u.waterAbsorb.xyz;p.sunDir=u.sunDirection.xyz;p.screenSizeInverse=u.frameScreen.yz;p.curAdjustedFrame=u.frameScreen.x;p.shadowMatrix=u.shadow;p.gl_ModelViewProjectionMatrixInverse=u.inverseViewProjection;
 p.gl_FragCoord=float4(i.position.xy*u.fragmentToLogical.xy+u.fragmentToLogical.zw,i.position.zw);
#ifdef HIGH_QUALITY
 p.gl_SampleID=sampleID;
#else
 p.gl_SampleID=0;
#endif
 p.vTranformedPos[0]=i.p0;p.vTranformedPos[1]=i.p1;p.vTranformedPos[2]=i.p2;p.vTranformedPos[3]=i.p3;p.vTranformedPos[4]=i.p4;p.vuvMain=i.uvMain;p.vuvNorm=i.uvNorm;p.vData1=i.data1;p.vData2=i.data2;p.vData3=i.data3;p.vData4=i.data4;p.vRotMat=float3x3(i.r0,i.r1,i.r2);p.Run();return p.fragColor;
}

struct DecalAtlasVertex {float2 position,uv;};
struct DecalAtlasUniforms {float4 sourceClamp;uint4 flags;};
struct DecalAtlasVarying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex DecalAtlasVarying GroundDecalAtlasVertex(uint vid [[vertex_id]],device const DecalAtlasVertex* v [[buffer(0)]]) {
 DecalAtlasVarying o;o.position=float4(v[vid].position.x,-v[vid].position.y,0,1);o.uv=v[vid].uv;return o;
}
fragment float4 GroundDecalAtlasFragment(DecalAtlasVarying v [[stage_in]],constant DecalAtlasUniforms& u [[buffer(1)]],texture2d<float> tex [[texture(0)]],sampler linearClamp [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
 float2 uv=clamp(v.uv,u.sourceClamp.xy,u.sourceClamp.zw);if(u.flags.x)uv.y=1-uv.y;
 return tex.sample(linearClamp,uv,level(0));
}
)MSL";
inline std::string NativeGroundDecalShaderSource(bool high,bool shadows)
{
	return std::string(high?"#define HIGH_QUALITY\n#ifndef RECOIL_NATIVE_SAMPLE_SHADING\n#define RECOIL_NATIVE_SAMPLE_SHADING 1\n#endif\n":"")+
		"#define NATIVE_GROUND_DECAL_SHADOWS "+std::to_string(unsigned(shadows))+"\n"+NativeGroundDecalShaders;
}
}
