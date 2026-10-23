/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Exact31657 GLSL expressions; interface/types adapted to MSL.
namespace rendering::metal4::lightning {
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
inline float mod(float x,float y){return x-y*floor(x/y); }
struct LightningUniform {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float parameters[21];uint4 options;float4 alpha;};
bool LightningAlpha(float a,float r,uint c){switch(c){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;default:return true;}}
struct CannonBoltVarying {float4 position [[position]];float3 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float3 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float coverage [[RECOIL_SAMPLE_PERSPECTIVE]];float segPos [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct CannonBoltVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float4 cornerAndSeg;float4 startPosAndWidth;float4 endPosAndLife;float4 coreColor;float4 edgeColor;float4 boltParams;float4 extraParams;float3 vCoreColor;float3 vEdgeColor;float alpha;float widthPos;float coverage;float segPos;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Strip vertex: xy = quad corner (-1..1), z = segment index


// Per-instance (one per arm)
   // xyz = bolt start, w = base width
       // xyz = bolt end,   w = life fraction (0..1)
           // rgb = core color, a = alpha
           // rgb = edge color, a = range falloff
          // x = seed, y = unused, z = segCount, w = jitterAmp
         // x = isBranch flag, y = widthScale, z = glowMult, w = impactSize



float hash11(float x) {
	return fract(sin(x * 12.9898) * 43758.5453);
}
float3 hash31(float x) {
	return float3(hash11(x), hash11(x + 17.13), hash11(x + 31.71)) * 2.0 - 1.0;
}

void cullVertex() {
	gl_Position = float4(2.0, 2.0, 2.0, 1.0);
}

void run()
{
	float segIndex = cornerAndSeg.z;
	float segCount = boltParams.z;
	if (segIndex >= segCount) { cullVertex(); return; }

	float3 startPos = startPosAndWidth.xyz;
	float baseWidth = startPosAndWidth.w * extraParams.y;
	float3 endPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	float seed     = boltParams.x;
	float jitterAmp= boltParams.w;

	float3 boltDir = endPos - startPos;
	float boltLen = length(boltDir);
	if (boltLen < 0.01 || segCount < 1.0) { cullVertex(); return; }
	float3 forward = boltDir / boltLen;

	// Two consistent perpendicular axes (Gram-Schmidt around an arbitrary up)
	float3 upRef = abs(forward.y) > 0.9 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
	float3 perpA = normalize(cross(forward, upRef));
	float3 perpB = normalize(cross(forward, perpA));

	// Time-quantized flicker phase (one phase per render frame): the bolt fully
	// "rerolls" its jitter pattern a few times per second for a more alive look
	// while still being stable within a single rendered frame.
	float frameTick = floor(timeInfo.z * 30.0);

	// Per-segment t along bolt; this vertex is at either t0 or t1 depending on quad.y.
	// SEGMENT_LENGTH_VAR jitters each interior boundary by a deterministic hash of
	// (seed, boundaryIndex). Endpoints (index 0 and segCount) stay fixed so the
	// bolt always meets its start/end. Adjacent segments share the boundary hash,
	// so segment N's t1 == segment N+1's t0 (no gaps).
	float yNorm = cornerAndSeg.y * 0.5 + 0.5;  // 0..1: 0 = segment start, 1 = segment end
	float invSeg = 1.0 / segCount;
	float lenVarAmp = SEGMENT_LENGTH_VAR * invSeg * 0.5;
	float b0 = segIndex;
	float b1 = segIndex + 1.0;
	float off0 = (b0 > 0.5 && b0 < segCount - 0.5) ? (hash11(seed * 41.0 + b0 * 17.3 + frameTick * 0.29) * 2.0 - 1.0) * lenVarAmp : 0.0;
	float off1 = (b1 > 0.5 && b1 < segCount - 0.5) ? (hash11(seed * 41.0 + b1 * 17.3 + frameTick * 0.29) * 2.0 - 1.0) * lenVarAmp : 0.0;
	float t0 = b0 * invSeg + off0;
	float t1 = b1 * invSeg + off1;
	float tHere = mix(t0, t1, yNorm);
	// Also compute the OTHER endpoint of this segment so we can derive a local
	// tangent for perpendicular offsetting (matches between adjacent segments
	// because both share the same hash inputs at the boundary).
	float tNext = (yNorm < 0.5) ? t1 : t0;

	// Hash inputs (combine seed + segment-corner + frame for organic per-shot pattern)
	float keyHere = seed * 13.0 + tHere * 91.0 + frameTick * 0.37;
	float keyNext = seed * 13.0 + tNext * 91.0 + frameTick * 0.37;

	// Distance-based jitter compensation: when the bolt width gets clamped UP
	// to MIN_PIXEL_WIDTH at distance, the unmodified world-space jitter (still
	// at full elmos) traces a tight zigzag with an artificially fat brush,
	// producing a chunky "pingpong" look from far away. Scale the jitter down
	// by the same coverage ratio so the silhouette stays visually similar at
	// any zoom: close-up = full jaggedness, far away = naturally smoothed out.
	// Zoom-invariant jitter: bounded to a fraction of bolt length so the
	// silhouette is identical at every zoom level (jitter and length both live
	// in world space, so their ratio is preserved under projection).
	float jitterScale = min(jitterAmp, boltLen * JITTER_MAX_BOLT_FRAC);

	// Perpendicular jitter offset (zero at the bolt ends so it stays anchored)
	float endFadeHere = sin(tHere * 3.14159);
	float endFadeNext = sin(tNext * 3.14159);
	float3 jitHere = (perpA * hash11(keyHere) + perpB * hash11(keyHere + 7.7)) * 2.0 - (perpA + perpB);
	float3 jitNext = (perpA * hash11(keyNext) + perpB * hash11(keyNext + 7.7)) * 2.0 - (perpA + perpB);
	jitHere *= jitterScale * endFadeHere;
	jitNext *= jitterScale * endFadeNext;

	float3 posHere = mix(startPos, endPos, tHere) + jitHere;
	float3 posNext = mix(startPos, endPos, tNext) + jitNext;

	// Local tangent for this segment (drives perpendicular billboard direction)
	float3 segDir = posNext - posHere;
	float segLen = length(segDir);
	if (segLen < 0.0001) { cullVertex(); return; }
	segDir /= segLen;

	// Camera-facing perpendicular for width quad
	float3 camPos = cameraViewInv[3].xyz;
	float3 toCam  = normalize(camPos - posHere);
	float3 right  = cross(segDir, toCam);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fb = normalize(cross(segDir, float3(0.0, 1.0, 0.0)));
		if (length(fb) < 0.001) fb = normalize(cross(segDir, float3(1.0, 0.0, 0.0)));
		float bl = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fb, right / max(rightLen, 0.001), bl));
	} else {
		right = right / rightLen;
	}

