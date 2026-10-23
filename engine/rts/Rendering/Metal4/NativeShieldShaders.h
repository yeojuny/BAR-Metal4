/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4::shield {
// Mechanical current31657 ShieldSphereColorGL4 translation. Original geometry,
// per-instance values, engine UBO time/camera and binding6 impacts are retained.
// Retains the existing finite vertical-impact basis correction.
inline constexpr const char* SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(#include <metal_stdlib>
using namespace metal;
struct ShieldVertex {packed_float3 position,normal;packed_float2 uv;};
struct ShieldInstance {float4 instPosRadius,instRotMargin,instColor1,instColor2,instParams;};
struct ShieldUniform {float4x4 view,projection;float4 cameraPosition,factors;uint4 options,raster;};
struct ShieldVarying {
 float4 position [[position]];
 float4 modelPos [[RECOIL_SAMPLE_PERSPECTIVE]],worldPos [[RECOIL_SAMPLE_PERSPECTIVE]],viewPos [[RECOIL_SAMPLE_PERSPECTIVE]];
 float colormix [[RECOIL_SAMPLE_PERSPECTIVE]],normalizedFragDepth [[RECOIL_SAMPLE_PERSPECTIVE]];
 float2 v_screenUV [[RECOIL_SAMPLE_NOPERSPECTIVE]];
 float4 v_translationScale [[flat]];
 float4 v_color1 [[flat]];
 float4 v_color2 [[flat]];
 float2 v_fadeOverlap [[flat]];
 int4 v_params [[flat]];
 float2 v_arcBreath [[flat]];
 float v_cameraInside [[flat]];
};
float ShieldMod(float x,float y){return x-y*floor(x/y);}
float2 ShieldMod(float2 x,float y){return x-y*floor(x/y);}
float3x3 ShieldMat3(float4x4 m){return float3x3(m[0].xyz,m[1].xyz,m[2].xyz);}
bool ShieldAlpha(float a,float r,uint c){switch(c){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;case 7:return true;}return false;}
float2 ShieldSampleUV(float2 uv,bool flip,uint legacyClamp){if(legacyClamp&1u)uv.x=clamp(uv.x,0.0f,1.0f);if(legacyClamp&2u)uv.y=clamp(uv.y,0.0f,1.0f);if(flip)uv.y=1.0f-uv.y;return uv;}
#define NORM2SNORM(value) (value * 2.0 - 1.0)
#define SNORM2NORM(value) (value * 0.5 + 0.5)

//Lua limitations only allow to send 24 bits. Should be enough :)
#define BITMASK_FIELD(value, pos) ((uint(value) & (1u << uint(pos))) != 0u)

float GetViewSpaceDepth(float depthNDC,constant ShieldUniform& u) {
	return -u.projection[3][2] / (u.projection[2][2] + depthNDC);
}

float4x4 CalculateLookAtMatrix(float3 eye, float3 center, float3 up) {
	float3 zaxis = normalize(center - eye); //from center towards eye vector
	// Unsynced renderer correction: the original up can be parallel to a
	// legitimate vertical impact direction. Preserve the original path whenever
	// its squared length is representable and nonzero; stabilize only the
	// original zero/underflow normalization. No hit, geometry or effect is lost.
	float3 xaxis = cross(zaxis, up);
	if (dot(xaxis, xaxis) == 0.0) {
		float scale = max(abs(xaxis.x), max(abs(xaxis.y), abs(xaxis.z)));
		if (scale > 0.0) {
			xaxis /= scale; // nonzero direction whose squared length underflowed
		} else {
			float3 alternateUp = abs(zaxis.x) <= abs(zaxis.z) ? float3(1.0, 0.0, 0.0) : float3(0.0, 0.0, 1.0);
			xaxis = cross(zaxis, alternateUp);
		}
	}
	xaxis = normalize(xaxis);
	float3 yaxis = cross(xaxis, zaxis);

	float4x4 lookAtMatrix;

	lookAtMatrix[0] = float4(xaxis.x, yaxis.x, zaxis.x, 0.0);
	lookAtMatrix[1] = float4(xaxis.y, yaxis.y, zaxis.y, 0.0);
	lookAtMatrix[2] = float4(xaxis.z, yaxis.z, zaxis.z, 0.0);
	lookAtMatrix[3] = float4(dot(xaxis, -eye), dot(yaxis, -eye), dot(zaxis, -eye), 1.0);

	return lookAtMatrix;
}

float4x4 CalculateLookAtMatrix(float3 eye, float3 center, float roll) {
	return CalculateLookAtMatrix(eye, center, float3(sin(roll), cos(roll), 0.0));
}

float Value3D( float3 P ) {
	//  https://github.com/BrianSharpe/Wombat/blob/master/Value3D.glsl

	// establish our grid cell and unit position
	float3 Pi = floor(P);
	float3 Pf = P - Pi;
	float3 Pf_min1 = Pf - 1.0;

	// clamp the domain
	Pi.xyz = Pi.xyz - floor(Pi.xyz * ( 1.0 / 69.0 )) * 69.0;
	float3 Pi_inc1 = step( Pi, float3( 69.0 - 1.5 ) ) * ( Pi + 1.0 );

	// calculate the hash
	float4 Pt = float4( Pi.xy, Pi_inc1.xy ) + float2( 50.0, 161.0 ).xyxy;
	Pt *= Pt;
	Pt = Pt.xzxz * Pt.yyww;
	float2 hash_mod = float2( 1.0 / ( 635.298681 + float2( Pi.z, Pi_inc1.z ) * 48.500388 ) );
	float4 hash_lowz = fract( Pt * hash_mod.xxxx );
	float4 hash_highz = fract( Pt * hash_mod.yyyy );

	//	blend the results and return
	float3 blend = Pf * Pf * Pf * (Pf * (Pf * 6.0 - 15.0) + 10.0);
	float4 res0 = mix( hash_lowz, hash_highz, blend.z );
	float4 blend2 = float4( blend.xy, float2( 1.0 - blend.xy ) );
	return dot( res0, blend2.zxzx * blend2.wwyy );
}

float SimplexPerlin3D( float3 P ) {
    //  https://github.com/BrianSharpe/Wombat/blob/master/SimplexPerlin3D.glsl

    //  simplex math constants
    const float SKEWFACTOR = 1.0/3.0;
    const float UNSKEWFACTOR = 1.0/6.0;
    const float SIMPLEX_CORNER_POS = 0.5;
    const float SIMPLEX_TETRAHADRON_HEIGHT = 0.70710678118654752440084436210485;    // sqrt( 0.5 )

    //  establish our grid cell.
    P *= SIMPLEX_TETRAHADRON_HEIGHT;    // scale space so we can have an approx feature size of 1.0
    float3 Pi = floor( P + dot( P, float3( SKEWFACTOR) ) );

    //  Find the vectors to the corners of our simplex tetrahedron
    float3 x0 = P - Pi + dot(Pi, float3( UNSKEWFACTOR ) );
    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 Pi_1 = min( g.xyz, l.zxy );
    float3 Pi_2 = max( g.xyz, l.zxy );
    float3 x1 = x0 - Pi_1 + UNSKEWFACTOR;
    float3 x2 = x0 - Pi_2 + SKEWFACTOR;
    float3 x3 = x0 - SIMPLEX_CORNER_POS;

    //  pack them into a parallel-friendly arrangement
    float4 v1234_x = float4( x0.x, x1.x, x2.x, x3.x );
    float4 v1234_y = float4( x0.y, x1.y, x2.y, x3.y );
    float4 v1234_z = float4( x0.z, x1.z, x2.z, x3.z );

    // clamp the domain of our grid cell
    Pi.xyz = Pi.xyz - floor(Pi.xyz * ( 1.0 / 69.0 )) * 69.0;
    float3 Pi_inc1 = step( Pi, float3( 69.0 - 1.5 ) ) * ( Pi + 1.0 );

    //	generate the random vectors
    float4 Pt = float4( Pi.xy, Pi_inc1.xy ) + float2( 50.0, 161.0 ).xyxy;
    Pt *= Pt;
    float4 V1xy_V2xy = mix( Pt.xyxy, Pt.zwzw, float4( Pi_1.xy, Pi_2.xy ) );
    Pt = float4( Pt.x, V1xy_V2xy.xz, Pt.z ) * float4( Pt.y, V1xy_V2xy.yw, Pt.w );
    const float3 SOMELARGEFLOATS = float3( 635.298681, 682.357502, 668.926525 );
    const float3 ZINC = float3( 48.500388, 65.294118, 63.934599 );
    float3 lowz_mods = float3( 1.0 / ( SOMELARGEFLOATS.xyz + Pi.zzz * ZINC.xyz ) );
    float3 highz_mods = float3( 1.0 / ( SOMELARGEFLOATS.xyz + Pi_inc1.zzz * ZINC.xyz ) );
    Pi_1 = ( Pi_1.z < 0.5 ) ? lowz_mods : highz_mods;
    Pi_2 = ( Pi_2.z < 0.5 ) ? lowz_mods : highz_mods;
    float4 hash_0 = fract( Pt * float4( lowz_mods.x, Pi_1.x, Pi_2.x, highz_mods.x ) ) - 0.49999;
    float4 hash_1 = fract( Pt * float4( lowz_mods.y, Pi_1.y, Pi_2.y, highz_mods.y ) ) - 0.49999;
    float4 hash_2 = fract( Pt * float4( lowz_mods.z, Pi_1.z, Pi_2.z, highz_mods.z ) ) - 0.49999;

    //	evaluate gradients
    float4 grad_results = rsqrt( hash_0 * hash_0 + hash_1 * hash_1 + hash_2 * hash_2 ) * ( hash_0 * v1234_x + hash_1 * v1234_y + hash_2 * v1234_z );

    //	Normalization factor to scale the final result to a strict 1.0->-1.0 range
    //	http://briansharpe.wordpress.com/2012/01/13/simplex-noise/#comment-36
    const float FINAL_NORMALIZATION = 37.837227241611314102871574478976;

    //  evaluate the kernel weights ( use (0.5-x*x)^3 instead of (0.6-x*x)^4 to fix discontinuities )
    float4 kernel_weights = v1234_x * v1234_x + v1234_y * v1234_y + v1234_z * v1234_z;
    kernel_weights = max(0.5 - kernel_weights, 0.0);
    kernel_weights = kernel_weights*kernel_weights*kernel_weights;

    //	sum with the kernel and return
    return dot( kernel_weights, grad_results ) * FINAL_NORMALIZATION;
}

float StrangeSurface(float3 snormPos, float mult, float time) {
	const float MAX_ITER = 1.5;

	float2 p = snormPos.xz*(1.5 - abs(snormPos.y)) * mult;

	float2 i = p;
	float c = 0.0;
	float inten = 0.07;
	float r = length(p + float2(sin(time), sin(time * 0.433 + 2.0)) * 3.0);

	for (float n = 0.0; n < MAX_ITER; n++) {
		float t = r - time * (1.0 - (1.9 / (n + 1.0)));
		      t = r - time / (n + 0.6);
		i -= p + float2(
			cos(t - i.x - r) + sin(t + i.y),
			sin(t - i.y) + cos(t + i.x) + r
		);
		c += 1.0/length(float2(
			(sin(i.x + t) / inten),
			(cos(i.y + t) / inten)
			)
		);

	}

	c /= MAX_ITER;
	c = clamp(c, -1.0, 1.0);
	return c;
}

float Hexagon2D(float2 p, float edge0, float edge1) {
	p.x *= 0.57735 * 2.0;
	p.y += ShieldMod(floor(p.x), 2.0) * 0.5;
	p = abs((ShieldMod(p, 1.0) - 0.5));
	float val = abs(max(p.x * 1.5 + p.y, p.y * 2.0) - 1.0);
	return smoothstep(edge0, edge1, val);
}

float2 GetRippleOffset(float3 thisPoint, float3 impactPoint, float magMult, float gameFrame) {
	float2 dir = thisPoint.xy - impactPoint.xy;
	float dist = dot(thisPoint, impactPoint);
	float2 offset = dir * SNORM2NORM( sin(-dist * 1024.0 + gameFrame * 0.5) ) * 1.0 * magMult;
	return offset;
}

constant float3x3 RGB2YCBCR = float3x3(float3(0.2126,-0.114572,0.5),float3(0.7152,-0.385428,-0.454153),float3(0.0722,0.5,-0.0458471));

constant float3x3 YCBCR2RGB = float3x3(float3(1.0,1.0,1.0),float3(0.0,-0.187324,1.8556),float3(1.5748,-0.468124,-5.55112e-17));

constant float PI = 3.14159265358979323846f;


float3 RotateY(float3 p, float angle) {
	float c = cos(angle);
	float s = sin(angle);

	//{x Cos[ang] + z Sin[ang], y, z Cos[ang] - x Sin[ang]}
	return float3(p.x * c + p.z * s, p.y, p.z * c - p.x * s);
}


vertex ShieldVarying ShieldVertexMain(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],device const ShieldVertex* vertices [[buffer(0)]],constant ShieldUniform& u [[buffer(1)]],device const ShieldInstance* instances [[buffer(2)]])
{
 ShieldVarying out{};
 const ShieldInstance instance=instances[instanceId];
	float gameFrame = u.factors.x;
	int effects = int(instance.instParams.x + 0.5);

	out.v_translationScale = instance.instPosRadius;
	out.v_color1 = instance.instColor1;
	out.v_color2 = instance.instColor2;
	out.v_fadeOverlap = instance.instRotMargin.zw;
	out.v_params = int4(effects, int(instance.instParams.y + 0.5), int(instance.instParams.z + 0.5), int(instance.instParams.w + 0.5));

	// Idle-field terms that are constant across the whole shield: evaluate
	// them once per vertex instead of once per fragment.
	{
		const float ARC_BURST_FREQ = 0.013; // arc-flash frequency (per frame)
		const float BREATH_SPEED   = 0.018; // overall pulse speed

		float arcTime   = gameFrame * ARC_BURST_FREQ;
		float arcBucket = floor(arcTime);
		float arcFrac   = fract(arcTime);
		float arcSeedA  = Value3D(float3(arcBucket,       instance.instPosRadius.x * 0.07, instance.instPosRadius.z * 0.11));
		float arcSeedB  = Value3D(float3(arcBucket + 1.0, instance.instPosRadius.x * 0.07, instance.instPosRadius.z * 0.11));
		float arcSeed   = mix(arcSeedA, arcSeedB, smoothstep(0.0, 1.0, arcFrac));
		// Half-sine envelope across the bucket so even at peak seed the burst
		// fades in/out rather than ending abruptly.
		float arcEnvelope = sin(arcFrac * PI);
		out.v_arcBreath.x = smoothstep(0.78, 0.92, arcSeed) * arcEnvelope;

		// Slow breathing brightness modulation so idle shields feel alive
		out.v_arcBreath.y = 0.85 + 0.15 * SNORM2NORM(sin(gameFrame * BREATH_SPEED + instance.instPosRadius.x * 0.13));
	}

	// When the camera is inside the sphere only the back faces are visible,
	// so they must get the full shading path (see fragment shader).
	float3 cameraPos = u.cameraPosition.xyz;
	out.v_cameraInside = (distance(cameraPos, instance.instPosRadius.xyz) < instance.instPosRadius.w * 1.02) ? 1.0 : 0.0;

	out.modelPos = float4(vertices[vertexId].position, 1.0);

	if (BITMASK_FIELD(effects, 6)) {
		float r = length(out.modelPos.xyz);
		float theta = acos(out.modelPos.z / r);
		float phi = atan2(out.modelPos.y, out.modelPos.x);
		r += 0.010 * r * SNORM2NORM(sin( (2.0 * theta + instance.instPosRadius.z  * 13.0 + 3.3 * cos(phi + instance.instPosRadius.x * 17.0)) * 8.0 + gameFrame * 0.05));
		out.modelPos.xyz = float3(r * sin(theta) * cos(phi), r * sin(theta) * sin(phi), r * cos(theta));
	}

	out.worldPos = float4(instance.instPosRadius.www * out.modelPos.xyz, 1.0);	//scaling
	out.worldPos.xyz  = RotateY(out.worldPos.xyz, instance.instRotMargin.x);	//rotation around Yaw axis
	out.worldPos.xyz += instance.instPosRadius.xyz;						//translation in world space

	out.viewPos = u.view * out.worldPos;

	float3 worldNormal = normalize(RotateY(out.modelPos.xyz, instance.instRotMargin.x));
	float3 viewNormal = ShieldMat3(u.view) * worldNormal;

	out.colormix = dot(viewNormal, normalize(out.viewPos.xyz));
	out.colormix = pow(abs(out.colormix), instance.instRotMargin.y);

	float2 nearFar = u.projection[3][2] / float2(u.projection[2][2] - 1.0, u.projection[2][2] + 1.0);
	out.normalizedFragDepth = (-out.viewPos.z - nearFar.x) / (nearFar.y - nearFar.x);
	out.normalizedFragDepth = clamp(out.normalizedFragDepth, 0.0, 1.0);

	out.position = u.projection * out.viewPos;
	out.v_screenUV = SNORM2NORM(out.position.xy / out.position.w);

 if(!u.options.z)out.position.z=(out.position.z+out.position.w)*0.5;
 return out;
}
// HasImpacts=false is selected only when the original leaves binding6 unbound;
// there is no dummy storage allocation and no storage read in that variant.
template<bool HasImpacts>
float4 ShieldShade(ShieldVarying in,bool frontFacing,constant ShieldUniform& u,device const float4* impacts,float sampledDepth)
{
	float gameFrame = u.factors.x;
	int effects = in.v_params.x;
	float4 color1 = in.v_color1;
	float4 color2 = in.v_color2;
	float4 translationScale = in.v_translationScale;
	float shieldFade = in.v_fadeOverlap.x;
	float overlapScale = in.v_fadeOverlap.y;

	// A shield sphere is drawn double sided. Seen from outside, the back face is
	// a second, nearly identical layer under the front face: it only adds the
	// localized effects (contact outline, impacts) and its share of the rim is
	// folded into the front face's alpha instead. With the camera inside the
	// sphere only back faces exist, so they take the full path.
	bool cameraInside = in.v_cameraInside > 0.5;
	bool fullPath = frontFacing || cameraInside;

	float4 color;
	color = mix(color1, color2, in.colormix);
	if (!fullPath) {
		color.a = 0.0; // body layer is only drawn once (front face)
	}

	// --- Contact outline: cheap depth proximity test first -----------------
	const float outlineEffectSize = 30.0;
	const float outlineAlpha = 0.45;
	// The outline factor is smoothstep(0, depthDiff, size * noise): it has a
	// long soft tail. In the tail the noise is frozen at its mean, so the glow
	// is negligible (< 0.5%) beyond this distance and the tail costs no noise.
	const float OUTLINE_TAIL_RANGE = 300.0;
	const float OUTLINE_TAIL_NOISE = 0.5;
	// The animated crackle is only worth its cost in the bright core of the
	// band; it fades out between these two distances so no seam is visible.
	const float OUTLINE_CRACKLE_FADE_START = 30.0;
	const float OUTLINE_CRACKLE_FADE_END = 90.0;
	bool doOutline = BITMASK_FIELD(effects, 1) || BITMASK_FIELD(effects, 2);
	float depthDiff = 1.0e9;
	if (doOutline) {
		float minDepth = sampledDepth;
		if (!u.options.z) minDepth = NORM2SNORM(minDepth);

		float minDepthView = GetViewSpaceDepth( minDepth, u );
		depthDiff = abs(in.viewPos.z - minDepthView);
	}
	bool contactCore = doOutline && (depthDiff < OUTLINE_CRACKLE_FADE_END); // bright part of the band
	// The faint tail is only drawn on the front face; the back face's tail sits
	// under the front face's fill and is not worth keeping those fragments alive.
	bool nearContact = contactCore || (doOutline && fullPath && (depthDiff < OUTLINE_TAIL_RANGE));

	// --- Rim mask: also gates the (expensive) idle field ---------------------
	const float RIM_SHARPNESS = 1.5;   // higher = thinner edge band
	const float RIM_EPS = 0.005;       // below this the rim alpha is invisible
	float rim = 1.0 - clamp(in.colormix, 0.0, 1.0);
	rim = pow(rim, RIM_SHARPNESS);
	bool doRim = fullPath && (rim > RIM_EPS);

	int impactCount = BITMASK_FIELD(effects, 3) ? in.v_params.z : 0;

	// Back face away from any contact and without impacts: nothing to draw.
	if (!fullPath && !nearContact && impactCount == 0) {
		discard_fragment();
	}

	const float valueNoiseMovePace = 0.25;
	float valueNoise = 0.0;
	if (contactCore || impactCount > 0) {
		float3 valueNoiseVec = in.modelPos.xyz * translationScale.www;
		valueNoiseVec.y += gameFrame * valueNoiseMovePace;
		valueNoise = Value3D(valueNoiseVec);
	}

	// Body colour feeds the outline, so back faces need it only in the bright core.
	if (BITMASK_FIELD(effects, 6) && (fullPath || contactCore)) {
		const float perlinNoiseMovePace = 0.0025;
		float waveFront = ShieldMod(-gameFrame * 0.0020, 0.5);

		float3 perlinNoiseVec = in.modelPos.xyz;
		//perlinNoiseVec.y += gameFrame * perlinNoiseMovePace;

		//float perlin = 0.33 * abs(SimplexPerlin3D(perlinNoiseVec * 50.0)) + 0.5 * abs(SimplexPerlin3D(perlinNoiseVec * 0.5));
		float perlin = 5 * abs(StrangeSurface(in.modelPos.xyz, 4.0, gameFrame * perlinNoiseMovePace));

		float band = SNORM2NORM(cos((in.modelPos.y - waveFront) * PI * 4.0));

		float pb = pow( clamp(perlin * band, 0.0, 0.8), 0.25 );

		color = pow(color, float4(1.3 - pb));
	}

	if (nearContact) {
		// Noise modulates the band width in the core; blend to its mean towards
		// the tail so the tail needs no noise evaluation and stays seamless.
		float outlineNoise = mix(valueNoise, OUTLINE_TAIL_NOISE, smoothstep(OUTLINE_CRACKLE_FADE_START, OUTLINE_CRACKLE_FADE_END, depthDiff));
		float outlineFactor = smoothstep( 0.0, depthDiff, outlineEffectSize * outlineNoise );
		outlineFactor *= mix(0.25, 1.0, SNORM2NORM(sin(0.1*gameFrame + 5.0*(in.modelPos.x + in.modelPos.z +  in.modelPos.y))));

		// Animated crackle on the outline edge so where the shield meets terrain/units
		// it shimmers like a contact arc rather than a static halo. Only evaluated
		// in the bright core of the band, fading out so the tail stays seamless.
		if (contactCore) {
			const float OUTLINE_CRACKLE_SCALE = 130.0;  // noise frequency along the edge
			const float OUTLINE_CRACKLE_SPEED = 0.110; // scroll speed
			const float OUTLINE_CRACKLE_AMOUNT = 0.25; // modulation depth (0 = off, 1 = full flicker)
			float crackleAmount = OUTLINE_CRACKLE_AMOUNT * (1.0 - smoothstep(OUTLINE_CRACKLE_FADE_START, OUTLINE_CRACKLE_FADE_END, depthDiff));
			float3 outlineCrackleP = in.modelPos.xyz * OUTLINE_CRACKLE_SCALE;
			outlineCrackleP.y -= gameFrame * OUTLINE_CRACKLE_SPEED;
			outlineCrackleP.x += gameFrame * OUTLINE_CRACKLE_SPEED * 0.37;
			float outlineCrackle = SNORM2NORM(SimplexPerlin3D(outlineCrackleP));
			outlineFactor *= mix(1.0 - crackleAmount, 1.0 + crackleAmount, outlineCrackle);
		}
		outlineFactor = clamp(outlineFactor, 0.0, 1.0);

		// Scale by shieldFade so outline alpha fades with the rest of the shield
		color.a = mix(color.a, outlineAlpha * shieldFade, outlineFactor);
	}

	if (HasImpacts && impactCount > 0) { // actual bound impact animation
		const float4 impactColor = float4(0.35);

		float3 worldVec = normalize(in.worldPos.xyz - translationScale.xyz);

		float2 cameraDistanceFactors = float2(1.0);
		if (BITMASK_FIELD(effects, 7)) { // impactScaleWithDistance
			cameraDistanceFactors = float2(1.0 + 1.0 * in.normalizedFragDepth, 1.0 + 6.0 * in.normalizedFragDepth);
		}

		float4 impactFactor = float4(0.0);
		int impactBase = in.v_params.y;
		for (int i = 0; i < impactCount; ++i) {
			float4 impactInfo = impacts[impactBase + i];
			float3 worldImpactVec = normalize(impactInfo.xyz);
			float angleDist = acos( dot(worldVec, worldImpactVec) );
			float impactRadius = impactInfo.w * cameraDistanceFactors.x;
			if (angleDist >= impactRadius) {
				continue; // outside this impact's footprint: its factor would be 0
			}
			float3 thisImpactFactor = float3(smoothstep( impactRadius, 0.0, angleDist ));

			float centerFactor = pow(thisImpactFactor.r, 6.0 / cameraDistanceFactors.y);

			//-- swirl a bit
			float impactRoll = 0.35 * PI * centerFactor;
			impactRoll *= float(BITMASK_FIELD(effects, 5));

			float4x4 worldImpactMat = CalculateLookAtMatrix(worldImpactVec, float3(0.0), impactRoll);
			float3 impactNoiseVec = ShieldMat3(worldImpactMat) * worldVec;

			if (BITMASK_FIELD(effects, 8)) { // impactRipples
				float2 rippleOffset = GetRippleOffset(impactNoiseVec, float3(0.0, 0.0, 1.0), thisImpactFactor.r, gameFrame);
				impactNoiseVec.xy += rippleOffset;
				thisImpactFactor *= 1.0 + length(rippleOffset) * 1.0;
			}

			impactNoiseVec *= 36.0 / cameraDistanceFactors.x;

			float2 noiseVecOffset = 0.8 * centerFactor * float2( sin(gameFrame * PI * 0.15), cos(gameFrame * PI * 0.15) );
			noiseVecOffset *= float2(BITMASK_FIELD(effects, 4));

			// Make chromatic aberration effect around the impact point
			thisImpactFactor.r *= Hexagon2D(impactNoiseVec.xy + float2(noiseVecOffset.x, noiseVecOffset.y), 0.2, 0.2);
			thisImpactFactor.g *= Hexagon2D(impactNoiseVec.xy + float2(-noiseVecOffset.x, -noiseVecOffset.y), 0.2, 0.2);
			thisImpactFactor.b *= Hexagon2D(impactNoiseVec.xy + float2(noiseVecOffset.x * noiseVecOffset.y, 0.0), 0.2, 0.2);

			thisImpactFactor *= mix(0.6, 1.0, valueNoise);
			impactFactor.rgb += thisImpactFactor.rgb;
			impactFactor.a = 0.333333 * (impactFactor.r + impactFactor.g + impactFactor.b);
		}

		// Scale by shieldFade so impact flashes fade with the shield
		color += impactColor * impactFactor * shieldFade;
	}

	// --- Idle rim energy field ----------------------------------------------
	// Always-on enhancement that gives shields a visible, animated silhouette
	// without obstructing the units inside. Fresnel acts as the master mask:
	// rim ~= 0 head-on, ~= 1 at the silhouette, so the interior stays clear
	// even when many shields overlap.
	float3  rimHotBoost  = float3(0.0); // applied AFTER tonemap to keep saturation
	float rimHotAlpha  = 0.0;
	if (doRim) {
		const float RIM_ALPHA      = 0.45;  // peak alpha contribution at silhouette
		const float RIM_COLOR_GAIN = 2.2;   // how much rim brightens (lower = more saturated)
		const float HEX_SCALE_U    = 1.6;   // hex pattern density around belly
		const float HEX_SCALE_V    = 3.0;   // hex pattern density vertically
		const float HEX_DRIFT_U    = 0.0014;// horizontal drift speed
		const float HEX_DRIFT_V    = 0.0006;// vertical drift speed
		const float SWEEP_FREQ     = 5.0;   // vertical scanline density (higher = more bands on the shield at once)
		const float SWEEP_SPEED    = 0.040; // scanline upward speed (animation speed of each band)
		const float SWEEP_SHARP    = 5.0;   // shape exponent (2 = smooth breathe, 5 = narrow peaks)
		const float CRACKLE_SCALE  = 100.0;  // micro-noise frequency on rim
		const float CRACKLE_SPEED  = 0.170; // micro-noise scroll speed
		const float CRACKLE_AMOUNT = 0.44;  // crackle modulation strength
		const float HEX_FIRE_PROB  = 0.18;  // fraction of "charged" hex cells
		const float HEX_FIRE_GAIN  = 1.8;   // brightness boost on charged cells
		const float ARC_BURST_GAIN = 2.2;   // arc-flash brightness peak
		const float CHROMA_SPLIT   = 0.5;  // cyan-positive split at extreme rim
		const float HOT_ESCAPE     = 0.85;  // fraction of rim color to keep post-tonemap

		// Hex energy cells drifting around the sphere; spherical UV from in.modelPos
		float2 hexUV;
		hexUV.x = atan2(in.modelPos.x, in.modelPos.z) * (1.0 / PI) * HEX_SCALE_U;
		hexUV.y = in.modelPos.y * HEX_SCALE_V;
		hexUV  += float2(gameFrame * HEX_DRIFT_U, gameFrame * HEX_DRIFT_V);
		// Hexagon2D returns 1 in gaps, 0 inside cells; we want lit cells -> invert
		float hex = 1.0 - Hexagon2D(hexUV, 0.30, 0.55);

		// Per-cell randomization: use floor(hexUV) as cell id, hash with Value3D
		// to pick which cells "fire" brighter. Time is the third noise axis, so
		// Value3D's own fade curve cross-fades between adjacent time buckets and
		// cells charge/discharge smoothly with a single noise evaluation.
		float cellTime  = gameFrame * 0.020;
		float cellHash = Value3D(float3(floor(hexUV * 1.7), cellTime));
		float cellFire = smoothstep(1.0 - HEX_FIRE_PROB, 1.0, cellHash);
		hex *= mix(1.0, HEX_FIRE_GAIN, cellFire);

		// Vertical scanline sweep traveling up the shield. Use pow(sin, 2) for
		// a smooth breathe shape that still ranges 0..1 (so shield body
		// stays visible between bands). Lower SWEEP_SHARP for wider/softer
		// bands; higher for narrower peaks.
		float sweepRaw = SNORM2NORM(sin(in.modelPos.y * SWEEP_FREQ - gameFrame * SWEEP_SPEED));
		float sweep = pow(sweepRaw, SWEEP_SHARP);

		// Occasional arc-burst: a brighter, faster sweep band that swells
		// irregularly. The gate (randomness + envelope) is per shield and comes
		// from the vertex shader.
		float arcGate   = in.v_arcBreath.x;
		float arcLocal  = SNORM2NORM(sin(in.modelPos.y * SWEEP_FREQ * 2.2 - gameFrame * SWEEP_SPEED * 5.0));
		arcLocal = pow(arcLocal, SWEEP_SHARP);
		float arc = arcLocal * arcGate * ARC_BURST_GAIN;

		// Slow breathing brightness modulation so idle shields feel alive (per shield, from VS)
		float breath = in.v_arcBreath.y;

		// High-freq crackle noise modulating only the rim alpha for that
		// "containment field" feel (no color contribution -> stays cheap).
		// Value noise (already 0..1) instead of simplex: roughly a third of the cost.
		float3 crackleP = in.modelPos.xyz * CRACKLE_SCALE;
		crackleP.y -= gameFrame * CRACKLE_SPEED;
		float crackle = Value3D(crackleP);
		float crackleMod = mix(1.0 - CRACKLE_AMOUNT, 1.0 + CRACKLE_AMOUNT, crackle);

		// Combine: rim is the master mask, hex/sweep/arc are texture, breath modulates
		float idle = rim * (0.45 + 0.25 * hex + 0.75 * sweep + arc) * breath;

		// Rim color follows shield charge state. We derive a "warmness" from
		// color1 itself (high R, low B = damaged orange/red), and blend the
		// rim target between cool teal (healthy) and a hot orange (damaged).
		// This way the rim still announces low-charge urgency.
		const float3 RIM_COOL_COLOR = float3(0.10, 0.95, 1.20); // teal/cyan, healthy
		const float3 RIM_SCAV_COLOR = float3(0.90, 0.30, 1.30); // magenta/purple, healthy scavenger shield
		const float3 RIM_WARM_COLOR = float3(1.40, 0.45, 0.10); // orange/red, damaged
		bool scavenger = (in.v_params.w & 1) != 0;
		float warmness = clamp(color1.r - color1.b * 0.8, 0.0, 1.0);
		float3 rimTarget = mix(scavenger ? RIM_SCAV_COLOR : RIM_COOL_COLOR, RIM_WARM_COLOR, warmness);
		float3 rimTint   = mix(color1.rgb * 0.5, rimTarget, pow(rim, 0.6));

		// Chromatic dispersion at the extreme silhouette: bias toward the
		// rim target's dominant channel so the brightest hot edge keeps its
		// hue (cool when healthy, warm when damaged) instead of clipping.
		float chromaMask = pow(rim, 4.0);
		float3 coolChromaDir = scavenger ? float3(0.5, -0.6, 1.0) : float3(-1.0, 0.4, 1.0);
		float3 chromaDir   = mix(coolChromaDir, float3(1.0, -0.2, -0.8), warmness);
		float3 chromaSplit = chromaDir * CHROMA_SPLIT * chromaMask * idle;

		float3 rimColor = rimTint * idle * RIM_COLOR_GAIN + chromaSplit;
		float rimA    = idle * RIM_ALPHA * crackleMod;

		// Apply global fade so rim disappears when shield turns off / depletes
		rimColor *= shieldFade;
		rimA     *= shieldFade;

		// Replace (lerp) rather than add: additive + tonemap clamp was making
		// both G and B channels clip to 1.0 and the rim went white. By lerping
		// toward rimTint weighted by idle, the silhouette adopts the rim hue.
		float replaceWeight = clamp(idle * 1.2, 0.0, 1.0) * shieldFade;
		color.rgb = mix(color.rgb, rimTint, replaceWeight);
		// Small additive contribution for "hot" brightness, limited so it
		// does not push channels into the white clamp.
		float3 hotAdd = rimTint * idle * (RIM_COLOR_GAIN - 1.0) * shieldFade + chromaSplit;
		color.rgb += hotAdd * (1.0 - HOT_ESCAPE) * 0.4;

		// Post-tonemap: add the remaining hot contribution so saturated hue
		// survives the YCbCr luma clamp.
		rimHotBoost = hotAdd * HOT_ESCAPE * 0.4;
		rimHotAlpha = rimA * 0.35;

		// Seen from outside, the back face used to draw the same rim as a
		// second blended layer. Fold that layer in: a + a * (1 - a).
		if (!cameraInside) {
			rimA        = rimA * (2.0 - rimA);
			rimHotAlpha = rimHotAlpha * (2.0 - rimHotAlpha);
		}
		color.a += rimA;
	}

	//poor man's tonemapping ahead
	const float maxLuma = 0.8;
	float3 ycbcrColor = RGB2YCBCR * color.rgb;
	ycbcrColor.x = min(ycbcrColor.x, maxLuma);
	color.rgb = YCBCR2RGB * ycbcrColor;

	// Apply rim hot-boost AFTER tonemap so the silhouette keeps its color
	color.rgb += rimHotBoost;
	color.a   += rimHotAlpha;

	const float maxAlpha = 0.6;
	color.a = min(color.a, maxAlpha);

	// Final safeguard: multiply by shieldFade so the shield can never leave
	// residual opacity behind when it fades out (covers any contribution that
	// doesn't individually scale with shieldFade).
	color.a *= shieldFade;

	// Overlap dimming: when many shields stack on the same screen volume the
	// scene becomes unreadable. Each shield is given a per-frame opacity
	// scalar by the gadget based on how many other shields it overlaps and
	// whether it sits in front of or behind them. We dim only alpha so the
	// rim/glow color stays correct and shields just become more transparent.
	color.a *= overlapScale;

	if (u.raster.x && !ShieldAlpha(color.a,u.factors.w,u.raster.y)) discard_fragment();
	return color;

}
fragment float4 ShieldFragment0(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldFragment0Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldFragment2(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldFragment2Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory2_1(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory2_1Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory2_2(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory2_2Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory2_3(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory2_3Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldFragment4(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth1 [[texture(1)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldFragment4Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth1 [[texture(1)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory4_4(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory4_4Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory4_8(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory4_8Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory4_12(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory4_12Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldFragment6(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],depth2d<float> depth1 [[texture(1)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldFragment6Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],depth2d<float> depth1 [[texture(1)]],sampler depthSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(depthSampler,float2(in.v_screenUV.x,1.0-in.v_screenUV.y),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_5(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_5Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_9(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_9Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_13(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_13Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)));
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_6(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_6Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_10(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_10Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_14(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_14Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth0 [[texture(0)]],sampler sampler0 [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,depth0.sample(sampler0,ShieldSampleUV(in.v_screenUV,(u.raster.z&1u)!=0u,(u.raster.w>>0)&3u),level(0.0)).r);
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_7(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_7Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],depth2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)));
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_11(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_11Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]],texture2d<float> depth1 [[texture(1)]],sampler sampler1 [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,depth1.sample(sampler1,ShieldSampleUV(in.v_screenUV,(u.raster.z&2u)!=0u,(u.raster.w>>2)&3u),level(0.0)).r);
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
fragment float4 ShieldHistory6_15(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<false>(in,frontFacing,u,nullptr,minDepth);
}
fragment float4 ShieldHistory6_15Impacts(ShieldVarying in [[stage_in]],bool frontFacing [[front_facing]],constant ShieldUniform& u [[buffer(1)]],device const float4* impacts [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float minDepth=1.0;
 if(BITMASK_FIELD(in.v_params.x,1))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 if(BITMASK_FIELD(in.v_params.x,2))minDepth=min(minDepth,0.0f); // actual incomplete sampler red value
 return ShieldShade<true>(in,frontFacing,u,impacts,minDepth);
}
)MSL";
} // namespace rendering::metal4::shield
