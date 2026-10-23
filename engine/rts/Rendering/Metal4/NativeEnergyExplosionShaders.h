/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Exact31657 original NoGS vertex/fragment path and original indexed template.
namespace rendering::metal4::energy {
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
float radians(float x){return x*0.017453292519943295f;}
struct EnergyUniform {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float parameters[28];uint4 options;};
static_assert(sizeof(EnergyUniform)==272);
struct EnergyInstance {float4 spawnPosAndSize,velAndSpawnFrame,instColor,rotData;};
struct EnergyVarying {float4 position [[position]];float4 g_color [[flat]];float3 g_normal [[RECOIL_SAMPLE_PERSPECTIVE]];float3 g_worldPos [[RECOIL_SAMPLE_PERSPECTIVE]];float3 g_noisePos [[RECOIL_SAMPLE_PERSPECTIVE]];};


// Template mesh vertex carries one integer slot index (0..27), uploaded as a
// float because the engine VBO API convention uses float template attributes.






//__ENGINEUNIFORMBUFFERDEFS__





















// Same outputs as the geometry shader path (gsSrc in gfx_energy_explosion_particles_gl4.lua).





// Halo contributions under this round away in the RGBA8 framebuffer (sources clamp to [0, 1]),
// so each halo quad is cut to the radius where its falloff drops below it.
constant float GLOW_CUTOFF = 0.25 / 255.0;

// Corner signs per template slot in the geometry shader's emit order:
// cube = 6 faces x 4 (strips), octahedron = 8 triangles x 3.
constant float3 CUBE_CORNERS[24] = {
	float3( 1,-1,-1), float3( 1, 1,-1), float3( 1,-1, 1), float3( 1, 1, 1),
	float3(-1,-1,-1), float3(-1,-1, 1), float3(-1, 1,-1), float3(-1, 1, 1),
	float3(-1, 1,-1), float3(-1, 1, 1), float3( 1, 1,-1), float3( 1, 1, 1),
	float3(-1,-1,-1), float3( 1,-1,-1), float3(-1,-1, 1), float3( 1,-1, 1),
	float3(-1,-1, 1), float3( 1,-1, 1), float3(-1, 1, 1), float3( 1, 1, 1),
	float3(-1,-1,-1), float3(-1, 1,-1), float3( 1,-1,-1), float3( 1, 1,-1)
};
constant float3 CUBE_NORMALS[6] = {
	float3(1, 0, 0), float3(-1, 0, 0), float3(0, 1, 0), float3(0, -1, 0), float3(0, 0, 1), float3(0, 0, -1)
};
constant float3 OCTA_CORNERS[24] = {
	float3(0, 1, 0), float3(0, 0, 1), float3(1, 0, 0),
	float3(0, 1, 0), float3(-1, 0, 0), float3(0, 0, 1),
	float3(0, 1, 0), float3(0, 0, -1), float3(-1, 0, 0),
	float3(0, 1, 0), float3(1, 0, 0), float3(0, 0, -1),
	float3(0, -1, 0), float3(1, 0, 0), float3(0, 0, 1),
	float3(0, -1, 0), float3(0, 0, 1), float3(-1, 0, 0),
	float3(0, -1, 0), float3(-1, 0, 0), float3(0, 0, -1),
	float3(0, -1, 0), float3(0, 0, -1), float3(1, 0, 0)
};
constant float3 OCTA_NORMALS[8] = {
	float3( 1,  1,  1), float3(-1,  1,  1), float3(-1,  1, -1), float3( 1,  1, -1),
	float3( 1, -1,  1), float3(-1, -1,  1), float3(-1, -1, -1), float3( 1, -1, -1)
};
constant float2 GLOW_CORNERS[4] = {float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0), float2(1.0, 1.0)};

float hash11(float x) {
	return fract(sin(x) * 43758.5453);
}

float3x3 rotXYZ(float3 a) {
	float cx = cos(a.x), sx = sin(a.x);
	float cy = cos(a.y), sy = sin(a.y);
	float cz = cos(a.z), sz = sin(a.z);
	float3x3 Rx = float3x3(1,0,0, 0,cx,sx, 0,-sx,cx);
	float3x3 Ry = float3x3(cy,0,-sy, 0,1,0, sy,0,cy);
	float3x3 Rz = float3x3(cz,sz,0, -sz,cz,0, 0,0,1);
	return Rz * Ry * Rx;
}



//__ENGINEUNIFORMBUFFERDEFS__












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