	// Lifetime fade in/out
	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { cullVertex(); return; }

	// Per-segment thickness variation + per-bolt flicker
	float segWobble = mix(1.0 - THICKNESS_VARIATION, 1.0 + THICKNESS_VARIATION,
		hash11(seed * 3.1 + segIndex * 5.7 + frameTick * 0.21));
	float flickerSeed = seed * 0.97 + frameTick * 0.13;
	float flicker = 1.0 - FLICKER_AMPLITUDE * 0.5 + FLICKER_AMPLITUDE * hash11(flickerSeed);

	float width = baseWidth * lifePulse * segWobble * flicker;

	// Min-pixel-width inflation with alpha compensation
	float camDist = length(camPos - mix(startPos, endPos, 0.5));
	float minWidth = camDist * MIN_PIXEL_WIDTH;
	// Cap the inflated width to a fraction of the jitter amplitude so the
	// per-segment brush rotation never exceeds the path wiggle (otherwise the
	// brush teeth become visible as a chunky saw at extreme zoom-out).
	minWidth = min(minWidth, jitterScale * BRUSH_MAX_JITTER_FRAC);
	float coverageVal = clamp(width / max(minWidth, 0.001), 0.0, 1.0);
	width = max(width, minWidth);

	float3 vertexWorld = posHere + right * cornerAndSeg.x * width;
	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	vCoreColor = coreColor.rgb;
	vEdgeColor = edgeColor.rgb;
	widthPos = cornerAndSeg.x;
	coverage = coverageVal;
	segPos = tHere;

