/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace rendering::outlineProgram {
inline constexpr std::uint64_t VertexHash=0xba688a1cc1c4a789ULL;
inline constexpr std::string_view VertexSHA256="bc03c23f30acb8a15997193f50306e68191730dfd8f71787f2cbaccfcf1e659d";
inline constexpr std::uint64_t FragmentHash=0x87133872e083df42ULL;
inline constexpr std::string_view FragmentSHA256="ac77ba443f115d2c1458c3390f125561132cd6d582044e7f2bd44c1a22dfc94d";
inline constexpr std::uint64_t WidgetHash=0xd5b27f2629ca3998ULL;
inline constexpr std::string_view WidgetSHA256="36568db27c0a4a5ed742b19b1085373f0985acc9277d58a253b5fa5592ade966";
inline constexpr std::uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr std::string_view ShaderHelperSHA256="61e139efaf762f3940b6528c361785cacdfa9059cafe3fabef456d74f0128b23";
inline constexpr char VertexSource[]=R"OUTORIG(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_shading_language_420pack : require

#line 5000

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__
//__QUATERNIONDEFS__

layout (location = 0) in vec2 corner;
layout (location = 1) in vec4 bounds; // model space bounding sphere: center, radius
#if PER_UNIT == 1
	layout (location = 2) in vec4 color;
	layout (location = 3) in vec4 shape; // width and blur in pixels at 1080p, 1 = scale with zoom, unitID + 1
	layout (location = 4) in vec4 timing; // shown since, hidden since (-1 = shown) in draw seconds, fade seconds, pulse seconds
	layout (location = 5) in uvec4 instData;
#else
	layout (location = 2) in uvec4 instData;
#endif

struct SUniformsBuffer {
	uint composite; // u8 drawFlag; u8 unused1; u16 id;

	uint unused2;
	uint unused3;
	uint unused4;

	float maxHealth;
	float health;
	float unused5;
	float unused6;

	vec4 speed;
	vec4[5] userDefined; // can't use float[20] because float in arrays occupies 4 * float space
};

layout(std140, binding = 1) readonly buffer UniformsBuffer {
	SUniformsBuffer uni[];
};

#if USEQUATERNIONS == 0
	layout(std140, binding = 0) readonly buffer MatrixBuffer {
		mat4 UnitPieces[];
	};
#endif

#line 10000

#if PER_UNIT == 1
	uniform float zoomScale;
	uniform float resolutionScale; // screen height / 1080

	out DataVS {
		flat vec4 v_color; // alpha includes fade and pulse
		flat float v_width; // pixels
		flat float v_blur; // pixels
		flat uint v_id; // unitID + 1
	};
#else
	uniform float quadPadding; // pixels added on each side
#endif

void main()
{
	#if USEQUATERNIONS == 0
		mat4 modelMatrix = UnitPieces[instData.x];
	#else
		mat4 modelMatrix = TransformToMatrix(GetModelWorldTransform(instData.x));
	#endif

	#if PER_UNIT == 1
		float sizeScale = resolutionScale * mix(1.0, zoomScale, shape.z);
		float width = clamp(shape.x * sizeScale, 0.0, float(WIDTH_MAX));
		float blur = clamp(shape.y * sizeScale, 0.0, float(WIDTH_MAX));
		float alpha = color.a;
		if (timing.z > 0.0) {
			alpha *= clamp((timeInfo.y - timing.x) / timing.z, 0.0, 1.0);
			if (timing.y >= 0.0) {
				alpha *= 1.0 - clamp((timeInfo.y - timing.y) / timing.z, 0.0, 1.0);
			}
		}
		if (timing.w > 0.0) {
			alpha *= 0.6 + 0.4 * cos(timeInfo.y * 6.2831853 / timing.w);
		}
		v_color = vec4(color.rgb, alpha);
		v_width = width;
		v_blur = blur;
		v_id = uint(shape.w + 0.5);
		float padding = ceil(width + blur + 1.0) + 1.0; // the fragment shader's reach, plus one
	#else
		float alpha = 1.0;
		float padding = quadPadding;
	#endif

	// drawFlag bits 0-1: drawn as a model, not as an icon. A zero position means the unit entered LOS but has
	// not been drawn yet, so its matrix is not uploaded.
	if (alpha <= 0.0 || (uni[instData.y].composite & 3u) == 0u || dot(modelMatrix[3].xyz, modelMatrix[3].xyz) < 1.0) {
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // all four corners here: a degenerate quad outside the view
		return;
	}

	vec3 center = (modelMatrix * vec4(bounds.xyz, 1.0)).xyz;
	vec3 worldPos = center + (cameraViewInv[0].xyz * corner.x + cameraViewInv[1].xyz * corner.y) * bounds.w;
	gl_Position = cameraViewProj * vec4(worldPos, 1.0);
	gl_Position.xy += corner * (2.0 * padding / viewGeometry.xy) * gl_Position.w;
}
)OUTORIG";
inline constexpr char FragmentSource[]=R"OUTORIG(#version 420
#extension GL_ARB_uniform_buffer_object : require
#extension GL_ARB_shading_language_420pack : require