struct EnergyVertexProgram {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float coreBoost;float cubeNoise;float cubeNoiseScale;float cubeNoiseSpeed;float cubeShowInside;float drag;float drawRadius;float fadeInFrames;float glowBreath;float glowBreathFreq;float glowBreathFreqVar;float glowBreathVar;float glowFalloff;float glowIntensity;float glowScale;float3 gravity;float hueJitter;int u_shape;float whiteHotspot;float whiteHotspotThreshold;float wobbleAmp;float wobbleFreq;float wobbleFreqVar;float wobbleRampFrames;float wobbleVar;float vertexSlot;float4 spawnPosAndSize,velAndSpawnFrame,instColor,rotData,gl_Position;float4 g_color;float3 g_normal;float3 g_worldPos;float3 g_noisePos;void run() {
	// Dead particles and skipped halos collapse off-screen.
	gl_Position = float4(2.0, 2.0, 2.0, 1.0);
	g_color = float4(0.0);
	g_normal = float3(0.0);
	g_worldPos = float3(0.0);
	g_noisePos = float3(0.0);

	float currentFrame = timeInfo.x + timeInfo.w;
	float spawnFrame   = velAndSpawnFrame.w;
	float deathFrame   = rotData.w;
	if (currentFrame >= deathFrame) return;

	int slot = int(vertexSlot);
	bool isGlow = slot >= 24;
	if (isGlow && !(glowIntensity > 0.0 && glowScale > 1.001)) return;

	float t = max(currentFrame - spawnFrame, 0.0);

	float tCap   = (drag > 0.0001) ? (1.0 / drag) : 1.0e6;
	float tClamp = min(t, tCap);
	float3 worldPos = spawnPosAndSize.xyz
	              + velAndSpawnFrame.xyz * tClamp * (1.0 - 0.5 * drag * tClamp)
	              + 0.5 * gravity * t * t;

