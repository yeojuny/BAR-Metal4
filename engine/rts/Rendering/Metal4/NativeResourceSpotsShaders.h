/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Exact31657 metalspots_gl4 vertex/fragment and geothermal embedded shaders.
// Metal shader source: Copyright (c) 2026 Beherith, SPDX-License-Identifier: MIT.
// Geometry, instance data, occupancy, text-atlas and callback selection stay in original Lua.
namespace rendering::metal4::resourcespotsprogram {
inline constexpr const char* Source=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ResourceSpotsUniform {
 float4x4 viewProj,viewInv;
 float4 mapSize,timeInfo,values;
 uint4 options,imageFlags[2],swizzle[2];float4 imageLod[2],alpha;
};
struct ResourceSpotsOut {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];float circlealpha [[RECOIL_SAMPLE_PERSPECTIVE]];};
float4 ResourceClip(float4 p,constant ResourceSpotsUniform& u){if(!u.options.x)p.z=(p.z+p.w)*.5f;return p;}
float3 ResourceRotateY(float3 p,float a){float s=sin(a),c=cos(a);return float3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z);}
float ResourceChannel(float4 c,uint n){return n==0u?0.f:n==1u?1.f:n==0x1903u?c.r:n==0x1904u?c.g:n==0x1905u?c.b:c.a;}
float4 ResourceSwizzle(float4 c,uint4 s){return float4(ResourceChannel(c,s.x),ResourceChannel(c,s.y),ResourceChannel(c,s.z),ResourceChannel(c,s.w));}
float2 ResourceUV(float2 p,uint slot,constant ResourceSpotsUniform& u){return u.imageFlags[slot].x?float2(p.x,1.f-p.y):p;}
vertex ResourceSpotsOut ResourceSpotsVertex(uint vid [[vertex_id]],uint iid [[instance_id]],
 device const float4* vertices [[buffer(0)]],device const float4* instances [[buffer(1)]],constant ResourceSpotsUniform& u [[buffer(2)]]
#if METAL_SPOTS
 ,texture2d<float> heightMap [[texture(0)]],sampler heightSampler [[sampler(0)]]
#endif
){
 ResourceSpotsOut out{};out.uv=float2(-1.f);float4 local=vertices[vid];
#if METAL_SPOTS
 float4 world=instances[iid*3],visibility=instances[iid*3+1],atlasUV=instances[iid*3+2];
#else
 float4 world=instances[iid*2],visibility=instances[iid*2+1];
#endif
 float frame=u.timeInfo.x+u.timeInfo.w;float elapsed=(frame-visibility.y)/30.f;
#if METAL_SPOTS
 out.circlealpha=clamp(mix(.5f-elapsed,elapsed,step(.5f,visibility.x)),0.f,.5f);
 float3 toCamera=world.xyz-u.viewInv[3].xyz;float ratio=128.f*rsqrt(dot(toCamera,toCamera));float4 center=u.viewProj*float4(world.xyz,1.f);
 bool outside=any(abs(center.xy)>float2(center.w)*(1.f+ratio));
 if((local.w==0.f&&out.circlealpha<=0.f)||outside){out.position=ResourceClip(u.viewProj*float4(-1000,-1000,-1000,1),u);return out;}
 float2 heightUV=clamp(world.xz,float2(8.f),u.mapSize.xy-8.f)/u.mapSize.xy;
 float ground=ResourceSwizzle(heightMap.sample(heightSampler,ResourceUV(heightUV,0,u),level(clamp(0.f,u.imageLod[0].x,u.imageLod[0].y))),u.swizzle[0]).x;
 if((u.values.x<.5f&&ground<u.values.y)||(u.values.x>.5f&&ground>=u.values.y)){out.position=ResourceClip(u.viewProj*float4(-1000,-1000,-1000,1),u);return out;}
 float s=sign(local.z);float3 p=float3(local.x*s,0.f,local.y);p=ResourceRotateY(p,s*frame*.005f);
 float3 worldXYZ=float3(world.x,ground+5.f,world.z);p=p*(12.f+local.z)*2.f*world.w+worldXYZ;
 out.position=ResourceClip(u.viewProj*float4(p,1.f),u);out.color=float4(1,1,1,out.circlealpha);
 if(local.w==1.f){if(local.z!=0.f)out.circlealpha=0.f;out.color=visibility.x>.5f?float4(0,1,.5f,1):float4(1,0,0,1);}
 if(local.w==2.f){
  out.circlealpha=1.f;float3 billboard=float3(visibility.z*local.x,0.f,2.f*visibility.w*local.y);
  float3x3 rotation=float3x3(u.viewInv[0].xyz,u.viewInv[2].xyz,u.viewInv[1].xyz);
  out.position=ResourceClip(u.viewProj*float4(rotation*billboard*.25f+worldXYZ,1.f),u);
  out.uv=float2(local.x<=0.f?atlasUV.x:atlasUV.y,local.y<=0.f?atlasUV.z:atlasUV.w);
 }
#else
 out.circlealpha=clamp(visibility.x>.5f?elapsed:.5f-elapsed,0.f,.5f);
 if(out.circlealpha<=0.f){out.position=ResourceClip(float4(2,2,2,1),u);return out;}
 float3 p=float3(local.x,0.f,local.y);
 if(local.z<0.f)p=ResourceRotateY(p,frame*.005f);else{p.x*=-1.f;p=ResourceRotateY(p,-frame*.005f);}
 p=p*(12.f+local.z)*2.f*world.w+world.xyz;p.y+=2.f;
 out.position=ResourceClip(u.viewProj*float4(p,1.f),u);out.color=float4(.7f,1.f,.7f,out.circlealpha);
#endif
 return out;
}
bool ResourceAlphaPass(float a,constant ResourceSpotsUniform& u){if(!u.options.z)return true;float b=u.alpha.x;switch(u.options.w){case 0u:return false;case 1u:return a<b;case 2u:return a==b;case 3u:return a<=b;case 4u:return a>b;case 5u:return a!=b;case 6u:return a>=b;default:return true;}}
fragment float4 ResourceSpotsFragment(ResourceSpotsOut in [[stage_in]],constant ResourceSpotsUniform& u [[buffer(2)]]
#if METAL_SPOTS
 ,texture2d<float> textAtlas [[texture(1)]],sampler atlasSampler [[sampler(1)]]
#endif
 RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 color=float4(in.color.rgb,in.circlealpha);
#if METAL_SPOTS
 if(in.uv.x>-.5f)color=ResourceSwizzle(textAtlas.sample(atlasSampler,ResourceUV(in.uv,1,u),bias(u.imageLod[1].z)),u.swizzle[1]);
#endif
 if(!ResourceAlphaPass(color.a,u))discard_fragment();return color;
}
)MSL";
}