#line 20000

//__ENGINEUNIFORMBUFFERDEFS__
//__DEFINES__

uniform sampler2D mapDepths;
uniform sampler2D modelDepths;
uniform sampler2D modelMisc; // units: green = 1, unitID + 1 in red (low byte) and blue (high byte)

#if PER_UNIT == 1
	in DataVS {
		flat vec4 v_color;
		flat float v_width;
		flat float v_blur;
		flat uint v_id;
	};
	#define OUTLINE_WIDTH v_width
#else
	uniform float outlineWidth;
	#define OUTLINE_WIDTH outlineWidth
#endif

out vec4 fragColor;

const vec2 NEIGHBOURS[4] = vec2[4](vec2(-1.0, 0.0), vec2(1.0, 0.0), vec2(0.0, -1.0), vec2(0.0, 1.0));

bvec4 and4(bvec4 a, bvec4 b) {
	return bvec4(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w);
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
		return clamp(width - 1.0, 0.0, 1.0);
	}

	float OuterAlpha(float dist, float width, float reach) {
		float t = clamp((reach - dist) / (v_blur + 1.0), 0.0, 1.0);
		return v_blur > 0.0 ? t * t * (3.0 - 2.0 * t) : t;
	}

	bool IsMember(vec4 misc) {
		return all(equal(uvec2(misc.rb * 255.0 + 0.5), uvec2(v_id & 255u, v_id >> 8u)));
	}

	bvec4 GatherMembers(vec2 uv) {
		bvec4 members = equal(uvec4(textureGather(modelMisc, uv, 0) * 255.0 + 0.5), uvec4(v_id & 255u));
		if (any(members)) {
			members = and4(members, equal(uvec4(textureGather(modelMisc, uv, 2) * 255.0 + 0.5), uvec4(v_id >> 8u)));
		}
		return members;
	}
#else
	// Zero-K's falloff
	float Reach(float width) {
		return width * OUTLINE_REACH;
	}

	int SearchRadius(float width, float reach) {
		return min(int(ceil(reach)), int(ceil(width)) + 3);
	}

	float EdgeAlpha(float width) {
		return 1.0 - 1.5 / width;
	}

	float OuterAlpha(float dist, float width, float reach) {
		return 1.0 - pow(dist / reach, float(1 + int(width / 2.0)));
	}

	bool IsMember(vec4 misc) {
		return misc.g > 0.5;
	}

	bvec4 GatherMembers(vec2 uv) {
		return greaterThan(textureGather(modelMisc, uv, 1), vec4(0.5));
	}
#endif

