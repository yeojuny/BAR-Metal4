/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NATIVE_CAS_SHADER = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"CASMSL(
// Exact CAS/RCAS GLSL expressions from selected BAR31657; texture rows keep GL bottom origin.
struct CASUniforms { float4x4 cameraViewProjInv, cameraViewInv, cameraProj; float4 viewGeometry; int4 viewRect; float strength,groundY,debugFootprint,pad; float4 target; uint4 alpha;float4 raster; };
struct CASVarying { float4 position [[position]]; };
vertex CASVarying CASVertex(uint id [[vertex_id]],constant CASUniforms& u [[buffer(1)]]) { const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};return {float4(p[id],u.raster.y,1)}; }
float4 CASRead(texture2d<float,access::read> source,int2 p) {return source.read(uint2(p.x,int(source.get_height())-1-p.y));}
bool CASAlpha(float value,constant CASUniforms& u){
 if(!u.alpha.x)return true;float r=clamp(u.raster.x,0.0f,1.0f);
 switch(u.alpha.y){case 0:return false;case 1:return value<r;case 2:return value==r;case 3:return value<=r;case 4:return value>r;case 5:return value!=r;case 6:return value>=r;default:return true;}
}
struct CASOutput {float4 color [[color(0),index(0)]];float4 scale [[color(0),index(1)]];};
#define RCAS 0
#define PRESERVE_MSAA_EDGES 0
#define GRAIN_NOISE_ON 0
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment0(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 1
#define PRESERVE_MSAA_EDGES 0
#define GRAIN_NOISE_ON 0
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment1(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 0
#define PRESERVE_MSAA_EDGES 1
#define GRAIN_NOISE_ON 0
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment2(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 1
#define PRESERVE_MSAA_EDGES 1
#define GRAIN_NOISE_ON 0
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment3(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 0
#define PRESERVE_MSAA_EDGES 0
#define GRAIN_NOISE_ON 1
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment4(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 1
#define PRESERVE_MSAA_EDGES 0
#define GRAIN_NOISE_ON 1
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment5(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 0
#define PRESERVE_MSAA_EDGES 1
#define GRAIN_NOISE_ON 1
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment6(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
#define RCAS 1
#define PRESERVE_MSAA_EDGES 1
#define GRAIN_NOISE_ON 1
#define SHAPE_GRAIN 1
#define GRAIN_LO 0.03137f
#define GRAIN_HI 0.12549f
#define GRAIN_NOISE 0.300f
#define FOOT_LO 0.400f
#define FOOT_HI 1.000f
fragment CASOutput CASFragment7(CASVarying input [[stage_in]], texture2d<float,access::read> source [[texture(0)]],constant CASUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
float4 position=input.position;CASOutput result;result.scale=float4(0);

	int2 p = int2(float2(position.x * u.target.z, (u.target.y - position.y) * u.target.w)) - u.viewRect.xy;
	// fetches outside the texture are undefined, so the edge texels repeat
	int2 lo = max(p - 1, int2(0));
	int2 hi = min(p + 1, u.viewRect.zw);

	//   b
	// d e f
	//   h
	float3 b = CASRead(source, int2(p.x, lo.y)).rgb;
	float3 d = CASRead(source, int2(lo.x, p.y)).rgb;
	float3 e = CASRead(source, p).rgb;
	float3 f = CASRead(source, int2(hi.x, p.y)).rgb;
	float3 h = CASRead(source, int2(p.x, hi.y)).rgb;

#if RCAS
	// the strongest negative lobe that clips no channel, one for all channels
	float3 mn4 = min(min(b, d), min(f, h));
	float3 mx4 = max(max(b, d), max(f, h));
	float3 hitMin = min(mn4, e) / (4.0f * mx4);
	float3 hitMax = (1.0f - max(mx4, e)) / (4.0f * mn4 - 4.0f);
	// an all black or all white ring cannot clip on that side (0/0 above)
	hitMin = select(hitMin, float3(1.0e4f), mx4 == float3(0.0f));
	hitMax = select(hitMax, float3(-1.0e4f), mn4 == float3(1.0f));
	float3 lobeRGB = max(-hitMin, hitMax);
	float lobe = max(-0.1875f, min(max(max(lobeRGB.r, lobeRGB.g), lobeRGB.b), 0.0f));
#if SHAPE_GRAIN || GRAIN_NOISE_ON
	// luma of the taps, 0..2; grain = how far below the gate the local contrast sits
	const float3 lw = float3(0.5f, 1.0f, 0.5f);
	float bL = dot(b, lw), dL = dot(d, lw), eL = dot(e, lw), fL = dot(f, lw), hL = dot(h, lw);
	float mnL = min(min(bL, dL), min(min(fL, hL), eL));
	float mxL = max(max(bL, dL), max(max(fL, hL), eL));
	float grain = 1.0f - smoothstep(GRAIN_LO, GRAIN_HI, mxL - mnL);
#endif
#if SHAPE_GRAIN
	// CAS's lobe shrinks as the neighborhood nears black or white; blend to it for grain
	float ampL = clamp(min(mnL, 2.0f - mxL) / max(mxL, 1.0e-5f), 0.0f, 1.0f);
	lobe = max(lobe, mix(lobe, -0.2f * sqrt(ampL), grain));
#endif
#if GRAIN_NOISE_ON
	// one-pixel structure relative to the local contrast: 1 = isolated pixel or one-pixel line, 0.5f = edge, 0 = ramp
	float speck = max(abs((bL + hL) - 2.0f * eL), abs((dL + fL) - 2.0f * eL)) / max(2.0f * (mxL - mnL), 1.0e-5f);
	// ground elmos under this pixel: the view ray against the plane at the camera target's height
	float4 rayEnd = u.cameraViewProjInv * float4((float2(p) + 0.5f) / float2(u.viewRect.zw + 1) * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 camPos = u.cameraViewInv[3].xyz;
	float3 dir = normalize(rayEnd.xyz / rayEnd.w - camPos);
	float pixelAngle = 2.0f / (u.cameraProj[1][1] * u.viewGeometry.y);
	float footprint = (u.groundY - camPos.y) / min(dir.y, -1.0e-4f) * pixelAngle;
	float minified = smoothstep(FOOT_LO, FOOT_HI, footprint);
	lobe *= 1.0f - GRAIN_NOISE * min(speck, 1.0f) * grain * minified;
#endif
	lobe *= u.strength;
	float3 outColor = (lobe * ((b + d) + (f + h)) + e) * (1.0f / (4.0f * lobe + 1.0f));
#else
	// the negative lobe shrinks as the neighborhood approaches the signal limits
	float3 mn = min(min(min(d, e), min(f, b)), h);
	float3 mx = max(max(max(d, e), max(f, b)), h);
	float3 amp = clamp(min(mn, 1.0f - mx) * (1.0f / max(mx, 1.0e-5f)), 0.0f, 1.0f);
	float3 w = sqrt(amp) * u.strength;
	float3 outColor = clamp((((b + d) + (f + h)) * w + e) * (1.0f / (1.0f + 4.0f * w)), 0.0f, 1.0f);
#endif
#if RCAS && GRAIN_NOISE_ON
	outColor = mix(outColor, float3(1.0f, 0.0f, 0.0f), 0.5f * u.debugFootprint * minified);
#endif

#if PRESERVE_MSAA_EDGES
	// e is the average of the samples: scale them toward white or black until it lands on outColor
	outColor = clamp(outColor, 0.0f, 1.0f);
	bool3 up = outColor >= e;
	float3 scale = select(outColor, 1.0f - outColor, up) / max(select(e, 1.0f - e, up), 1.0e-5f);
	result.color = float4(select(float3(0.0f), 1.0f - scale, up), 1.0f);
	result.scale = float4(scale, 0.0f);
#else
	result.color = float4(outColor, 1.0f);
#endif

if(!CASAlpha(result.color.a,u))discard_fragment();
return result;
}
#undef RCAS
#undef PRESERVE_MSAA_EDGES
#undef GRAIN_NOISE_ON
#undef SHAPE_GRAIN
#undef GRAIN_LO
#undef GRAIN_HI
#undef GRAIN_NOISE
#undef FOOT_LO
#undef FOOT_HI
)CASMSL";
}