	float rangeFalloff = edgeColor.a;
	float alphaFalloff = 1.0 - rangeFalloff * tHere;
	alpha = coreColor.a * lifePulse * alphaFalloff * coverageVal * flicker;
}

};
vertex CannonBoltVarying CannonBoltVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){CannonBoltVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.cornerAndSeg=mesh[id];p.startPosAndWidth=instances[instance*6];p.endPosAndLife=instances[instance*6+1];p.coreColor=instances[instance*6+2];p.edgeColor=instances[instance*6+3];p.boltParams=instances[instance*6+4];p.extraParams=instances[instance*6+5];p.run();CannonBoltVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;out.alpha=p.alpha;out.widthPos=p.widthPos;out.coverage=p.coverage;out.segPos=p.segPos;return out;}
struct CannonBoltFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float3 vCoreColor;float3 vEdgeColor;float alpha;float widthPos;float coverage;float segPos;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	float edgeDist = abs(widthPos);
	float fw = fwidth(edgeDist);
	float aaStart = CORE_EDGE_START - fw * 0.5;
	float aaEnd   = max(CORE_EDGE_END, CORE_EDGE_START + fw);
	float coreFactor = 1.0 - smoothstep(aaStart, aaEnd, edgeDist);

	// Radial alpha falloff across width (no texture needed; pure procedural)
	float radial = 1.0 - smoothstep(0.0, 1.0, edgeDist);

	float3 col = mix(vEdgeColor, vCoreColor, coreFactor);
	col *= BRIGHTNESS_MULT;
	col *= (1.0 + coreFactor * CORE_BRIGHTNESS);
	col *= radial * alpha;

	float lum = dot(col, float3(0.299, 0.587, 0.114));
	if (lum < 0.0005) discard_fragment();
	float edgeSoft = smoothstep(0.0005, 0.004, lum);
	col *= edgeSoft;

	fragColor = float4(col, 0.0);
}

};
fragment float4 CannonBoltFragmentMain(CannonBoltVarying input [[stage_in]],constant LightningUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));CannonBoltFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.alpha=input.alpha;p.widthPos=input.widthPos;p.coverage=input.coverage;p.segPos=input.segPos;p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}
struct CannonGlowVarying {float4 position [[position]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float lengthT [[RECOIL_SAMPLE_PERSPECTIVE]];float3 glowColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float glowMult [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct CannonGlowVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float4 cornerAndSeg;float4 startPosAndWidth;float4 endPosAndLife;float4 coreColor;float4 edgeColor;float4 boltParams;float4 extraParams;float widthPos;float lengthT;float3 glowColor;float alpha;float glowMult;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__











float hash11(float x) {
	return fract(sin(x * 12.9898) * 43758.5453);
}

void run()
{
	float3 startPos = startPosAndWidth.xyz;
	float baseWidth = startPosAndWidth.w * extraParams.y;
	float3 endPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	float seed     = boltParams.x;
	float segCount = boltParams.z;

	float3 boltDir = endPos - startPos;
	float boltLen = length(boltDir);
	if (boltLen < 0.01 || segCount < 1.0) { gl_Position = float4(2.0,2.0,2.0,1.0); return; }
	// Glow is drawn as ONE quad per arm spanning the full length (start->end):
	// drawAll only draws the first quad of the segment strip for this pass.
	float3 forward = boltDir / boltLen;

	// Lifetime envelope first so we can early-out
	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { gl_Position = float4(2.0,2.0,2.0,1.0); return; }

	// Position on the STRAIGHT bolt axis (no jitter). yNorm = 0 at start, 1 at end.
	float yNorm = cornerAndSeg.y * 0.5 + 0.5;
	float tHere = yNorm;
	float3 posOnAxis = mix(startPos, endPos, tHere);

	// Camera-facing perpendicular based on the OVERALL bolt forward (constant
	// across all segments of this bolt, so quads share orientation).
	float3 camPos = cameraViewInv[3].xyz;
	float3 toCam  = normalize(camPos - posOnAxis);
	float3 right  = cross(forward, toCam);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fb = normalize(cross(forward, float3(0.0, 1.0, 0.0)));
		if (length(fb) < 0.001) fb = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
		float bl = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fb, right / max(rightLen, 0.001), bl));
	} else {
		right = right / rightLen;
	}

	// Slight per-bolt flicker (same hash key as the bolt body so they match)
	float frameTick = floor(timeInfo.z * 30.0);
	float flickerSeed = seed * 0.97 + frameTick * 0.13;
	float flicker = 1.0 - FLICKER_AMPLITUDE * 0.5 + FLICKER_AMPLITUDE * hash11(flickerSeed);
	float glowWidth = baseWidth * GLOW_WIDTH_MULT * lifePulse * flicker;

	// Min-pixel-width inflation with alpha compensation: keep the glow halo
	// visible when zoomed out (otherwise it shrinks to nothing while the bolt
	// core's own min-pixel clamp leaves a fat plain stripe behind).
	float camDist = length(camPos - posOnAxis);
	float minWidth = camDist * GLOW_MIN_PIXEL_WIDTH;
	float coverageVal = clamp(glowWidth / max(minWidth, 0.001), 0.0, 1.0);
	glowWidth = max(glowWidth, minWidth);