	if (wobbleAmp > 0.0001) {
		float wobbleT   = max(currentFrame - spawnFrame, 0.0);
		float totalLife = max(deathFrame - spawnFrame, 1.0);
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

	float fadeOut = (fadeFrames > 0.5)
		? clamp((deathFrame - currentFrame) / fadeFrames, 0.0, 1.0)
		: 1.0;
	float fadeIn  = (fadeInFrames > 0.5)
		? clamp(t / fadeInFrames, 0.0, 1.0)
		: 1.0;
	float fade    = fadeOut * fadeIn;

	float3 center = worldPos;
	float4 col = instColor * fade;
	float size  = drawRadius * sizeMult;

	float3 phaseSeed = float3(rotData.x, rotData.y, rotData.x + rotData.y);
	float seed = radians(phaseSeed.x);

	float3 tint = float3(1.0);
	if (hueJitter > 0.0001) {
		tint = float3(1.0) + hueJitter * float3(
			sin(seed),
			sin(seed + 2.094),
			sin(seed + 4.188));
	}

	if (!isGlow) {
		float rotVel = rotData.y;
		float rotAcc = rotData.z;
		float rotVal = rotData.x + rotVel * t + 0.5 * rotAcc * t * t;

		float3 noiseSeed = phaseSeed * 137.0 + float3(11.0, 47.0, 83.0);
		float h  = dot(phaseSeed, float3(0.123, 0.456, 0.789));
		float3 phase = float3(hash11(h), hash11(h+1.7), hash11(h+3.3)) * 6.2831853;
		float r = radians(rotVal);
		float3 ang = phase + float3(r * 1.0, r * 1.3, r * 0.7);
		float3x3 R = rotXYZ(ang);

		float3 s, n;
		if (u_shape == 1) {
			s = OCTA_CORNERS[slot];
			n = R * (OCTA_NORMALS[slot / 3] * 0.57735027);
		} else {
			s = CUBE_CORNERS[slot];
			n = R * CUBE_NORMALS[slot / 4];
		}
		float3 lp = s.x * (R * float3(size, 0, 0)) + s.y * (R * float3(0, size, 0)) + s.z * (R * float3(0, 0, size));
		g_color = float4(col.rgb * tint * coreBoost, col.a);
		g_normal = n;
		g_worldPos = center + lp;
		g_noisePos = lp * cubeNoiseScale + noiseSeed;
		gl_Position = cameraViewProj * float4(g_worldPos, 1.0);
		return;
	}

	// Halo: same per-particle colour and cut radius as the geometry shader path.
	float totalLifeBR = max(deathFrame - spawnFrame, 1.0);
	float lifeFrac    = clamp(t / totalLifeBR, 0.0, 1.0);
	float breathScale = 1.0 - smoothstep(0.5, 1.0, lifeFrac);

	float gI = glowIntensity;
	if (glowBreath > 0.0001) {
		float hAmp  = fract(sin(seed * 91.7253 + 17.31) * 43758.5453);
		float hFreq = fract(sin(seed * 33.1117 + 43.93) * 27183.4500);
		float ampScale  = max(0.0, 1.0 + glowBreathVar     * (2.0 * hAmp  - 1.0));
		float freqScale = max(0.0, 1.0 + glowBreathFreqVar * (2.0 * hFreq - 1.0));
		float ph = (timeInfo.x + timeInfo.w) * glowBreathFreq * freqScale * (6.2831853 / 30.0) + seed;
		gI *= max(1.0 + glowBreath * breathScale * ampScale * sin(ph), 0.35);
	}
	float3  glowTint = col.rgb / max(max(col.r, max(col.g, col.b)), 0.001);
	float gLuma    = dot(glowTint, float3(0.2126, 0.7152, 0.0722));
	const float GLOW_LUMA_TARGET = 0.55;
	const float GLOW_BOOST_MAX   = 5.0;
	float glowBoost = min(GLOW_LUMA_TARGET / max(gLuma, 0.001), GLOW_BOOST_MAX);
	float4 glowCol = float4(glowTint * tint * (gI * glowBoost * col.a), gI * col.a);
	float peak = max(max(glowCol.r, glowCol.g), max(glowCol.b, glowCol.a));
	if (peak <= GLOW_CUTOFF) return;

	float extent = min(1.0 - pow(GLOW_CUTOFF / peak, 1.0 / max(glowFalloff, 0.01)), 1.0);
	float2 uv = GLOW_CORNERS[slot - 24] * extent;
	float halfSize = size * glowScale;
	g_color = glowCol;
	g_normal = float3(uv, 2.0);
	g_worldPos = center;
	g_noisePos = float3(0.0);
	gl_Position = cameraViewProj * float4(center + (cameraViewInv[0].xyz * uv.x + cameraViewInv[1].xyz * uv.y) * halfSize, 1.0);
}

};
struct EnergyFragmentProgram {float4x4 cameraViewInv;float4 timeInfo;float coreBoost;float cubeNoise;float cubeNoiseScale;float cubeNoiseSpeed;float cubeShowInside;float drag;float drawRadius;float fadeInFrames;float glowBreath;float glowBreathFreq;float glowBreathFreqVar;float glowBreathVar;float glowFalloff;float glowIntensity;float glowScale;float3 gravity;float hueJitter;int u_shape;float whiteHotspot;float whiteHotspotThreshold;float wobbleAmp;float wobbleFreq;float wobbleFreqVar;float wobbleRampFrames;float wobbleVar;float4 fragColor;float4 g_color;float3 g_normal;float3 g_worldPos;float3 g_noisePos;void run() {
	if (g_normal.z > 1.5) {
		// halo: tint, breath, intensity and fade are already in g_color (see the GS)
		float rd = length(g_normal.xy);
		if (rd > 1.0) discard_fragment();
		fragColor = g_color * pow(1.0 - rd, max(glowFalloff, 0.01));
		return;
	}

	float3 N = normalize(g_normal);
	float3 lightDir = normalize(float3(0.4, 1.0, 0.25));
	float dirShade = 0.85 + 0.15 * max(dot(N, lightDir), 0.0);

	float shade = dirShade;
	float aMul  = 1.0;
	if (cubeShowInside > 0.001) {
		float3 viewDir = normalize(cameraViewInv[3].xyz - g_worldPos);
		float ndv = dot(N, viewDir);
		float shade3D, alpha3D;
		if (ndv >= 0.0) {
			shade3D = 0.80 + 0.45 * (1.0 - ndv);
			alpha3D = 1.0;
		} else {
			shade3D = 0.30 + 0.30 * (-ndv);
			alpha3D = 0.55;
		}
		shade = mix(dirShade, dirShade * shade3D, cubeShowInside);
		aMul  = mix(1.0,      alpha3D,            cubeShowInside);
	}