// A member pixel counts as visible where the unit is in front of the terrain.
void main(void)
{
	float width = OUTLINE_WIDTH;
	vec2 texelSize = 1.0 / viewGeometry.xy;
	vec2 texel = floor(gl_FragCoord.xy - viewGeometry.zw);
	ivec2 itexel = ivec2(texel);

	float alpha;
	if (IsMember(texelFetch(modelMisc, itexel, 0)) && texelFetch(mapDepths, itexel, 0).x > texelFetch(modelDepths, itexel, 0).x) {
		// Visible member pixel: outline it where a neighbouring member pixel is behind the terrain
		float edge = 0.0;
		for (int i = 0; i < 4; i++) {
			vec2 uv = (texel + 0.5 + NEIGHBOURS[i]) * texelSize;
			if (IsMember(textureLod(modelMisc, uv, 0.0)) && textureLod(mapDepths, uv, 0.0).x <= textureLod(modelDepths, uv, 0.0).x) {
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
				vec4 dx = vec4(x, x + 1, x + 1, x);
				vec4 dy = vec4(y + 1, y + 1, y, y);
				vec4 distSq = mix(dx * dx + dy * dy, vec4(1e9), greaterThan(max(dx, dy), vec4(radius)));
				if (!any(lessThan(distSq, vec4(reachSq)))) continue;

				vec2 uv = (texel + vec2(x, y) + 1.0) * texelSize;
				bvec4 isMember = GatherMembers(uv);
				if (!any(isMember)) continue;

				bvec4 inFront = greaterThan(textureGather(mapDepths, uv, 0), textureGather(modelDepths, uv, 0));
				vec4 hitSq = mix(vec4(nearestSq), distSq, and4(isMember, inFront));
				nearestSq = min(nearestSq, min(min(hitSq.x, hitSq.y), min(hitSq.z, hitSq.w)));
			}
		}
		// Hits at or beyond reach give alpha <= 0, same as no hit
		alpha = OuterAlpha(sqrt(nearestSq), width, reach);
	}

	#if PER_UNIT == 1
		alpha = max(alpha, 0.0) * v_color.a;
		if (alpha <= 0.0) discard;
		fragColor = vec4(v_color.rgb, alpha);
	#else
		fragColor = vec4(0.0, 0.0, 0.0, alpha);
	#endif
}
)OUTORIG";
inline constexpr char EngineDefinitions[]=R"OUTORIG(layout(std140, binding = 0) uniform UniformMatrixBuffer {
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

)OUTORIG";
inline constexpr char QuaternionDefinitions[]=R"OUTORIG(// Quaternion math functions
struct Transform {
	vec4 quat;
	vec4 trSc;
};

layout(std140, binding = 0) readonly buffer TransformBuffer {
	Transform transforms[];
};

uint GetUnpackedValue(uint packedValue, uint byteNum) {
	return (packedValue >> (8u * byteNum)) & 0xFFu;
}

vec4 MultiplyQuat(vec4 a, vec4 b)
{
    return vec4(a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz), a.w * b.w - dot(a.xyz, b.xyz));
}

vec3 RotateByQuaternion(vec4 q, vec3 v) {
	return 2.0 * dot(q.xyz, v) * q.xyz + (q.w * q.w - dot(q.xyz, q.xyz)) * v + 2.0 * q.w * cross(q.xyz, v);
}

vec4 RotateByQuaternion(vec4 q, vec4 v) {
	return vec4(RotateByQuaternion(q, v.xyz), v.w);
}

vec4 InvertNormalizedQuaternion(vec4 q) {
	return vec4(-q.x, -q.y, -q.z, q.w);
}

vec3 ApplyTransform(Transform tra, vec3 v) {
	return RotateByQuaternion(tra.quat, v * tra.trSc.w) + tra.trSc.xyz;
}

vec4 ApplyTransform(Transform tra, vec4 v) {
	return vec4(RotateByQuaternion(tra.quat, v.xyz * tra.trSc.w) + tra.trSc.xyz * v.w, v.w);
}

Transform ApplyTransform(Transform parentTra, Transform childTra) {
	return Transform(
		MultiplyQuat(parentTra.quat, childTra.quat),
		vec4(
			parentTra.trSc.xyz + RotateByQuaternion(parentTra.quat, parentTra.trSc.w * childTra.trSc.xyz),
			parentTra.trSc.w * childTra.trSc.w
		)
	);
}

