/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* OrbMSL=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"ORB(
struct OrbVertex {packed_float4 pos;packed_float3 normal;packed_float2 uv;};
struct OrbInstance {float4 posrad,parameters,color1,color2;uint4 instData;};
struct OrbPublished {Model model;float4 team;};
static_assert(sizeof(OrbVertex)==36 && sizeof(OrbInstance)==80 && sizeof(OrbPublished)==144,"Original Orb ABI");
struct OrbOut {float4 position [[position]],color [[RECOIL_SAMPLE_PERSPECTIVE]];float unitID [[RECOIL_SAMPLE_PERSPECTIVE]];float gameFrame [[flat]];int technique [[flat]];float opacity [[RECOIL_SAMPLE_PERSPECTIVE]];float4 modelPos [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex OrbOut OrbVertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const OrbVertex* vertices [[buffer(0)]],device const OrbInstance* instances [[buffer(1)]],device const OrbPublished* pubs [[buffer(2)]],constant Uniform& u [[buffer(3)]]){
 const OrbInstance s=instances[instance];const OrbVertex v=vertices[id];const OrbPublished pub=pubs[instance];OrbOut out={};out.position=float4(2,2,2,1);if((pub.model.metadata.x&3u)<1u)return out;
 float3 worldPos=pub.model.drawPos.xyz;out.unitID=.1+float(pub.model.metadata.x>>16)/256000.0;float rot=pub.model.drawPos.w;float sn=sin(rot),cs=cos(rot);float3x3 rotation=float3x3(float3(cs,0,-sn),float3(0,1,0),float3(sn,0,cs));
 float3 pos=float3(v.pos.xyz);float radius=.99*s.posrad.w;radius*=1.0-exp(-.10*((u.time.x+u.time.w)-s.parameters.x));radius+=sin(u.time.z)-1.0;
 float3 world=rotation*(float3(1,-1,1)*pos.xzy*radius+s.posrad.xyz)+worldPos;
 out.position=(u.options.x?u.reflectionVP:u.vp)*float4(world,1);if(!u.options.y)out.position.z=(out.position.z+out.position.w)*.5;
 float3 normal=rotation*float3(v.normal).xzy;normal.y*=-1;float3 toCamera=normalize(u.viewInv[3].xyz-world);out.opacity=pow(abs(dot(normal,toCamera)),1.0);
 float4 color1=s.color1,color2=s.color2;if(s.color1.r<0){color1=float4(pub.team.rgb*.45+.3,.5);color2=float4(pub.team.rgb*.5,.66);}out.color=mix(color1,color2,out.opacity);
 if(s.parameters.z>.5){float shield=clamp(pub.model.user[0].z,0.0,1.0);out.color=mix(float4(.8,.4,0,1),out.color,shield);}
 out.modelPos=float4(pos.xzy*s.posrad.w,clamp(pub.model.health.y/pub.model.health.x,0.0,1.0));out.gameFrame=u.time.z;out.technique=int(floor(s.parameters.w));return out;
}
struct OrbContext {OrbOut input;float time,reflection;};
	#define PI (acos(0.0) * 2.0)

	float Orb_hash13(thread OrbContext& ctx, float3 p3) {
		const float HASHSCALE1 = 44.38975;
		p3  = fract(p3 * HASHSCALE1);
		p3 += dot(p3, p3.yzx + 19.19);
		return fract((p3.x + p3.y) * p3.z);
	}

	float Orb_noise12(thread OrbContext& ctx, float2 p){
		float2 ij = floor(p);
		float2 xy = fract(p);
		xy = 3.0 * xy * xy - 2.0 * xy * xy * xy;
		//xy = 0.5 * (1.0 - cos(PI * xy));
		float a = Orb_hash13(ctx, float3(ij + float2(0.0, 0.0), ctx.input.unitID));
		float b = Orb_hash13(ctx, float3(ij + float2(1.0, 0.0), ctx.input.unitID));
		float c = Orb_hash13(ctx, float3(ij + float2(0.0, 1.0), ctx.input.unitID));
		float d = Orb_hash13(ctx, float3(ij + float2(1.0, 1.0), ctx.input.unitID));
		float x1 = mix(a, b, xy.x);
		float x2 = mix(c, d, xy.x);
		return mix(x1, x2, xy.y);
	}