	float noiseVal = 0.5;
	if (cubeNoise > 0.001 || whiteHotspot > 0.001) {
		float tt = (timeInfo.x + timeInfo.w) * cubeNoiseSpeed * (1.0 / 30.0);
		noiseVal = valueNoise3(g_noisePos + float3(tt, tt * 0.7, tt * 1.3));
		if (cubeNoise > 0.001) {
			shade *= 1.0 + cubeNoise * (noiseVal * 2.0 - 1.0);
		}
	}

	// g_color.rgb already carries the hue tint and coreBoost
	float3 baseRgb = g_color.rgb * shade;

	if (whiteHotspot > 0.0001) {
		float hotspot = smoothstep(whiteHotspotThreshold, 1.0, noiseVal) * whiteHotspot;
		baseRgb = mix(baseRgb, float3(1.0) * max(shade, 0.6), hotspot);
	}

	// Premultiplied output (matches ONE/1-SRC_ALPHA blend); ensures the
	// per-particle fade-in/out baked into g_color.a actually attenuates
	// the visible colour instead of only lifting destination weight.
	float outA = g_color.a * aMul;
	fragColor = float4(baseRgb * outA, outA);
}

};
vertex EnergyVarying EnergyVertexMain(uint vertexIndex [[vertex_id]],uint instance [[instance_id]],device const float* slots [[buffer(0)]],device const EnergyInstance* instances [[buffer(1)]],constant EnergyUniform& u [[buffer(2)]]){EnergyVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.coreBoost=u.parameters[0];p.cubeNoise=u.parameters[1];p.cubeNoiseScale=u.parameters[2];p.cubeNoiseSpeed=u.parameters[3];p.cubeShowInside=u.parameters[4];p.drag=u.parameters[5];p.drawRadius=u.parameters[6];p.fadeInFrames=u.parameters[7];p.glowBreath=u.parameters[8];p.glowBreathFreq=u.parameters[9];p.glowBreathFreqVar=u.parameters[10];p.glowBreathVar=u.parameters[11];p.glowFalloff=u.parameters[12];p.glowIntensity=u.parameters[13];p.glowScale=u.parameters[14];p.gravity=float3(u.parameters[15],u.parameters[16],u.parameters[17]);p.hueJitter=u.parameters[18];p.u_shape=int(u.parameters[19]);p.whiteHotspot=u.parameters[20];p.whiteHotspotThreshold=u.parameters[21];p.wobbleAmp=u.parameters[22];p.wobbleFreq=u.parameters[23];p.wobbleFreqVar=u.parameters[24];p.wobbleRampFrames=u.parameters[25];p.wobbleVar=u.parameters[26];p.vertexSlot=slots[vertexIndex];const EnergyInstance row=instances[instance];p.spawnPosAndSize=row.spawnPosAndSize;p.velAndSpawnFrame=row.velAndSpawnFrame;p.instColor=row.instColor;p.rotData=row.rotData;p.run();EnergyVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.g_color=p.g_color;out.g_normal=p.g_normal;out.g_worldPos=p.g_worldPos;out.g_noisePos=p.g_noisePos;return out;}
fragment float4 EnergyFragmentMain(EnergyVarying input [[stage_in]],constant EnergyUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));EnergyFragmentProgram p{};p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.coreBoost=u.parameters[0];p.cubeNoise=u.parameters[1];p.cubeNoiseScale=u.parameters[2];p.cubeNoiseSpeed=u.parameters[3];p.cubeShowInside=u.parameters[4];p.drag=u.parameters[5];p.drawRadius=u.parameters[6];p.fadeInFrames=u.parameters[7];p.glowBreath=u.parameters[8];p.glowBreathFreq=u.parameters[9];p.glowBreathFreqVar=u.parameters[10];p.glowBreathVar=u.parameters[11];p.glowFalloff=u.parameters[12];p.glowIntensity=u.parameters[13];p.glowScale=u.parameters[14];p.gravity=float3(u.parameters[15],u.parameters[16],u.parameters[17]);p.hueJitter=u.parameters[18];p.u_shape=int(u.parameters[19]);p.whiteHotspot=u.parameters[20];p.whiteHotspotThreshold=u.parameters[21];p.wobbleAmp=u.parameters[22];p.wobbleFreq=u.parameters[23];p.wobbleFreqVar=u.parameters[24];p.wobbleRampFrames=u.parameters[25];p.wobbleVar=u.parameters[26];p.g_color=input.g_color;p.g_normal=input.g_normal;p.g_worldPos=input.g_worldPos;p.g_noisePos=input.g_noisePos;p.run();return p.fragColor;}
)MSL";
}