Transform InvertTransformAffine(Transform tra) {
	vec4 invR = InvertNormalizedQuaternion(tra.quat);
	float invS = 1.0 / tra.trSc.w;
	return Transform(
		invR,
		vec4(
			RotateByQuaternion(invR, -tra.trSc.xyz * invS),
			invS
		)
	);
}

mat4 TransformToMatrix(Transform tra) {
	float qxx = tra.quat.x * tra.quat.x;
	float qyy = tra.quat.y * tra.quat.y;
	float qzz = tra.quat.z * tra.quat.z;
	float qxz = tra.quat.x * tra.quat.z;
	float qxy = tra.quat.x * tra.quat.y;
	float qyz = tra.quat.y * tra.quat.z;
	float qrx = tra.quat.w * tra.quat.x;
	float qry = tra.quat.w * tra.quat.y;
	float qrz = tra.quat.w * tra.quat.z;

	mat3 rot = mat3(
		vec3(1.0 - 2.0 * (qyy + qzz), 2.0 * (qxy + qrz)      , 2.0 * (qxz - qry)      ),
		vec3(2.0 * (qxy - qrz)      , 1.0 - 2.0 * (qxx + qzz), 2.0 * (qyz + qrx)      ),
		vec3(2.0 * (qxz + qry)      , 2.0 * (qyz - qrx)      , 1.0 - 2.0 * (qxx + qyy))
	);

	rot *= tra.trSc.w;

	return mat4(
		vec4(rot[0]      , 0.0),
		vec4(rot[1]      , 0.0),
		vec4(rot[2]      , 0.0),
		vec4(tra.trSc.xyz, 1.0)
	);
}
vec4 SLerp(vec4 qa, vec4 qb, float t) {
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
		return mix(qa, qb, step(0.5, t));
	#endif

	float halfTheta = acos(cosHalfTheta);

	// both should be divided by sinHalfTheta (calculation skipped),
	// but it makes no sense to do it due to follow up normalization
	float ratioA = sin((1.0 - t) * halfTheta);
	float ratioB = sin((      t) * halfTheta);

	return qa * ratioA + qb * ratioB; // already normalized
}

vec4 Lerp(vec4 qa, vec4 qb, float t) {
	// Ensure shortest path
	if (dot(qa, qb) < 0.0)
		qb = -qb;
	return normalize(mix(qa, qb, t)); // GLSL's mix() = (1 - t) * qa + t * qb
}

Transform SLerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = clamp(a, 0.0, 1.0);
	return Transform(
		SLerp(t0.quat, t1.quat, a),
		mix(t0.trSc, t1.trSc, a)
	);
}


Transform Lerp(Transform t0, Transform t1, float a) {
	// generally good idea, otherwise extrapolation artifacts
	// will be nasty in some cases (e.g. fast rotation)
	a = clamp(a, 0.0, 1.0);
	return Transform(
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
		timeInfo.w
	);
}

// This helper function gets the transform that gets you the piece-space to model-space transform
Transform GetPieceModelTransform(uint baseIndex, uint pieceID)
{
	return Lerp(
		transforms[baseIndex + 2u * (1u + pieceID) + 0u],
		transforms[baseIndex + 2u * (1u + pieceID) + 1u],
		timeInfo.w
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

)OUTORIG";
inline constexpr std::array<std::string_view,3> SamplerNames={"mapDepths","modelDepths","modelMisc"};
struct UniformMetadataEntry {std::string_view name;unsigned glType;bool integer;float sourceDefault;int perUnit;};
inline constexpr std::array<UniformMetadataEntry,7> UniformMetadata={{{"outlineWidth",0x1406,false,0,0},{"quadPadding",0x1406,false,0,0},{"zoomScale",0x1406,false,0,1},{"resolutionScale",0x1406,false,0,1},{"mapDepths",0x8b5e,true,0,-1},{"modelDepths",0x8b5e,true,0,-1},{"modelMisc",0x8b5e,true,0,-1}}};
}