	float Orb_noise13(thread OrbContext& ctx,  float3 P ) {
		//  https://github.com/BrianSharpe/Wombat/blob/master/Perlin3D.glsl

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
		const float3 SOMELARGEFLOATS = float3( 635.298681, 682.357502, 668.926525 );
		const float3 ZINC = float3( 48.500388, 65.294118, 63.934599 );
		float3 lowz_mod = float3( 1.0 / ( SOMELARGEFLOATS + Pi.zzz * ZINC ) );
		float3 highz_mod = float3( 1.0 / ( SOMELARGEFLOATS + Pi_inc1.zzz * ZINC ) );
		float4 hashx0 = fract( Pt * lowz_mod.xxxx );
		float4 hashx1 = fract( Pt * highz_mod.xxxx );
		float4 hashy0 = fract( Pt * lowz_mod.yyyy );
		float4 hashy1 = fract( Pt * highz_mod.yyyy );
		float4 hashz0 = fract( Pt * lowz_mod.zzzz );
		float4 hashz1 = fract( Pt * highz_mod.zzzz );

		// calculate the gradients
		float4 grad_x0 = hashx0 - 0.49999;
		float4 grad_y0 = hashy0 - 0.49999;
		float4 grad_z0 = hashz0 - 0.49999;
		float4 grad_x1 = hashx1 - 0.49999;
		float4 grad_y1 = hashy1 - 0.49999;
		float4 grad_z1 = hashz1 - 0.49999;
		float4 grad_results_0 = rsqrt( grad_x0 * grad_x0 + grad_y0 * grad_y0 + grad_z0 * grad_z0 ) * ( float2( Pf.x, Pf_min1.x ).xyxy * grad_x0 + float2( Pf.y, Pf_min1.y ).xxyy * grad_y0 + Pf.zzzz * grad_z0 );
		float4 grad_results_1 = rsqrt( grad_x1 * grad_x1 + grad_y1 * grad_y1 + grad_z1 * grad_z1 ) * ( float2( Pf.x, Pf_min1.x ).xyxy * grad_x1 + float2( Pf.y, Pf_min1.y ).xxyy * grad_y1 + Pf_min1.zzzz * grad_z1 );

		// Classic Perlin Interpolation
		float3 blend = Pf * Pf * Pf * (Pf * (Pf * 6.0 - 15.0) + 10.0);
		float4 res0 = mix( grad_results_0, grad_results_1, blend.z );
		float4 blend2 = float4( blend.xy, float2( 1.0 - blend.xy ) );
		float final = dot( res0, blend2.zxzx * blend2.wwyy );
		return ( final * 1.1547005383792515290182975610039 );  // scale things to a strict -1.0->1.0 range  *= 1.0/sqrt(0.75)
	}

	float Orb_Fbm12(thread OrbContext& ctx, float2 P) {
		const int octaves = 4;
		const float lacunarity = 1.8;
		const float gain = 0.80;

		float sum = 0.0;
		float amp = 0.8;
		float2 pp = P;

		int i;

		for(i = 0; i < octaves; ++i)
		{
			amp *= gain;
			sum += amp * Orb_noise12(ctx, pp);
			pp *= lacunarity;
		}
		return sum;
	}

	float Orb_Fbm31Magic(thread OrbContext& ctx, float3 p) {
		 float v = 0.0;
		 v += Orb_noise13(ctx, p * 1.0) * 2.200;
		 v -= Orb_noise13(ctx, p * 4.0) * 3.125;
		 return v;
	}

	float Orb_Fbm31Electro(thread OrbContext& ctx, float3 p) {
		 float v = 0.0;
		 v += Orb_noise13(ctx, p * 0.9) * 0.99;
		 v += Orb_noise13(ctx, p * 3.99) * 0.49;
		 v += Orb_noise13(ctx, p * 8.01) * 0.249;
		 v += Orb_noise13(ctx, p * 25.05) * 0.124;
		 return v;
	}

	#define SNORM2NORM(value) (value * 0.5 + 0.5)
	#define NORM2SNORM(value) (value * 2.0 - 1.0)

	
	

