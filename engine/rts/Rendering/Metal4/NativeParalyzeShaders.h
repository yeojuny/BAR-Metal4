/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* ParalyzeMSL=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"PARA(
struct ParalyzeVertex {packed_float3 position;uint piece;};
static_assert(sizeof(ParalyzeVertex)==16,"Paralyze consumed source vertex fields");
struct ParalyzeOut {float4 position [[position]];float3 modelPos [[RECOIL_SAMPLE_PERSPECTIVE]];float paralysis [[flat]];};
vertex ParalyzeOut ParalyzeVertexMain(uint id [[vertex_id]],device const ParalyzeVertex* vertices [[buffer(0)]],device const Transform* transforms [[buffer(2)]],constant Uniform& u [[buffer(3)]],device const Model* model [[buffer(4)]]){
 const ParalyzeVertex v=vertices[id];Transform piece=TXLerp(transforms[2+2*v.piece],transforms[3+2*v.piece],u.time.w);Transform root=TXLerp(transforms[0],transforms[1],u.time.w);
 ParalyzeOut out;out.modelPos=Rotate(piece.q,float3(v.position)*piece.t.w)+piece.t.xyz;float3 world=Rotate(root.q,out.modelPos*root.t.w)+root.t.xyz;
 float strength=model->user[1].x;out.paralysis=min(strength*strength,1.1);out.position=u.vp*float4(world,1);
 if((model->metadata.x&3u)==0u||out.paralysis==0.0)out.position=float4(2,2,2,1);if(!u.options.y)out.position.z=(out.position.z+out.position.w)*.5;return out;
}
float ParalyzeMod(float x,float y){return x-y*floor(x/y);}float2 ParalyzeMod(float2 x,float y){return x-y*floor(x/y);}float4 ParalyzeMod(float4 x,float y){return x-y*floor(x/y);}
float4 Paralyze_permute(float4 x){return ParalyzeMod(((x*34.0)+1.0)*x, 289.0);}
float2 Paralyze_permute(float2 x){return ParalyzeMod(((x*34.0)+1.0)*x, 289.0);}
float Paralyze_permute(float x){return ParalyzeMod(((x*34.0)+1.0)*x, 289.0);} // whole numbers in and out, so no floor
float4 Paralyze_taylorInvSqrt(float4 r){return 1.79284291400159 - 0.85373472095314 * r;}
float Paralyze_taylorInvSqrt(float r){return 1.79284291400159 - 0.85373472095314 * r;}