	float3 vertexWorld = posOnAxis + right * cornerAndSeg.x * glowWidth;
	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	widthPos = cornerAndSeg.x;
	// Pass tHere through and compute the sin in the FS. Doing sin() in the VS
	// would make the value at the two end-vertices both 0 and the GPU's linear
	// interpolation would yield 0 across the entire quad (since the quad now
	// spans the full bolt length); only per-pixel sin gives the proper bell.
	lengthT = tHere;
	glowColor = edgeColor.rgb;
	float rangeFalloff = edgeColor.a;
	float alphaFalloff = 1.0 - rangeFalloff * tHere;
	alpha = coreColor.a * lifePulse * alphaFalloff * flicker * coverageVal;
	glowMult = extraParams.z;
}

};
vertex CannonGlowVarying CannonGlowVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){CannonGlowVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.cornerAndSeg=mesh[id];p.startPosAndWidth=instances[instance*6];p.endPosAndLife=instances[instance*6+1];p.coreColor=instances[instance*6+2];p.edgeColor=instances[instance*6+3];p.boltParams=instances[instance*6+4];p.extraParams=instances[instance*6+5];p.run();CannonGlowVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.widthPos=p.widthPos;out.lengthT=p.lengthT;out.glowColor=p.glowColor;out.alpha=p.alpha;out.glowMult=p.glowMult;return out;}
struct CannonGlowFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float widthPos;float lengthT;float3 glowColor;float alpha;float glowMult;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	float edgeDist = abs(widthPos);
	if (edgeDist >= 1.0) discard_fragment();
	// Gaussian-like falloff so quad edges fade to zero quickly (hides quad outlines).
	// GLOW_FALLOFF_POWER controls sharpness: higher = tighter core, longer dim tail.
	float g = exp(-edgeDist * edgeDist * GLOW_FALLOFF_POWER);
	// Subtract the value at the edge so the curve actually reaches 0 at |x|=1
	// (otherwise a residual constant produces the rectangular outline).
	float edgeVal = exp(-GLOW_FALLOFF_POWER);
	float falloff = max(g - edgeVal, 0.0) / max(1.0 - edgeVal, 0.0001);
	// Anchor glow to zero at both bolt ends. Computed in FS so per-pixel sin
	// gives the proper bell shape across the full-bolt quad.
	float lengthEndFade = sin(clamp(lengthT, 0.0, 1.0) * 3.14159);
	falloff *= lengthEndFade;
	float3 col = glowColor * (falloff * alpha * glowMult);
	float lum = dot(col, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();
	fragColor = float4(col, 0.0);
}

};
fragment float4 CannonGlowFragmentMain(CannonGlowVarying input [[stage_in]],constant LightningUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));CannonGlowFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.widthPos=input.widthPos;p.lengthT=input.lengthT;p.glowColor=input.glowColor;p.alpha=input.alpha;p.glowMult=input.glowMult;p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}
struct CannonImpactVarying {float4 position [[position]];float2 texCoords [[RECOIL_SAMPLE_PERSPECTIVE]];float3 vColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct CannonImpactVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float4 cornerAndSeg;float4 endPosAndLife;float4 coreColor;float4 boltParams;float4 extraParams;float2 texCoords;float3 vColor;float alpha;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__









float hash11(float x) {
	return fract(sin(x * 12.9898) * 43758.5453);
}

void run()
{
	float impactSize= extraParams.w;
	if (impactSize <= 0.0) {
		gl_Position = float4(2.0,2.0,2.0,1.0); return;
	}

	float3 worldPos = endPosAndLife.xyz;
	float lifeFrac = endPosAndLife.w;

	float fadeIn  = smoothstep(0.0, FADE_IN_END, lifeFrac);
	float fadeOut = 1.0 - smoothstep(FADE_OUT_START, 1.0, lifeFrac);
	float lifePulse = fadeIn * fadeOut;
	if (lifePulse < 0.001) { gl_Position = float4(2.0,2.0,2.0,1.0); return; }

	float frameTick = floor(timeInfo.z * 30.0);
	float seed = boltParams.x;
	float flicker = 1.0 - FLICKER_AMPLITUDE * 0.5 + FLICKER_AMPLITUDE * hash11(seed * 0.97 + frameTick * 0.13);

	float3 camRight = cameraViewInv[0].xyz;
	float3 camUp    = cameraViewInv[1].xyz;
	float size = impactSize * lifePulse * flicker;
	float3 vert = worldPos
		+ camRight * cornerAndSeg.x * size
		+ camUp    * cornerAndSeg.y * size;

	gl_Position = cameraViewProj * float4(vert, 1.0);
	texCoords = cornerAndSeg.xy * 0.5 + 0.5;
	vColor = coreColor.rgb;
	alpha = coreColor.a * lifePulse * flicker;
}

};
vertex CannonImpactVarying CannonImpactVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){CannonImpactVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.cornerAndSeg=mesh[id];p.endPosAndLife=instances[instance*6+1];p.coreColor=instances[instance*6+2];p.boltParams=instances[instance*6+4];p.extraParams=instances[instance*6+5];p.run();CannonImpactVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.texCoords=p.texCoords;out.vColor=p.vColor;out.alpha=p.alpha;return out;}
struct CannonImpactFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float2 texCoords;float3 vColor;float alpha;float4 fragColor;texture2d<float> impactTex;float4 sample(texture2d<float> t,float2 uv){constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear);return t.sample(s,uv);}





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__