	float3 Orb_LightningOrb(thread OrbContext& ctx, float2 vUv, float3 color) {
		float2 uv = NORM2SNORM(vUv);

		const float strength = 0.01;
		const float dx = 0.2;

		float t = 0.0;

		for (int k = -4; k < 14; ++k) {
			float2 thisUV = uv;
			thisUV.x -= dx * float(k);
			thisUV.y += float(k);
			t += abs(strength / ((thisUV.x + Orb_Fbm12(ctx,  thisUV + ctx.time ))));
		}

		return color * t;
	}

float Orb_mirroredRepeat(thread OrbContext& ctx, float x, float repeats) {
    x *= repeats;
    float i = floor(x);
    float f = fract(x);
    // If i is odd, mirror the fractional part
    if ((i - 2.0 * floor(i / 2.0)) == 1.0) {
        f = 1.0 - f;
    }
    return f;
}

float3 Orb_LightningOrb2(thread OrbContext& ctx, float2 vUv, float3 color) {

    // Example: NO fract(), but still repeating:
    // vUv.x *= 3.0;

    // Or: mirror repeat for 2 tiles
    vUv.x = Orb_mirroredRepeat(ctx, vUv.x, 2.0);

    // Animated surface distortion - warp UVs with slow turbulent noise
    float2 distortUV = vUv * 3.0 + float2(ctx.time * 0.3, ctx.time * 0.17);
    vUv.x += Orb_Fbm12(ctx, distortUV) * 0.04;
    vUv.y += Orb_Fbm12(ctx, distortUV + float2(7.3, 3.1)) * 0.03;

    // From here on, continue as you did before:
    float2 uv = NORM2SNORM(vUv);

    float violence = (1 - ctx.input.modelPos.w);
    const float strength = 0.06 + 0.3 * violence;
    const float dx = 0.16;

    float t = 0.1;
    for (int k = -4; k < 5; ++k) {
        float2 thisUV = uv;
        thisUV.x -= dx * float(k);
        thisUV.y += 2.0 * float(k);
        float2 fbmUV = float2(thisUV.x * 1.0 + ctx.time, thisUV.y + 0.3 * ctx.time);

        // Your fract()-free or tiled/noise logic remains the same here:
        t += abs(strength / (thisUV.x + (3.0 * Orb_Fbm12(ctx, fbmUV) - 1.9)));
    }

    return color * t;
}



	float3 Orb_MagicOrb(thread OrbContext& ctx, float3 noiseVec, float3 color) {
		float t = 0.0;

		for( int i = 1; i < 2; ++i ) {
			t = abs(2.0 / ((noiseVec.y + Orb_Fbm31Magic(ctx,  noiseVec + 0.5 * ctx.time / float(i)) ) * 75.0));
			t += 1.3 * float(i);
		}
		return color * t;
	}

	float3 Orb_ElectroOrb(thread OrbContext& ctx, float3 noiseVec, float3 color) {
		float t = 0.0;

		for( int i = 0; i < 5; ++i ) {
			noiseVec = noiseVec.zyx;
			t = abs(2.0 / (Orb_Fbm31Electro(ctx, noiseVec + float3(0.0, ctx.time / float(i + 1), 0.0)) * 120.0));
			t += 0.2 * float(i + 1);
		}

		return color * t;
	}

	// Returns the X coords (around the belly) as [0-1], the Y coords as down-up [0-1]
	float2 Orb_RadialCoords(thread OrbContext& ctx, float3 a_coords)
	{
		float3 a_coords_n = normalize(a_coords);
		float lon = atan2(a_coords_n.z, a_coords_n.x);
		float lat = acos(a_coords_n.y);
		float2 sphereCoords = float2(lon, lat) / PI;
		return float2(sphereCoords.x * 0.5 + 0.5, 1.0 - sphereCoords.y);
	}