float4 Paralyze_grad4(float j, float4 ip){
  const float4 ones = float4(1.0, 1.0, 1.0, -1.0);
  float4 p;

  p.xyz = floor( fract (float3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
  p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
  p.xyz += float(p.w < 0.0); // p.xyz is always negative here, so this is the whole sign fix-up

  return p;
}

float Paralyze_snoise(float4 v){
  const float2  C = float2( 0.138196601125010504,  // (5 - sqrt(5))/20  G4
                        0.309016994374947451); // (sqrt(5) - 1)/4   F4
// First corner
  float4 i  = floor(v + dot(v, C.yyyy) );
  float4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
  float4 i0;

  float3 isX = step( x0.yzw, x0.xxx );
  float3 isYZ = step( x0.zww, x0.yyz );
//  i0.x = dot( isX, float3( 1.0 ) );
  i0.x = isX.x + isX.y + isX.z;
  i0.yzw = 1.0 - isX;

//  i0.y += dot( isYZ.xy, float2( 1.0 ) );
  i0.y += isYZ.x + isYZ.y;
  i0.zw += 1.0 - isYZ.xy;

  i0.z += isYZ.z;
  i0.w += 1.0 - isYZ.z;

  // i0 now contains the unique values 0,1,2,3 in each channel
  float4 i3 = clamp( i0, 0.0, 1.0 );
  float4 i2 = clamp( i0-1.0, 0.0, 1.0 );
  float4 i1 = clamp( i0-2.0, 0.0, 1.0 );

  //  x0 = x0 - 0.0 + 0.0 * C
  float4 x1 = x0 - i1 + 1.0 * C.xxxx;
  float4 x2 = x0 - i2 + 2.0 * C.xxxx;
  float4 x3 = x0 - i3 + 3.0 * C.xxxx;
  float4 x4 = x0 - 1.0 + 4.0 * C.xxxx;

// Permutations
  i = ParalyzeMod(i, 289.0);
  // the first level only ever hashes i.w or i.w + 1, so do those two once and pick per corner
  float2 jw = Paralyze_permute(i.ww + float2(0.0, 1.0));
  float j0 = Paralyze_permute( Paralyze_permute( Paralyze_permute( jw.x + i.z) + i.y) + i.x);
  float4 j1 = Paralyze_permute( Paralyze_permute( Paralyze_permute(
             jw.x + float4(i1.w, i2.w, i3.w, 1.0 ) * (jw.y - jw.x)
           + i.z + float4(i1.z, i2.z, i3.z, 1.0 ))
           + i.y + float4(i1.y, i2.y, i3.y, 1.0 ))
           + i.x + float4(i1.x, i2.x, i3.x, 1.0 ));
// Gradients
// ( 7*7*6 points uniformly over a cube, mapped onto a 4-octahedron.)
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.

  float4 ip = float4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

  float4 p0 = Paralyze_grad4(j0,   ip);
  float4 p1 = Paralyze_grad4(j1.x, ip);
  float4 p2 = Paralyze_grad4(j1.y, ip);
  float4 p3 = Paralyze_grad4(j1.z, ip);
  float4 p4 = Paralyze_grad4(j1.w, ip);

// Normalise gradients
  float4 norm = Paralyze_taylorInvSqrt(float4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  p4 *= Paralyze_taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
  float3 m0 = max(0.6 - float3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
  float2 m1 = max(0.6 - float2(dot(x3,x3), dot(x4,x4)            ), 0.0);
  m0 = m0 * m0;
  m1 = m1 * m1;
  return 49.0 * ( dot(m0*m0, float3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 )))
               + dot(m1*m1, float2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;

}

fragment float4 ParalyzeFragment(ParalyzeOut in [[stage_in]],constant Uniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 fragColor;
	float paralysis_level = in.paralysis; // values of 1 are fully paralyzed

	float noisescale;
	float persistence;
	float lacunarity;
	float3 minlightningcolor;
	float3 maxlightningcolor;
	float4 wholeunitbasecolor;
	float lightningalpha;
	float lighting_sharpness;
	float lighting_width;
	float lightning_speed;

	// ------------------ CONFIG START --------------------

	if (paralysis_level < 0.9999) { // not fully paralyzed
		noisescale = 0.15;
		persistence = 0.45;
		lacunarity = 2.5;
		minlightningcolor = float3(0.1, 0.1, 0.5); //blue
		maxlightningcolor = float3(0.9, 0.9, 0.9); //white
		wholeunitbasecolor = float4(0.0, 0.0, 0.0, 0.0); // none
		lightningalpha = 1.4;
		lighting_sharpness = 12.8;
		lighting_width = 3.95;
		lightning_speed = 0.14;
	}
	else{ // fully paralyzed
		noisescale = 0.31;
		persistence = 0.45;
		lacunarity = 2.5;
		minlightningcolor = float3(0.1, 0.1, 1.0); //blue
		maxlightningcolor = float3(1.0, 1.0, 1.0); //white
		wholeunitbasecolor = float4(0.49, 0.43, 0.94, 0.35); // light blue base tone
		lightningalpha = 1.2;
		lighting_sharpness = 4.8;
		lighting_width = 3.8;
		lightning_speed = 0.95;
	}
	// ------------------ CONFIG END --------------------

	float4 noiseposition = noisescale * float4(in.modelPos, (u.time.x + u.time.w) * lightning_speed);
	float noise4 = 0;
	for (int i = 1; i <= 4; ++i) {
		noise4 += pow(persistence, float(i)) * Paralyze_snoise(noiseposition * 0.025 * pow(lacunarity, float(i)));
	}
	noise4 = (1.0 * noise4 + 0.5);
	float electricity = clamp(1.0 - abs(noise4 - 0.5) * lighting_width, 0.0, 1.0);
	electricity = clamp(pow(electricity, lighting_sharpness), 0.0, 1.0);

	float3 lightningcolor;
	float effectalpha;
	if (paralysis_level < 0.9999) {
		if(u.options.w){
			paralysis_level = min(paralysis_level * 3.0, 1.0);
			if (paralysis_level > 0.49) { wholeunitbasecolor = float4(0.35, 0.43, 0.94, 0.18); }
		}
		// Calculate the lightning color based on the amount of electricity
		lightningcolor = mix(minlightningcolor, maxlightningcolor, electricity);
		effectalpha = paralysis_level * lightningalpha; // less transparency non-paralyzed
	}
	else
	{
		lightningcolor = mix(minlightningcolor, maxlightningcolor, electricity);
		effectalpha = clamp(paralysis_level * lightningalpha, 0.0, 1.0);
	}

	fragColor = float4(lightningcolor, electricity*effectalpha);
	fragColor = max(wholeunitbasecolor, fragColor); // apply whole unit base color
if(u.options.z){float a=fragColor.a,b=u.alpha.x;bool pass;switch(uint(u.alpha.y)){case 0:pass=false;break;case 1:pass=a<b;break;case 2:pass=a==b;break;case 3:pass=a<=b;break;case 4:pass=a>b;break;case 5:pass=a!=b;break;case 6:pass=a>=b;break;default:pass=true;}if(!pass)discard_fragment();}return fragColor;
}

)PARA";
}