void run()
{
	float4 tex = sample(impactTex, texCoords);
	float shape = max(tex.a, dot(tex.rgb, float3(0.299, 0.587, 0.114)));
	if (shape < 0.001) discard_fragment();
	float3 col = vColor * shape * alpha * IMPACT_BRIGHTNESS * BRIGHTNESS_MULT;
	float lum = dot(col, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();
	fragColor = float4(col, 0.0);
}

};
fragment float4 CannonImpactFragmentMain(CannonImpactVarying input [[stage_in]],constant LightningUniform& u [[buffer(2)]],texture2d<float> image [[texture(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));CannonImpactFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.texCoords=input.texCoords;p.vColor=input.vColor;p.alpha=input.alpha;p.impactTex=image;p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}
struct EnvironmentBoltVarying {float4 position [[position]];float3 vCoreColor [[RECOIL_SAMPLE_PERSPECTIVE]];float3 vEdgeColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float coverage [[RECOIL_SAMPLE_PERSPECTIVE]];float feather [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct EnvironmentBoltVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float4 cornerAndSeg;float4 startPosAndWidth;float4 endPosAndTip;float4 coreColor;float4 edgeColor;float4 boltParams;float4 extraParams;float3 vCoreColor;float3 vEdgeColor;float alpha;float widthPos;float coverage;float feather;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__

// Strip vertex: xy = quad corner (-1..1), z = segment index


// Per-instance (one per arm)
   // xyz = arm start, w = base width
       // xyz = current (grown) tip, w = tip taper
          // rgb = core color, a = alphaMul
          // rgb = edge color, a = root taper
         // x = seed, y = unused, z = segCount, w = jitterAmp
        // x = widthMul, y = widthScale, z = glowMult, w = feather



float hash11(float x) { return fract(sin(x * 12.9898) * 43758.5453); }

void cullVertex() { gl_Position = float4(2.0, 2.0, 2.0, 1.0); }

void run()
{
	float segIndex = cornerAndSeg.z;
	float segCount = boltParams.z;
	if (segIndex >= segCount) { cullVertex(); return; }

	float3 startPos   = startPosAndWidth.xyz;
	float baseWidth = startPosAndWidth.w * extraParams.y * extraParams.x;
	float3 endPos     = endPosAndTip.xyz;
	float tipTaper  = endPosAndTip.w;
	float rootTaper = edgeColor.a;

	float seed     = boltParams.x;
	float jitterAmp= boltParams.w;

	float3 boltDir = endPos - startPos;
	float boltLen = length(boltDir);
	if (boltLen < 0.01 || segCount < 1.0) { cullVertex(); return; }
	float3 forward = boltDir / boltLen;

	float3 upRef = abs(forward.y) > 0.9 ? float3(1.0, 0.0, 0.0) : float3(0.0, 1.0, 0.0);
	float3 perpA = normalize(cross(forward, upRef));
	float3 perpB = normalize(cross(forward, perpA));

	float frameTick = ENV_LIGHTNING_ANIMATE > 0.5 ? floor(timeInfo.z * ANIMATE_RATE) : 0.0;

	// Per-segment t with deterministic per-boundary length jitter (endpoints fixed)
	float yNorm = cornerAndSeg.y * 0.5 + 0.5;
	float invSeg = 1.0 / segCount;
	float lenVarAmp = SEGMENT_LENGTH_VAR * invSeg * 0.5;
	float b0 = segIndex;
	float b1 = segIndex + 1.0;
	float off0 = (b0 > 0.5 && b0 < segCount - 0.5) ? (hash11(seed * 41.0 + b0 * 17.3 + frameTick * 0.29) * 2.0 - 1.0) * lenVarAmp : 0.0;
	float off1 = (b1 > 0.5 && b1 < segCount - 0.5) ? (hash11(seed * 41.0 + b1 * 17.3 + frameTick * 0.29) * 2.0 - 1.0) * lenVarAmp : 0.0;
	float t0 = b0 * invSeg + off0;
	float t1 = b1 * invSeg + off1;
	float tHere = mix(t0, t1, yNorm);
	float tNext = (yNorm < 0.5) ? t1 : t0;

	float keyHere = seed * 13.0 + tHere * 91.0 + frameTick * 0.37;
	float keyNext = seed * 13.0 + tNext * 91.0 + frameTick * 0.37;

	float jitterScale = min(jitterAmp, boltLen * JITTER_MAX_BOLT_FRAC);

	float endFadeHere = sin(tHere * 3.14159);
	float endFadeNext = sin(tNext * 3.14159);
	float3 jitHere = (perpA * hash11(keyHere) + perpB * hash11(keyHere + 7.7)) * 2.0 - (perpA + perpB);
	float3 jitNext = (perpA * hash11(keyNext) + perpB * hash11(keyNext + 7.7)) * 2.0 - (perpA + perpB);
	jitHere *= jitterScale * endFadeHere;
	jitNext *= jitterScale * endFadeNext;

	float3 posHere = mix(startPos, endPos, tHere) + jitHere;
	float3 posNext = mix(startPos, endPos, tNext) + jitNext;

	float3 segDir = posNext - posHere;
	float segLen = length(segDir);
	if (segLen < 0.0001) { cullVertex(); return; }
	segDir /= segLen;

	float3 camPos = cameraViewInv[3].xyz;
	float3 toCam  = normalize(camPos - posHere);
	float3 right  = cross(segDir, toCam);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fb = normalize(cross(segDir, float3(0.0, 1.0, 0.0)));
		if (length(fb) < 0.001) fb = normalize(cross(segDir, float3(1.0, 0.0, 0.0)));
		float bl = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fb, right / max(rightLen, 0.001), bl));
	} else {
		right = right / rightLen;
	}

	float segWobble = mix(1.0 - THICKNESS_VARIATION, 1.0 + THICKNESS_VARIATION,
		hash11(seed * 3.1 + segIndex * 5.7 + frameTick * 0.21));
	float flicker = 1.0;
	if (ENV_LIGHTNING_ANIMATE > 0.5) {
		float flickerSeed = seed * 0.97 + frameTick * 0.13;
		flicker = 1.0 - FLICKER_AMPLITUDE * 0.5 + FLICKER_AMPLITUDE * hash11(flickerSeed);
	}

	// Leading-tip taper: thin toward the (unconnected) far end.
	float tipW = mix(1.0, tipTaper, smoothstep(TIP_TAPER_START, 1.0, tHere));
	// Root taper: thin toward the origin (1.0 = no taper, full width at start).
	float rootW = mix(1.0, rootTaper, 1.0 - smoothstep(0.0, ROOT_TAPER_END, tHere));

	float width = baseWidth * segWobble * flicker * tipW * rootW;

	float camDist = length(camPos - mix(startPos, endPos, 0.5));
	float minWidth = camDist * MIN_PIXEL_WIDTH;
	minWidth = min(minWidth, jitterScale * BRUSH_MAX_JITTER_FRAC);
	float coverageVal = clamp(width / max(minWidth, 0.001), 0.0, 1.0);
	width = max(width, minWidth);

	float3 vertexWorld = posHere + right * cornerAndSeg.x * width;
	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	vCoreColor = coreColor.rgb;
	vEdgeColor = edgeColor.rgb;
	widthPos = cornerAndSeg.x;
	coverage = coverageVal;
	feather  = extraParams.w;

	alpha = coreColor.a * coverageVal * flicker * (1.0 - LENGTH_FALLOFF * tHere);
}

};
vertex EnvironmentBoltVarying EnvironmentBoltVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){EnvironmentBoltVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.cornerAndSeg=mesh[id];p.startPosAndWidth=instances[instance*6];p.endPosAndTip=instances[instance*6+1];p.coreColor=instances[instance*6+2];p.edgeColor=instances[instance*6+3];p.boltParams=instances[instance*6+4];p.extraParams=instances[instance*6+5];p.run();EnvironmentBoltVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.vCoreColor=p.vCoreColor;out.vEdgeColor=p.vEdgeColor;out.alpha=p.alpha;out.widthPos=p.widthPos;out.coverage=p.coverage;out.feather=p.feather;return out;}
struct EnvironmentBoltFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float3 vCoreColor;float3 vEdgeColor;float alpha;float widthPos;float coverage;float feather;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	float edgeDist = abs(widthPos);
	float fw = fwidth(edgeDist);
	float coreEnd = CORE_EDGE_END * mix(1.0, 2.5, feather);
	float aaStart = CORE_EDGE_START - fw * 0.5;
	float aaEnd   = max(coreEnd, CORE_EDGE_START + fw);
	float coreFactor = 1.0 - smoothstep(aaStart, aaEnd, edgeDist);

	float radial = 1.0 - smoothstep(0.0, 1.0, edgeDist);

	float3 col = mix(vEdgeColor, vCoreColor, coreFactor);
	col *= BRIGHTNESS_MULT;
	col *= (1.0 + coreFactor * CORE_BRIGHTNESS);
	col *= radial * alpha;

	float lum = dot(col, float3(0.299, 0.587, 0.114));
	if (lum < 0.0005) discard_fragment();
	float edgeSoft = smoothstep(0.0005, 0.004, lum);
	col *= edgeSoft;

	fragColor = float4(col, 0.0);
}

};
fragment float4 EnvironmentBoltFragmentMain(EnvironmentBoltVarying input [[stage_in]],constant LightningUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));EnvironmentBoltFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.vCoreColor=input.vCoreColor;p.vEdgeColor=input.vEdgeColor;p.alpha=input.alpha;p.widthPos=input.widthPos;p.coverage=input.coverage;p.feather=input.feather;p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}
struct EnvironmentGlowVarying {float4 position [[position]];float widthPos [[RECOIL_SAMPLE_PERSPECTIVE]];float lengthT [[RECOIL_SAMPLE_PERSPECTIVE]];float3 glowColor [[RECOIL_SAMPLE_PERSPECTIVE]];float alpha [[RECOIL_SAMPLE_PERSPECTIVE]];float glowMult [[RECOIL_SAMPLE_PERSPECTIVE]];float feather [[RECOIL_SAMPLE_PERSPECTIVE]];};
struct EnvironmentGlowVertexProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float4 cornerAndSeg;float4 startPosAndWidth;float4 endPosAndTip;float4 coreColor;float4 edgeColor;float4 boltParams;float4 extraParams;float widthPos;float lengthT;float3 glowColor;float alpha;float glowMult;float feather;float4 gl_Position;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__











