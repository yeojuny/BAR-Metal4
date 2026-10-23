/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <array>
#include <string_view>

namespace rendering::metal4::nano {
// Source-paired30770/31657 vertex/fragment expressions; options.z selects the
// exact31657 spawn-time guard. Only GLSL types,
// interfaces, column constructors and texture/discard intrinsics map to MSL.
// VS SHA256: 0eccaf9fb21a2efb2af4718f296fcb1825be2fa81e1f172b6557a2e1dc928f34
// FS SHA256: 06785f2f06336cf9bb036baef9b9ec776e3523d377d82eff3773e420950ee4bb
inline constexpr std::array<std::string_view,20> PARAMETER_NAMES={"wobbleAmp","wobbleFreq","wobbleVar","wobbleFreqVar","wobbleRampFrames","drawRadius","glowScale","glowIntensity","losAlwaysVisible","cubeShowInside","cubeNoise","cubeNoiseSpeed","cubeNoiseScale","glowFalloff","coreBoost","hueJitter","whiteHotspot","whiteHotspotThreshold","sizePulseAmp","sizePulseFreq"};
inline constexpr const char* LUA_SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
// GLSL radians intrinsic, unavailable as a native MSL builtin.
float radians(float angle) { return angle * (3.14159265358979323846f / 180.f); }
struct LuaNanoUniform {float4x4 cameraViewProj,cameraViewInv;float4 mapSize,timeInfo;float parameters[24];uint4 options;};
struct LuaNanoVarying {float4 position [[position]];float4 g_color [[RECOIL_SAMPLE_PERSPECTIVE]];float3 g_normal [[RECOIL_SAMPLE_PERSPECTIVE]],g_worldPos [[RECOIL_SAMPLE_PERSPECTIVE]],g_localPos [[RECOIL_SAMPLE_PERSPECTIVE]],g_noiseSeed [[RECOIL_SAMPLE_PERSPECTIVE]];float2 g_glowUV [[RECOIL_SAMPLE_PERSPECTIVE]];float g_isGlow [[RECOIL_SAMPLE_PERSPECTIVE]],g_seed [[RECOIL_SAMPLE_PERSPECTIVE]];};
constexpr sampler infoSampler(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
struct LuaNanoProgram {
float vertexSlot;float4 spawnPosAndSize,velAndSpawnFrame,instColor,rotData;
float4x4 cameraViewProj,cameraViewInv;float4 mapSize,timeInfo;int u_shape;bool rejectFutureSpawns;
float4 gl_Position,fragColor,g_color;float3 g_normal,g_worldPos,g_localPos,g_noiseSeed;float2 g_glowUV;float g_isGlow,g_seed;
float wobbleAmp;
float wobbleFreq;
float wobbleVar;
float wobbleFreqVar;
float wobbleRampFrames;
float drawRadius;
float glowScale;
float glowIntensity;
float losAlwaysVisible;
float cubeShowInside;
float cubeNoise;
float cubeNoiseSpeed;
float cubeNoiseScale;
float glowFalloff;
float coreBoost;
float hueJitter;
float whiteHotspot;
float whiteHotspotThreshold;
float sizePulseAmp;
float sizePulseFreq;
float hash11(float x) {
	return fract(sin(x) * 43758.5453);
}

float3x3 rotXYZ(float3 a) {
	float cx = cos(a.x), sx = sin(a.x);
	float cy = cos(a.y), sy = sin(a.y);
	float cz = cos(a.z), sz = sin(a.z);
	float3x3 Rx = float3x3(float3(1,0,0),float3(0,cx,sx),float3(0,-sx,cx));
	float3x3 Ry = float3x3(float3(cy,0,-sy),float3(0,1,0),float3(sy,0,cy));
	float3x3 Rz = float3x3(float3(cz,sz,0),float3(-sz,cz,0),float3(0,0,1));
	return Rz * Ry * Rx;
}

void emitShapeVertex(int slot, float3 localPos, float3 normal, float3 center, float4 col, float3 noiseSeed, float seed) {
	g_color = col;
	g_normal = normal;
	g_noiseSeed = noiseSeed;
	g_isGlow = 0.0;
	g_glowUV = float2(0.0);
	g_seed = seed;
	g_localPos = localPos;
	g_worldPos = center + localPos;
	gl_Position = cameraViewProj * float4(g_worldPos, 1.0);
}

void emitGlowVertex(int slot, float3 localPos, float2 glowUV, float3 center, float4 col, float seed) {
	g_color = col;
	g_normal = float3(0.0, 1.0, 0.0);
	g_noiseSeed = float3(0.0);
	g_localPos = float3(0.0);
	g_isGlow = 1.0;
	g_seed = seed;
	g_glowUV = glowUV;
	g_worldPos = center + localPos;
	gl_Position = cameraViewProj * float4(g_worldPos, 1.0);
}

void VertexRun() {
	float currentFrame = timeInfo.x + timeInfo.w;
	float spawnFrame   = velAndSpawnFrame.w;
	float deathFrame   = rotData.w;

	if (currentFrame >= deathFrame || (rejectFutureSpawns && currentFrame < spawnFrame)) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		g_color = float4(0.0);
		g_normal = float3(0.0);
		g_worldPos = float3(0.0);
		g_localPos = float3(0.0);
		g_noiseSeed = float3(0.0);
		g_glowUV = float2(0.0);
		g_isGlow = 0.0;
		g_seed = 0.0;
		return;
	}

	float t = currentFrame - spawnFrame;
	if (t < 0.0) t = 0.0;

	float3 worldPos = spawnPosAndSize.xyz + velAndSpawnFrame.xyz * t;

	if (wobbleAmp > 0.0001) {
		float wobbleT   = max(currentFrame - rotData.z, 0.0);
		float totalLife = max(deathFrame - rotData.z, 1.0);
		float bell      = sin(3.14159265 * wobbleT / totalLife);
		if (wobbleRampFrames > 0.5)
			bell *= min(1.0, wobbleT / wobbleRampFrames);
		float3 vdir = velAndSpawnFrame.xyz;
		float3 ref, axA, axB;
		float refAngle = rotData.x * 0.01745329;
		float s1 = sin(refAngle), c1 = cos(refAngle);
		float s2 = sin(refAngle * 2.19), c2 = cos(refAngle * 2.19);
		if (abs(vdir.y) < 0.95) {
			ref = normalize(float3(s1, c1, s2));
		} else {
			ref = normalize(float3(c1, s2, c2));
		}
		float h1 = fract(sin(rotData.x * 12.9898 + rotData.y * 78.233) * 43758.5453);
		float h2 = fract(sin(rotData.x * 23.1451 + rotData.y * 34.567) * 65432.0987);
		axA = normalize(float3(
			sin(h1 * 6.28318),
			sin(h2 * 6.28318),
			cos(h1 * 3.14159 + h2 * 3.14159)
		));
		axB = cross(axA, ref);
		if (dot(axB, axB) > 0.001) {
			axB = normalize(axB);
		} else {
			axB = cross(axA, (abs(axA.x) < 0.9) ? float3(1, 0, 0) : float3(0, 1, 0));
			axB = normalize(axB);
		}
		float phaseOff = radians(rotData.x);
		float hash     = fract(sin(rotData.x * 12.9898 + rotData.y * 78.233) * 43758.5453);
		float freqScale = max(0.0, 1.0 + wobbleFreqVar * (2.0 * hash - 1.0));
		float dirSign  = (fract(hash * 7.31) < 0.5) ? -1.0 : 1.0;
		float hash2    = fract(hash * 113.7 + 0.317);
		float ampScale = max(0.0, 1.0 + wobbleVar * (2.0 * hash2 - 1.0));
		float ph = currentFrame * wobbleFreq * freqScale * dirSign * (6.2831853 / 30.0) + phaseOff;
		worldPos += (axA * cos(ph) + axB * sin(ph)) * (wobbleAmp * ampScale * bell);
	}

	float packedW    = abs(spawnPosAndSize.w);
	float fadeFrames = floor(packedW / 1024.0);
	float sizeMult   = (packedW - fadeFrames * 1024.0) / 256.0;

	float fade = (fadeFrames > 0.5)
		? clamp((deathFrame - currentFrame) / fadeFrames, 0.0, 1.0)
		: 1.0;

	float rotVel = rotData.y;
	float rotVal = rotData.x + rotVel  * t;

	float3 center = worldPos;
	float4 col = instColor * fade;
	float size  = drawRadius * sizeMult * (0.5 + 0.5 * fade);

	float3 phaseSeed = float3(rotData.x, rotData.y, rotData.x + rotData.y);
	float3 noiseSeed = phaseSeed * 137.0 + float3(11.0, 47.0, 83.0);
	float h  = dot(phaseSeed, float3(0.123, 0.456, 0.789));
	float3 phase = float3(hash11(h), hash11(h+1.7), hash11(h+3.3)) * 6.2831853;
	float r = radians(rotVal);
	float3 ang = phase + float3(r * 1.0, r * 1.3, r * 0.7);
	float3x3 R = rotXYZ(ang);
	float seed = radians(phaseSeed.x);

	int slot = int(vertexSlot);

	// The geometry shader only emits the glow quad when glow is actually enabled.
	// The template mesh always contains the 4 glow slots, so move them off-screen
	// when disabled to avoid wasted fragment shader work.
	bool emitGlow = (glowIntensity > 0.001) && (glowScale > 1.001);
	if (slot >= 24 && !emitGlow) {
		gl_Position = float4(2.0, 2.0, 2.0, 1.0);
		g_color = float4(0.0);
		g_normal = float3(0.0);
		g_worldPos = float3(0.0);
		g_localPos = float3(0.0);
		g_noiseSeed = float3(0.0);
		g_glowUV = float2(0.0);
		g_isGlow = 0.0;
		g_seed = 0.0;
		return;
	}

	if (u_shape == 1) {
		// ---- OCTAHEDRON ---- 24 shape verts (8 tris * 3) then 4 glow verts.
		if (slot < 24) {
			float3 X = R * float3(size, 0, 0); float3 nX = -X;
			float3 Y = R * float3(0, size, 0); float3 nY = -Y;
			float3 Z = R * float3(0, 0, size); float3 nZ = -Z;
			float k = 0.57735027;
			float3 n[8];
			n[0] = R * float3( k,  k,  k);
			n[1] = R * float3(-k,  k,  k);
			n[2] = R * float3(-k,  k, -k);
			n[3] = R * float3( k,  k, -k);
			n[4] = R * float3( k, -k,  k);
			n[5] = R * float3(-k, -k,  k);
			n[6] = R * float3(-k, -k, -k);
			n[7] = R * float3( k, -k, -k);
			float3 corners[8][3];
			corners[0][0] = Y;  corners[0][1] = Z;  corners[0][2] = X;
			corners[1][0] = Y;  corners[1][1] = nX; corners[1][2] = Z;
			corners[2][0] = Y;  corners[2][1] = nZ; corners[2][2] = nX;
			corners[3][0] = Y;  corners[3][1] = X;  corners[3][2] = nZ;
			corners[4][0] = nY; corners[4][1] = X;  corners[4][2] = Z;
			corners[5][0] = nY; corners[5][1] = Z;  corners[5][2] = nX;
			corners[6][0] = nY; corners[6][1] = nX; corners[6][2] = nZ;
			corners[7][0] = nY; corners[7][1] = nZ; corners[7][2] = X;
			int tri = slot / 3;
			int ci  = slot - tri * 3;
			emitShapeVertex(slot, corners[tri][ci], n[tri], center, col, noiseSeed, seed);
		} else {
			float3 right = cameraViewInv[0].xyz * (size * glowScale);
			float3 up    = cameraViewInv[1].xyz * (size * glowScale);
			int gi = slot - 24;
			if (gi == 0) emitGlowVertex(slot, -right - up, float2(-1.0, -1.0), center, col, seed);
			else if (gi == 1) emitGlowVertex(slot,  right - up, float2( 1.0, -1.0), center, col, seed);
			else if (gi == 2) emitGlowVertex(slot, -right + up, float2(-1.0,  1.0), center, col, seed);
			else              emitGlowVertex(slot,  right + up, float2( 1.0,  1.0), center, col, seed);
		}
	} else {
		// ---- CUBE (default) ---- 24 shape verts (6 quads * 4) then 4 glow verts.
		if (slot < 24) {
			float3 X = R * float3(size, 0, 0);
			float3 Y = R * float3(0, size, 0);
			float3 Z = R * float3(0, 0, size);
			float3 nXp =  R[0]; float3 nXm = -R[0];
			float3 nYp =  R[1]; float3 nYm = -R[1];
			float3 nZp =  R[2]; float3 nZm = -R[2];
			float3 corners[6][4];
			float3 normals[6];
			corners[0][0] =  X-Y-Z; corners[0][1] =  X+Y-Z; corners[0][2] =  X-Y+Z; corners[0][3] =  X+Y+Z; normals[0] = nXp;
			corners[1][0] = -X-Y-Z; corners[1][1] = -X-Y+Z; corners[1][2] = -X+Y-Z; corners[1][3] = -X+Y+Z; normals[1] = nXm;
			corners[2][0] = -X+Y-Z; corners[2][1] = -X+Y+Z; corners[2][2] =  X+Y-Z; corners[2][3] =  X+Y+Z; normals[2] = nYp;
			corners[3][0] = -X-Y-Z; corners[3][1] =  X-Y-Z; corners[3][2] = -X-Y+Z; corners[3][3] =  X-Y+Z; normals[3] = nYm;
			corners[4][0] = -X-Y+Z; corners[4][1] =  X-Y+Z; corners[4][2] = -X+Y+Z; corners[4][3] =  X+Y+Z; normals[4] = nZp;
			corners[5][0] = -X-Y-Z; corners[5][1] = -X+Y-Z; corners[5][2] =  X-Y-Z; corners[5][3] =  X+Y-Z; normals[5] = nZm;
			int quad = slot / 4;
			int ci   = slot - quad * 4;
			emitShapeVertex(slot, corners[quad][ci], normals[quad], center, col, noiseSeed, seed);
		} else {
			float3 right = cameraViewInv[0].xyz * (size * glowScale);
			float3 up    = cameraViewInv[1].xyz * (size * glowScale);
			int gi = slot - 24;
			if (gi == 0) emitGlowVertex(slot, -right - up, float2(-1.0, -1.0), center, col, seed);
			else if (gi == 1) emitGlowVertex(slot,  right - up, float2( 1.0, -1.0), center, col, seed);
			else if (gi == 2) emitGlowVertex(slot, -right + up, float2(-1.0,  1.0), center, col, seed);
			else              emitGlowVertex(slot,  right + up, float2( 1.0,  1.0), center, col, seed);
		}
	}
}

float hash13(float3 p) {
	p  = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
float valueNoise3(float3 p) {
	float3 i = floor(p);
	float3 f = fract(p);
	f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	float n000 = hash13(i + float3(0,0,0));
	float n100 = hash13(i + float3(1,0,0));
	float n010 = hash13(i + float3(0,1,0));
	float n110 = hash13(i + float3(1,1,0));
	float n001 = hash13(i + float3(0,0,1));
	float n101 = hash13(i + float3(1,0,1));
	float n011 = hash13(i + float3(0,1,1));
	float n111 = hash13(i + float3(1,1,1));
	float4 nx = mix(float4(n000, n010, n001, n011), float4(n100, n110, n101, n111), f.x);
	float2 ny = mix(nx.xz, nx.yw, f.y);
	return mix(ny.x, ny.y, f.z);
}

void FragmentRun(texture2d<float> infoTex) {
	// Early LOS discard -- bail before ANY shading/noise work for fogged
	// fragments. Saves the view-dir, noise, and normalize math when a particle
	// spawns in unseen territory.
	float losMul = 1.0;
	if (losAlwaysVisible < 0.5) {
		float2 losUV = clamp(g_worldPos.xz, float2(0.0), mapSize.xy) / mapSize.zw;
		float los = dot(float3(0.3333), infoTex.sample(infoSampler, losUV).rgb);
		losMul = smoothstep(0.30, 0.50, los);
		if (losMul < 0.02) discard_fragment();
	}

	// Per-particle hue jitter, identical curve to the billboard FS so both
	// modes drift through the same range of tints.
	float3 tint = float3(1.0);
	if (hueJitter > 0.0001) {
		tint = float3(1.0) + hueJitter * float3(
			sin(g_seed),
			sin(g_seed + 2.094),
			sin(g_seed + 4.188));
	}

	// Halo path: radial falloff billboard around the shape. Same falloff math
	// as the billboard FS so visuals match across modes.
	if (g_isGlow > 0.5) {
		float rd = length(g_glowUV);
		if (rd > 1.0) discard_fragment();
		float t = 1.0 - rd;
		float gI = glowIntensity;
		float glow = pow(clamp(t, 0.0, 1.0), max(glowFalloff, 0.01)) * gI;
		// Saturation-preserving brightness equalization -- see billboard FS.
		float3  glowTint = g_color.rgb / max(max(g_color.r, max(g_color.g, g_color.b)), 0.001);
		float gLuma    = dot(glowTint, float3(0.2126, 0.7152, 0.0722));
		const float GLOW_LUMA_TARGET = 0.55;
		const float GLOW_BOOST_MAX   = 5.0;
		float glowBoost = min(GLOW_LUMA_TARGET / max(gLuma, 0.001), GLOW_BOOST_MAX);
		fragColor = float4(glowTint * tint * (glow * glowBoost), g_color.a * glow) * losMul;
		return;
	}

	float3 N = normalize(g_normal);

	// Sun-style directional shading -- always on so faces are distinguishable
	// regardless of camera angle.
	float3 lightDir = normalize(float3(0.4, 1.0, 0.25));
	float dirShade = 0.85 + 0.15 * max(dot(N, lightDir), 0.0);

	float shade = dirShade;
	float aMul  = 1.0;

	// 3D view-dependent shading: only computed when enabled (skip the viewDir
	// normalize + ndv work entirely when cubeShowInside is off).
	if (cubeShowInside > 0.001) {
		float3 viewDir = normalize(cameraViewInv[3].xyz - g_worldPos);
		float ndv = dot(N, viewDir);
		float shade3D, alpha3D;
		if (ndv >= 0.0) {
			shade3D = 0.80 + 0.45 * (1.0 - ndv);    // front: edge-rim boost
			alpha3D = 1.0;
		} else {
			shade3D = 0.30 + 0.30 * (-ndv);         // back: dim but visible
			alpha3D = 0.55;
		}
		shade = mix(dirShade, dirShade * shade3D, cubeShowInside);
		aMul  = mix(1.0,      alpha3D,            cubeShowInside);
	}

	// Animated face noise: sampled in object-local space (rides along with the
	// tumbling cube) with a per-particle seed offset so cubes don't sparkle in
	// lockstep. Single-octave value noise with quintic fade -- cheap and the
	// per-particle offset hides any residual grid structure.
	float noiseVal = 0.5;  // neutral default when cubeNoise == 0
	if (cubeNoise > 0.001 || whiteHotspot > 0.001) {
		float tt = (timeInfo.x + timeInfo.w) * cubeNoiseSpeed * (1.0 / 30.0);
		float3 samp = g_localPos * cubeNoiseScale + g_noiseSeed + float3(tt, tt * 0.7, tt * 1.3);
		noiseVal = valueNoise3(samp);
		if (cubeNoise > 0.001) {
			shade *= 1.0 + cubeNoise * (noiseVal * 2.0 - 1.0);
		}
	}

	// Team-tinted base color with hue jitter and face shading baked in.
	float3 baseRgb = g_color.rgb * tint * shade * coreBoost;

	// White hotspot overlay: reuses the animated noise so bright regions
	// crawl across faces over time. Threshold + smoothstep keeps only the
	// high-noise peaks whitening, so the team color dominates everywhere else.
	if (whiteHotspot > 0.0001) {
		float hotspot = smoothstep(whiteHotspotThreshold, 1.0, noiseVal) * whiteHotspot;
		baseRgb = mix(baseRgb, float3(1.0) * max(shade, 0.6), hotspot);
	}

	fragColor = float4(baseRgb, g_color.a * aMul) * losMul;
}

};
LuaNanoProgram LuaNanoUniforms(constant LuaNanoUniform& u) {
 LuaNanoProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.mapSize=u.mapSize;p.timeInfo=u.timeInfo;p.u_shape=int(u.options.y);p.rejectFutureSpawns=u.options.z!=0;
 p.wobbleAmp=u.parameters[0];
 p.wobbleFreq=u.parameters[1];
 p.wobbleVar=u.parameters[2];
 p.wobbleFreqVar=u.parameters[3];
 p.wobbleRampFrames=u.parameters[4];
 p.drawRadius=u.parameters[5];
 p.glowScale=u.parameters[6];
 p.glowIntensity=u.parameters[7];
 p.losAlwaysVisible=u.parameters[8];
 p.cubeShowInside=u.parameters[9];
 p.cubeNoise=u.parameters[10];
 p.cubeNoiseSpeed=u.parameters[11];
 p.cubeNoiseScale=u.parameters[12];
 p.glowFalloff=u.parameters[13];
 p.coreBoost=u.parameters[14];
 p.hueJitter=u.parameters[15];
 p.whiteHotspot=u.parameters[16];
 p.whiteHotspotThreshold=u.parameters[17];
 p.sizePulseAmp=u.parameters[18];
 p.sizePulseFreq=u.parameters[19];
 return p;
}
vertex LuaNanoVarying LuaNanoVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float* slots [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LuaNanoUniform& u [[buffer(2)]]) {
 LuaNanoProgram p=LuaNanoUniforms(u);p.vertexSlot=slots[id];p.spawnPosAndSize=instances[instance*4];p.velAndSpawnFrame=instances[instance*4+1];p.instColor=instances[instance*4+2];p.rotData=instances[instance*4+3];p.VertexRun();
 LuaNanoVarying v;v.position=p.gl_Position;if(u.options.x==1)v.position.z=(v.position.z+v.position.w)*.5f;
 v.g_color=p.g_color;v.g_normal=p.g_normal;v.g_worldPos=p.g_worldPos;v.g_localPos=p.g_localPos;v.g_noiseSeed=p.g_noiseSeed;v.g_glowUV=p.g_glowUV;v.g_isGlow=p.g_isGlow;v.g_seed=p.g_seed;return v;
}
fragment float4 LuaNanoFragmentMain(LuaNanoVarying v [[stage_in]],constant LuaNanoUniform& u [[buffer(2)]],texture2d<float> infoTex [[texture(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
 LuaNanoProgram p=LuaNanoUniforms(u);p.g_color=v.g_color;p.g_normal=v.g_normal;p.g_worldPos=v.g_worldPos;p.g_localPos=v.g_localPos;p.g_noiseSeed=v.g_noiseSeed;p.g_glowUV=v.g_glowUV;p.g_isGlow=v.g_isGlow;p.g_seed=v.g_seed;p.FragmentRun(infoTex);return p.fragColor;
}
)MSL";
// Exact pinned engine NoGS analytic shader bodies; no Lua-mode substitution.
// VS SHA256: 0dc266ab522b1bcf17500425c59335ce0e122872d32f2c30d1c2492f48bddb9c
// FS SHA256: 6c6ecc494018df3db371eae8a09848b310e0cfb4a1a385dee1364ad3dd050ac9
inline constexpr const char* ENGINE_SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
// GLSL radians intrinsic, unavailable as a native MSL builtin.
float radians(float angle) { return angle * (3.14159265358979323846f / 180.f); }
constant float TAU=6.2831853;
constant float3 LUMA_WEIGHTS=float3(.2126,.7152,.0722);
constant float GLOW_TARGET_LUMA=.55,GLOW_MAX_BOOST=5;
struct EngineNanoUniform {float4x4 cameraViewProj;float4 cameraPos,cameraRight,cameraUp,clipPlane;float4 frustumPlanes[6];float parameters[20];uint4 options;};
struct EngineNanoTemplate {packed_float3 position,normal;packed_float2 glowUV;float isGlow;};
struct EngineNanoInstance {packed_float3 position,velocity;packed_float4 frames;uchar4 color;};
struct EngineNanoVarying {float4 position [[position]];float clipDistance [[clip_distance]];float4 g_color [[RECOIL_SAMPLE_PERSPECTIVE]];float3 g_normal [[RECOIL_SAMPLE_PERSPECTIVE]],g_worldPos [[RECOIL_SAMPLE_PERSPECTIVE]],g_localPos [[RECOIL_SAMPLE_PERSPECTIVE]],g_noiseSeed [[RECOIL_SAMPLE_PERSPECTIVE]];float2 g_glowUV [[RECOIL_SAMPLE_PERSPECTIVE]];float g_isGlow [[RECOIL_SAMPLE_PERSPECTIVE]],g_seed [[RECOIL_SAMPLE_PERSPECTIVE]],g_fade [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct EngineNanoProgram {
float3 templatePosition,templateNormal,particleStartPos,particleVelocity,cameraPos,cameraRight,cameraUp;float2 templateGlowUV;float templateIsGlow;
float4 particleFrames,particleColor,clipPlane,frustumPlanes[6];float4x4 cameraViewProj;
float4 gl_Position,fragColor,g_color;float clipDistance;float3 g_normal,g_worldPos,g_localPos,g_noiseSeed;float2 g_glowUV;float g_isGlow,g_seed,g_fade;
float drawRadius;
float sizeVariation;
float baseAlpha;
float alphaVariation;
float glowScale;
float glowIntensity;
float glowFalloff;
float colorEqualize;
float colorTargetLuma;
float hueJitter;
float coreBoost;
float showInside;
float noiseAmount;
float noiseScale;
float whiteHotspot;
float whiteHotspotThreshold;
float rotationRange;
float rotationRatePerFrame;
float noiseSpeedPerFrame;
float animationFrame;
bool outsideFrustum(float3 center, float radius)
{
	for (int i = 0; i < 6; ++i) {
		if (dot(frustumPlanes[i].xyz, center) + frustumPlanes[i].w < -radius)
			return true;
	}

	return false;
}

/* There is no geometry stage to return from here, so a vertex that should not
 * exist is pushed outside clip space and degenerates instead. */
void discardVertex()
{
	g_color = float4(0.0);
	g_normal = float3(0.0);
	g_worldPos = float3(0.0);
	g_localPos = float3(0.0);
	g_noiseSeed = float3(0.0);
	g_glowUV = float2(0.0);
	g_isGlow = 0.0;
	g_seed = 0.0;
	g_fade = 0.0;
	gl_Position = float4(2.0, 2.0, 2.0, 1.0);
	clipDistance = 1.0;
}

float hash11(float value)
{
	return fract(sin(value) * 43758.5453);
}

float3 equalizeColor(float3 color)
{
	float luma = dot(color, LUMA_WEIGHTS);
	if (luma < 0.001)
		return color;

	color *= pow(colorTargetLuma / luma, colorEqualize);

	float maxChannel = max(color.r, max(color.g, color.b));
	if (maxChannel > 1.0)
		color /= maxChannel;

	return color;
}

float3x3 rotXYZ(float3 angle)
{
	float cx = cos(angle.x), sx = sin(angle.x);
	float cy = cos(angle.y), sy = sin(angle.y);
	float cz = cos(angle.z), sz = sin(angle.z);
	float3x3 rotateX = float3x3(float3(1,0,0),float3(0,cx,sx),float3(0,-sx,cx));
	float3x3 rotateY = float3x3(float3(cy,0,-sy),float3(0,1,0),float3(sy,0,cy));
	float3x3 rotateZ = float3x3(float3(cz,sz,0),float3(-sz,cz,0),float3(0,0,1));
	return rotateZ * rotateY * rotateX;
}

void VertexRun()
{
	float createFrame = particleFrames.x;
	float deathFrame = particleFrames.y;
	float baseFrame = particleFrames.z;
	float fadeFrames = particleFrames.w;

	// the buffer is rebuilt on a cadence, so a particle can outlive its last upload
	if (animationFrame >= deathFrame) {
		discardVertex();
		return;
	}

	float age = max(animationFrame - createFrame, 0.0);
	float motionAge = max(animationFrame - baseFrame, 0.0);
	float fade = clamp((deathFrame - animationFrame) / max(fadeFrames, 0.01), 0.0, 1.0);
	float3 center = particleStartPos + particleVelocity * motionAge;

	float particleHash = dot(particleVelocity, float3(12.9898, 78.233, 37.719)) + createFrame * 0.6180339;
	float3 randomValues = float3(
		hash11(particleHash + 1.7),
		hash11(particleHash + 3.3),
		hash11(particleHash + 5.9)
	);

	float sizeMultiplier = 1.0 + sizeVariation * (hash11(particleHash + 7.1) * 2.0 - 1.0);
	// shrinks all the way to nothing, so the end of a fade is never a pop
	float size = drawRadius * sizeMultiplier * fade;
	float alpha = baseAlpha * (1.0 + alphaVariation * (hash11(particleHash + 11.3) * 2.0 - 1.0));

	float haloSize = size * glowScale;

	if (outsideFrustum(center, haloSize)) {
		discardVertex();
		return;
	}

	g_color = float4(equalizeColor(particleColor.rgb), max(alpha, 0.0));
	g_seed = randomValues.x * TAU;
	g_fade = fade;

	float3 worldOffset;
	if (templateIsGlow > 0.5) {
		worldOffset = (cameraRight * templateGlowUV.x + cameraUp * templateGlowUV.y) * haloSize;
		g_normal = float3(0.0, 1.0, 0.0);
		g_localPos = float3(0.0);
		g_noiseSeed = float3(0.0);
		g_glowUV = templateGlowUV;
		g_isGlow = 1.0;
	} else {
		float rotValue = mix(-rotationRange, rotationRange, hash11(particleHash + 13.7));
		float rotVelocity = mix(-rotationRatePerFrame, rotationRatePerFrame, hash11(particleHash + 17.9));
		float rotation = radians(rotValue + rotVelocity * age);
		float3 phase = randomValues * TAU;
		float3x3 rotationMatrix = rotXYZ(phase + float3(rotation, rotation * 1.3, rotation * 0.7));

		worldOffset = rotationMatrix * (templatePosition * size);
		g_normal = rotationMatrix * templateNormal;
		g_localPos = worldOffset;
		g_noiseSeed = randomValues * (360.0 * 137.0) + float3(11.0, 47.0, 83.0);
		g_glowUV = float2(0.0);
		g_isGlow = 0.0;
	}

	g_worldPos = center + worldOffset;
	gl_Position = cameraViewProj * float4(g_worldPos, 1.0);
	clipDistance = dot(float4(g_worldPos, 1.0), clipPlane);
}

float hash13(float3 value)
{
	value = fract(value * 0.1031);
	value += dot(value, value.zyx + 31.32);
	return fract((value.x + value.y) * value.z);
}

float valueNoise3(float3 value)
{
	float3 cell = floor(value);
	float3 fraction = fract(value);
	fraction = fraction * fraction * fraction * (fraction * (fraction * 6.0 - 15.0) + 10.0);

	float n000 = hash13(cell + float3(0, 0, 0));
	float n100 = hash13(cell + float3(1, 0, 0));
	float n010 = hash13(cell + float3(0, 1, 0));
	float n110 = hash13(cell + float3(1, 1, 0));
	float n001 = hash13(cell + float3(0, 0, 1));
	float n101 = hash13(cell + float3(1, 0, 1));
	float n011 = hash13(cell + float3(0, 1, 1));
	float n111 = hash13(cell + float3(1, 1, 1));
	float4 xMix = mix(float4(n000, n010, n001, n011), float4(n100, n110, n101, n111), fraction.x);
	float2 yMix = mix(xMix.xz, xMix.yw, fraction.y);
	return mix(yMix.x, yMix.y, fraction.z);
}

void FragmentRun()
{
	float3 tint = float3(1.0) + hueJitter * float3(
		sin(g_seed),
		sin(g_seed + 2.094),
		sin(g_seed + 4.188)
	);

	if (g_isGlow > 0.5) {
		float radialDistance = length(g_glowUV);
		if (radialDistance > 1.0)
			discard_fragment();

		float glow = pow(clamp(1.0 - radialDistance, 0.0, 1.0), glowFalloff) * glowIntensity;

		/* The halo carries the team colour at full saturation; normalising it
		 * first keeps a dark team's halo as bright as a light team's. */
		float3 glowTint = g_color.rgb / max(max(g_color.r, max(g_color.g, g_color.b)), 0.001);
		float glowLuma = dot(glowTint, LUMA_WEIGHTS);
		float glowBoost = min(GLOW_TARGET_LUMA / max(glowLuma, 0.001), GLOW_MAX_BOOST);

		/* Output is premultiplied (blend is ONE, ONE_MINUS_SRC_ALPHA), so the
		 * fade has to scale colour as well as alpha or the light never dims. */
		fragColor = float4(glowTint * tint * (glow * glowBoost), g_color.a * glow) * g_fade;
		return;
	}

	float3 normal = normalize(g_normal);
	float3 lightDirection = normalize(float3(0.4, 1.0, 0.25));
	float directionalShade = 0.85 + 0.15 * max(dot(normal, lightDirection), 0.0);

	/* Back faces are dimmed rather than culled, so the shape reads as a
	 * translucent chunk instead of a flat silhouette. */
	float3 viewDirection = normalize(cameraPos - g_worldPos);
	float normalDotView = dot(normal, viewDirection);
	float shade3D;
	float alpha3D;
	if (normalDotView >= 0.0) {
		shade3D = 0.80 + 0.45 * (1.0 - normalDotView);
		alpha3D = 1.0;
	} else {
		shade3D = 0.30 + 0.30 * (-normalDotView);
		alpha3D = 0.55;
	}

	float shade = mix(directionalShade, directionalShade * shade3D, showInside);
	float alphaMultiplier = mix(1.0, alpha3D, showInside);

	float noiseTime = animationFrame * noiseSpeedPerFrame;
	float3 noisePosition = g_localPos * noiseScale + g_noiseSeed + float3(noiseTime, noiseTime * 0.7, noiseTime * 1.3);
	float noiseValue = valueNoise3(noisePosition);
	shade *= 1.0 + noiseAmount * (noiseValue * 2.0 - 1.0);

	float3 baseColor = g_color.rgb * tint * shade * coreBoost;
	float hotspot = smoothstep(whiteHotspotThreshold, 1.0, noiseValue) * whiteHotspot;
	baseColor = mix(baseColor, float3(1.0) * max(shade, 0.6), hotspot);

	fragColor = float4(baseColor, g_color.a * alphaMultiplier) * g_fade;
}

};
EngineNanoProgram EngineNanoUniforms(constant EngineNanoUniform& u) {
 EngineNanoProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraPos=u.cameraPos.xyz;p.cameraRight=u.cameraRight.xyz;p.cameraUp=u.cameraUp.xyz;p.clipPlane=u.clipPlane;
 for(uint n=0;n<6;++n)p.frustumPlanes[n]=u.frustumPlanes[n];
 p.drawRadius=u.parameters[0];
 p.sizeVariation=u.parameters[1];
 p.baseAlpha=u.parameters[2];
 p.alphaVariation=u.parameters[3];
 p.glowScale=u.parameters[4];
 p.glowIntensity=u.parameters[5];
 p.glowFalloff=u.parameters[6];
 p.colorEqualize=u.parameters[7];
 p.colorTargetLuma=u.parameters[8];
 p.hueJitter=u.parameters[9];
 p.coreBoost=u.parameters[10];
 p.showInside=u.parameters[11];
 p.noiseAmount=u.parameters[12];
 p.noiseScale=u.parameters[13];
 p.whiteHotspot=u.parameters[14];
 p.whiteHotspotThreshold=u.parameters[15];
 p.rotationRange=u.parameters[16];
 p.rotationRatePerFrame=u.parameters[17];
 p.noiseSpeedPerFrame=u.parameters[18];
 p.animationFrame=u.parameters[19];
 return p;
}
vertex EngineNanoVarying EngineNanoVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const EngineNanoTemplate* mesh [[buffer(0)]],device const EngineNanoInstance* particles [[buffer(1)]],constant EngineNanoUniform& u [[buffer(2)]]) {
 EngineNanoProgram p=EngineNanoUniforms(u);p.templatePosition=float3(mesh[id].position);p.templateNormal=float3(mesh[id].normal);p.templateGlowUV=float2(mesh[id].glowUV);p.templateIsGlow=mesh[id].isGlow;
 p.particleStartPos=float3(particles[instance].position);p.particleVelocity=float3(particles[instance].velocity);p.particleFrames=float4(particles[instance].frames);p.particleColor=float4(particles[instance].color)/255.f;p.VertexRun();
 EngineNanoVarying v;v.position=p.gl_Position;if(u.options.x==1)v.position.z=(v.position.z+v.position.w)*.5f;v.clipDistance=p.clipDistance;
 v.g_color=p.g_color;v.g_normal=p.g_normal;v.g_worldPos=p.g_worldPos;v.g_localPos=p.g_localPos;v.g_noiseSeed=p.g_noiseSeed;v.g_glowUV=p.g_glowUV;v.g_isGlow=p.g_isGlow;v.g_seed=p.g_seed;v.g_fade=p.g_fade;return v;
}
fragment float4 EngineNanoFragmentMain(EngineNanoVarying v [[stage_in]],constant EngineNanoUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
 EngineNanoProgram p=EngineNanoUniforms(u);p.g_color=v.g_color;p.g_normal=v.g_normal;p.g_worldPos=v.g_worldPos;p.g_localPos=v.g_localPos;p.g_noiseSeed=v.g_noiseSeed;p.g_glowUV=v.g_glowUV;p.g_isGlow=v.g_isGlow;p.g_seed=v.g_seed;p.g_fade=v.g_fade;p.FragmentRun();return p.fragColor;
}
)MSL";
}
