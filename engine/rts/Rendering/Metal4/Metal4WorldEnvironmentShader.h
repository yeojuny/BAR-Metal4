/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
// Direct translations of engine CubeMapVS/FS and ModernSkyVS/FS, plus exact30770
// GenBrdfLut (1024 Hammersley samples, G_OPTION=1). No invented environment data.
inline constexpr const char* WORLD_ENVIRONMENT_SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
constant bool OriginalSkyHasCube [[function_constant(0)]];
struct SkyUniform {float4x4 vp,rotation;float4 cameraPosition,midMap,skyColor,sunColor,cloudColor,planeColor,sun;float4 parameters;uint4 flags;};
constant float3 cubeVertices[36]={
float3(1,-1,-1),float3(1,-1,1),float3(1,1,1),float3(1,1,1),float3(1,1,-1),float3(1,-1,-1),
float3(-1,-1,1),float3(-1,-1,-1),float3(-1,1,-1),float3(-1,1,-1),float3(-1,1,1),float3(-1,-1,1),
float3(-1,1,-1),float3(1,1,-1),float3(1,1,1),float3(1,1,1),float3(-1,1,1),float3(-1,1,-1),
float3(-1,-1,-1),float3(-1,-1,1),float3(1,-1,-1),float3(1,-1,-1),float3(-1,-1,1),float3(1,-1,1),
float3(-1,-1,1),float3(-1,1,1),float3(1,1,1),float3(1,1,1),float3(1,-1,1),float3(-1,-1,1),
float3(-1,1,-1),float3(-1,-1,-1),float3(1,-1,-1),float3(1,-1,-1),float3(1,1,-1),float3(-1,1,-1)};
struct SkyVarying {float4 clip [[position]];float3 direction [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex SkyVarying OriginalSkyVertex(uint vid [[vertex_id]],constant SkyUniform& u [[buffer(0)]]) {
 float3 p=cubeVertices[vid];SkyVarying out;
 if(u.flags.x==2){p=u.midMap.xyz+p*sqrt(16*(u.midMap.x*u.midMap.x+u.midMap.z*u.midMap.z));out.direction=p-u.cameraPosition.xyz;out.clip=u.vp*float4(p,1);}
 else{out.direction=p*float3(1,-1,1);out.clip=u.vp*u.rotation*float4(p,1);}
 out.clip=out.clip.xyww;if(u.flags.y)out.clip.y=-out.clip.y;return out;
}
float Value3D( float3 P )
{
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

#define noise(x) (Value3D(x))

constant float3x3 m = float3x3(0.0, 1.60,  1.20, -1.6, 0.72, -0.96, -1.2, -0.96, 1.28);

float fbmFull(float3 p)
{
	float f = 0.0;
	f += noise(p) / 2 ; p = m * p * 1.1;
	f += noise(p) / 4 ; p = m * p * 1.2;
	f += noise(p) / 6 ; p = m * p * 1.3;
	f += noise(p) / 12; p = m * p * 1.4;
	f += noise(p) / 24;
	return f;
}

float fbmSimple(float3 p)
{
	float f = 0.0;
	f += noise(p) / 2 ; p = m * p * 1.1;
	f += noise(p) / 4 ; p = m * p * 2.2;
	f += noise(p) / 12 ;
	f *= 1.25;
	return f;
}


float csstep(float m0, float m1, float n0, float n1, float v)
{
	return smoothstep(m0, m1, v) * (1.0 - smoothstep(n0, n1, v));
}


float4 OriginalModernSky(float3 direction,constant SkyUniform& u) {
 float3 pos=normalize(direction);float cirrus=u.cloudColor.w*.9f,cumulus=u.cloudColor.w*1.8f;
 float sunContribution=pow(max(0.f,dot(pos,normalize(u.sun.xyz))),64.f);
 float wp=(1-smoothstep(-.5f,-.2f,pos.y))*u.planeColor.w;
 float3 color=mix(u.skyColor.rgb,u.planeColor.rgb,wp);color=mix(color,u.sunColor.rgb*u.sunColor.w*1.3f,sunContribution);
 float3 extinction=mix(float3(1),float3(1-exp(u.sun.y))*.2f,-u.sun.y*.2f+.5f);
 float3 p=pos/pos.y*2+u.parameters.x*.05f;
 float density=smoothstep(1-cirrus,1.f,u.flags.y?fbmSimple(p):fbmFull(p))*.3f;
 color=mix(color,u.cloudColor.rgb*extinction*4,density*max(pos.y,0.f));
 for(int i=0;i<(u.flags.y?1:2);++i){float3 cp=pos;cp.y=smoothstep(-.5f,1.5f,pos.y);cp.xz/=cp.y;
  float3 q=(.7f+float(i)*.01f)*cp+u.parameters.x*.3f;
  float den=smoothstep(1-cumulus,1.f,u.flags.y?fbmSimple(q):fbmFull(q));
  color=mix(color,u.cloudColor.rgb*extinction*den*5,min(den,1.f)*max(pos.y,0.f));}
 return float4(color,.5f-csstep(-.8f,0.f,-.5f,.3f,pos.y));
}
fragment float4 OriginalSkyFragment(SkyVarying in [[stage_in]],constant SkyUniform& u [[buffer(0)]],texturecube<float> source [[texture(0), function_constant(OriginalSkyHasCube)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 if(!OriginalSkyHasCube || u.flags.x==2)return OriginalModernSky(in.direction,u);
 constexpr sampler linearCube(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
 float4 value=u.flags.z?source.sample(linearCube,normalize(in.direction)):source.sample(linearCube,normalize(in.direction),level(0));
 return mix(value,float4(u.planeColor.rgb,1),smoothstep(0.f,.5f,in.direction.y)*u.planeColor.w);
}
float2 OriginalBRDF(float nov,float roughness) {
 float3 N=float3(0,0,1),V=float3(sqrt(1-nov*nov),0,nov);float2 sum=0;
 for(uint i=0;i<1024;++i){uint bits=reverse_bits(i);float2 xi=float2(float(i)/1024,float(bits)*2.3283064365386963e-10f);
  float r2=roughness*roughness,phi=2*M_PI_F*xi.x,ct2=(1-xi.y)/(1+(r2*r2-1)*xi.y);
  float3 h=normalize(float3(sqrt(1-ct2)*sin(phi),-sqrt(1-ct2)*cos(phi),sqrt(ct2)));
  float3 l=2*dot(V,h)*h-V;float nl=max(dot(N,l),0.f),vh=max(dot(V,h),0.f),nv=clamp(dot(N,V),1e-7f,1.f),nh=clamp(dot(h,N),1e-7f,1.f);
  float k=r2/2,gl=nl/(nl*(1-k)+k),gv=nv/(nv*(1-k)+k),vis=(gl*gv*vh)/(nh*nv),fc=pow(1-vh,5.f);
  sum+=mix(float2(0),float2((1-fc)*vis,fc*vis),float2(float(nl>0)));}
 return sum/1024;
}
float3 OriginalCubeDirection(uint face,float2 uv) {
 float2 p=uv*2-1;
 switch(face){case 0:return float3(1,-p.y,-p.x);case 1:return float3(-1,-p.y,p.x);
 case 2:return float3(p.x,1,p.y);case 3:return float3(p.x,-1,-p.y);case 4:return float3(p.x,-p.y,1);default:return float3(-p.x,-p.y,-1);}
}
kernel void OriginalEquiRectToCube(uint3 p [[thread_position_in_grid]],texture2d<float> source [[texture(0)]],texture2d_array<float,access::write> result [[texture(1)]]) {
 uint2 size=uint2(result.get_width(),result.get_height());if(p.z>=6||any(p.xy>=size))return;
 float3 ray=normalize(OriginalCubeDirection(p.z,(float2(p.xy)+.5f)/float2(size)));
 constexpr sampler filter(coord::normalized,address::repeat,filter::linear);
 float2 uv=float2(atan2(ray.z,ray.x)/(2*M_PI_F)+.5f,acos(-ray.y)/M_PI_F);
 result.write(source.sample(filter,uv,level(0)),p.xy,p.z);
}
kernel void OriginalUnitBRDF(uint2 p [[thread_position_in_grid]],texture2d<float,access::write> result [[texture(0)]]) {
 uint2 extent=uint2(result.get_width(),result.get_height());if(any(p>=extent))return;
 float2 uv=(float2(p)+.5f)/float2(extent);result.write(float4(OriginalBRDF(uv.x,uv.y),0,1),p);
}
)MSL";
} // namespace rendering::metal4