float hash11(float x) { return fract(sin(x * 12.9898) * 43758.5453); }

void run()
{
	float3 startPos   = startPosAndWidth.xyz;
	float baseWidth = startPosAndWidth.w * extraParams.y * extraParams.x;
	float3 endPos     = endPosAndTip.xyz;

	float seed     = boltParams.x;
	float segCount = boltParams.z;
	float jitterAmp= boltParams.w;

	float3 boltDir = endPos - startPos;
	float boltLen = length(boltDir);
	if (boltLen < 0.01 || segCount < 1.0) { gl_Position = float4(2.0,2.0,2.0,1.0); return; }
	// One glow quad per arm: drawAll only draws the first quad of the strip.
	float3 forward = boltDir / boltLen;

	float yNorm = cornerAndSeg.y * 0.5 + 0.5;
	float tHere = yNorm;
	float3 posOnAxis = mix(startPos, endPos, tHere);

	float3 camPos = cameraViewInv[3].xyz;
	float3 toCam  = normalize(camPos - posOnAxis);
	float3 right  = cross(forward, toCam);
	float rightLen = length(right);
	if (rightLen < 0.3) {
		float3 fb = normalize(cross(forward, float3(0.0, 1.0, 0.0)));
		if (length(fb) < 0.001) fb = normalize(cross(forward, float3(1.0, 0.0, 0.0)));
		float bl = clamp(rightLen / 0.3, 0.0, 1.0);
		right = normalize(mix(fb, right / max(rightLen, 0.001), bl));
	} else {
		right = right / rightLen;
	}

	float flicker = 1.0;
	if (ENV_LIGHTNING_ANIMATE > 0.5) {
		float frameTick = floor(timeInfo.z * ANIMATE_RATE);
		flicker = 1.0 - FLICKER_AMPLITUDE * 0.5 + FLICKER_AMPLITUDE * hash11(seed * 0.97 + frameTick * 0.13);
	}
	float jitterScale = min(jitterAmp, boltLen * JITTER_MAX_BOLT_FRAC);
	float jitterRatio = clamp(jitterScale / max(boltLen, 0.001), 0.0, 1.0);
	// Strongly bent/jagged branches can diverge from the straight glow axis;
	// tighten + dim the halo there to reduce off-axis glow bleed.
	float alignment = 1.0 - smoothstep(0.02, 0.10, jitterRatio);
	float glowWidth = baseWidth * GLOW_WIDTH_MULT * flicker * mix(0.58, 1.0, alignment);

	float camDist = length(camPos - posOnAxis);
	float minWidth = camDist * GLOW_MIN_PIXEL_WIDTH;
	float coverageVal = clamp(glowWidth / max(minWidth, 0.001), 0.0, 1.0);
	glowWidth = max(glowWidth, minWidth);

	float3 vertexWorld = posOnAxis + right * cornerAndSeg.x * glowWidth;
	gl_Position = cameraViewProj * float4(vertexWorld, 1.0);

	widthPos = cornerAndSeg.x;
	lengthT  = tHere;
	glowColor = edgeColor.rgb;
	alpha = coreColor.a * flicker * coverageVal * (1.0 - LENGTH_FALLOFF * tHere) * mix(0.72, 1.0, alignment);
	glowMult = extraParams.z;
	feather  = extraParams.w;
}

};
vertex EnvironmentGlowVarying EnvironmentGlowVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){EnvironmentGlowVertexProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.cornerAndSeg=mesh[id];p.startPosAndWidth=instances[instance*6];p.endPosAndTip=instances[instance*6+1];p.coreColor=instances[instance*6+2];p.edgeColor=instances[instance*6+3];p.boltParams=instances[instance*6+4];p.extraParams=instances[instance*6+5];p.run();EnvironmentGlowVarying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;out.widthPos=p.widthPos;out.lengthT=p.lengthT;out.glowColor=p.glowColor;out.alpha=p.alpha;out.glowMult=p.glowMult;out.feather=p.feather;return out;}
struct EnvironmentGlowFragmentProgram{float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float ANIMATE_RATE;float BRIGHTNESS_MULT;float BRUSH_MAX_JITTER_FRAC;float CORE_BRIGHTNESS;float CORE_EDGE_END;float CORE_EDGE_START;float ENV_LIGHTNING_ANIMATE;float FADE_IN_END;float FADE_OUT_START;float FLICKER_AMPLITUDE;float GLOW_FALLOFF_POWER;float GLOW_MIN_PIXEL_WIDTH;float GLOW_WIDTH_MULT;float IMPACT_BRIGHTNESS;float JITTER_MAX_BOLT_FRAC;float LENGTH_FALLOFF;float MIN_PIXEL_WIDTH;float ROOT_TAPER_END;float SEGMENT_LENGTH_VAR;float THICKNESS_VARIATION;float TIP_TAPER_START;float widthPos;float lengthT;float3 glowColor;float alpha;float glowMult;float feather;float4 fragColor;





//__DEFINES__
//__ENGINEUNIFORMBUFFERDEFS__





void run()
{
	float edgeDist = abs(widthPos);
	if (edgeDist >= 1.0) discard_fragment();
	// Softer (lower power) falloff for higher feather -> wider diffuse halo.
	float power = GLOW_FALLOFF_POWER * mix(1.6, 0.5, feather);
	float g = exp(-edgeDist * edgeDist * power);
	float edgeVal = exp(-power);
	float falloff = max(g - edgeVal, 0.0) / max(1.0 - edgeVal, 0.0001);
	// Anchor glow to zero at the origin and the tip.
	float lengthEndFade = sin(clamp(lengthT, 0.0, 1.0) * 3.14159);
	falloff *= lengthEndFade;
	float3 col = glowColor * (falloff * alpha * glowMult);
	float lum = dot(col, float3(0.299, 0.587, 0.114));
	if (lum < 0.001) discard_fragment();
	fragColor = float4(col, 0.0);
}

};
fragment float4 EnvironmentGlowFragmentMain(EnvironmentGlowVarying input [[stage_in]],constant LightningUniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));EnvironmentGlowFragmentProgram p{};p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;p.ANIMATE_RATE=u.parameters[0];p.BRIGHTNESS_MULT=u.parameters[1];p.BRUSH_MAX_JITTER_FRAC=u.parameters[2];p.CORE_BRIGHTNESS=u.parameters[3];p.CORE_EDGE_END=u.parameters[4];p.CORE_EDGE_START=u.parameters[5];p.ENV_LIGHTNING_ANIMATE=u.parameters[6];p.FADE_IN_END=u.parameters[7];p.FADE_OUT_START=u.parameters[8];p.FLICKER_AMPLITUDE=u.parameters[9];p.GLOW_FALLOFF_POWER=u.parameters[10];p.GLOW_MIN_PIXEL_WIDTH=u.parameters[11];p.GLOW_WIDTH_MULT=u.parameters[12];p.IMPACT_BRIGHTNESS=u.parameters[13];p.JITTER_MAX_BOLT_FRAC=u.parameters[14];p.LENGTH_FALLOFF=u.parameters[15];p.MIN_PIXEL_WIDTH=u.parameters[16];p.ROOT_TAPER_END=u.parameters[17];p.SEGMENT_LENGTH_VAR=u.parameters[18];p.THICKNESS_VARIATION=u.parameters[19];p.TIP_TAPER_START=u.parameters[20];p.widthPos=input.widthPos;p.lengthT=input.lengthT;p.glowColor=input.glowColor;p.alpha=input.alpha;p.glowMult=input.glowMult;p.feather=input.feather;p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}
)MSL";
}