	float3 Orb_RotAroundY(thread OrbContext& ctx, float3 p)
	{
		float ra = -ctx.time * 0.5;
		float4x4 tr = float4x4(float4(cos(ra),0,sin(ra),0),float4(0,1,0,0),float4(-sin(ra),0,cos(ra),0),float4(0,0,0,1));

		return (tr * float4(p, 1.0)).xyz;
	}

float4 OrbMain(thread OrbContext& ctx)
{
 float4 fragColor;
	// Per-unit ctx.time offset so orbs don't animate in sync
	float unitPhase = fract(ctx.input.unitID * 1337.7);
	ctx.time = ctx.input.gameFrame + unitPhase * 120.0;

	fragColor = ctx.input.color;

	//ctx.input.modelPos contains the sphere's coords.

	if (ctx.input.technique == 1) { // LightningOrb
		float3 noiseVec = ctx.input.modelPos.xyz;
		noiseVec = Orb_RotAroundY(ctx, noiseVec);
		float3 nDir = normalize(noiseVec);

		// Primary projection (poles at top/bottom of sphere)
		float2 vUv = Orb_RadialCoords(ctx, noiseVec);
		float3 baseColor = fragColor.rgb;
		float3 col = Orb_LightningOrb2(ctx, vUv, baseColor);

		// Near the poles, blend in a second projection rotated 90° around X
		// so its pole singularity is at the equator of the primary projection
		
		float poleFade = abs(nDir.y);
		if (poleFade > 0.45) {
			float3 rotatedVec = float3(noiseVec.x, -noiseVec.z, noiseVec.y);
			float2 vUv2 = Orb_RadialCoords(ctx, rotatedVec);
			float3 col2 = Orb_LightningOrb2(ctx, vUv2, baseColor);
			float blendWeight = smoothstep(0.45, 0.85, poleFade);
			col = mix(col, col2, blendWeight);
		}
		

		// Color variation - bright white on arc peaks, base color in gaps
		float arcIntensity = clamp(length(col) / (length(baseColor) + 0.001), 0.0, 3.0) / 3.0;
		float3 hotColor = mix(baseColor, float3(1.0), 0.35);
		col = mix(col, col * hotColor / (baseColor + 0.001), arcIntensity);

		// Rim glow - bright edge halo
		float rim = pow(1.0 - ctx.input.opacity, 2.5);
		col += baseColor * rim * 0.8;

		// Inner core glow - bright volumetric center
		float core = pow(ctx.input.opacity, 2.0);
		col += baseColor * core * 0.6;

		// Pulse / breathing brightness
		float pulse = 1.0 + 0.08 * sin(ctx.time * 2.3) + 0.05 * sin(ctx.time * 5.7);
		col *= pulse;

		fragColor.rgba = float4(col, 1.0) * 1.2; return fragColor;
		//fragColor.rgb = max(fragColor.rgb, col * col);
		//fragColor.rgb = max(fragColor.rgb, col * 2);
	}
	else if (ctx.input.technique == 2) { // MagicOrb
		float3 noiseVec = ctx.input.modelPos.xyz;
		noiseVec = Orb_RotAroundY(ctx, noiseVec);
		float3 baseColor = fragColor.rgb;
		float3 col = Orb_MagicOrb(ctx, noiseVec, baseColor);
		fragColor.rgb = max(baseColor, col * col);

		float rim = pow(1.0 - ctx.input.opacity, 2.5);
		fragColor.rgb += baseColor * rim * 0.8;
		float core = pow(ctx.input.opacity, 2.0);
		fragColor.rgb += baseColor * core * 0.6;

		float pulse = 1.0 + 0.08 * sin(ctx.time * 2.3) + 0.05 * sin(ctx.time * 5.7);
		fragColor.rgb *= pulse;
	}
	else if (ctx.input.technique == 3) { // ElectroOrb
		float3 noiseVec = ctx.input.modelPos.xyz;
		noiseVec = Orb_RotAroundY(ctx, noiseVec);
		float3 baseColor = fragColor.rgb;
		float3 col = Orb_ElectroOrb(ctx, noiseVec, baseColor);
		fragColor.rgb = max(baseColor, col * col);

		float rim = pow(1.0 - ctx.input.opacity, 2.5);
		fragColor.rgb += baseColor * rim * 0.8;
		float core = pow(ctx.input.opacity, 2.0);
		fragColor.rgb += baseColor * core * 0.6;

		float pulse = 1.0 + 0.08 * sin(ctx.time * 2.3) + 0.05 * sin(ctx.time * 5.7);
		fragColor.rgb *= pulse;
	}

	fragColor.a = length(fragColor.rgb);
	if (ctx.reflection > 0) fragColor.rgba *= 3.0;
	//fragColor.rgba = float4(1.0);
	//fragColor.rgb = ctx.input.modelPos.xyz;
	//fragColor.rgba = float4(ctx.input.opacity,ctx.input.opacity, ctx.input.opacity, 1.0);
	//fragColor.rgb = ctx.input.color.rgb;
return fragColor;
}

#undef PI
fragment float4 OrbFragmentMain(OrbOut in [[stage_in]],constant Uniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 OrbContext context={in,0,float(u.options.x)};float4 color=OrbMain(context);
 if(u.options.z){float a=color.a,b=u.alpha.x;bool pass;switch(uint(u.alpha.y)){case 0:pass=false;break;case 1:pass=a<b;break;case 2:pass=a==b;break;case 3:pass=a<=b;break;case 4:pass=a>b;break;case 5:pass=a!=b;break;case 6:pass=a>=b;break;default:pass=true;}if(!pass)discard_fragment();}return color;
}
)ORB";
}
