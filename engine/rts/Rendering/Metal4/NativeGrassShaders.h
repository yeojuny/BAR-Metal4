/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
// Exact31657 map_grass_gl4 vertex/fragment translation. Original shaders:
// Copyright (c) 2024 Beherith (mysterme@gmail.com), MIT License.
namespace rendering::metal4::grassprogram {
inline constexpr const char* Source=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct GrassUniform {
 float4x4 viewProj,viewInv,shadowView;
 float4 mapSize,fogColor,wind,night;
 float defines[18];float distanceMult;int unitBendCount;
 uint4 options,imageFlags[6],swizzle[6];float4 imageLod[6],alpha;
};
struct GrassOut {float4 position [[position]];float4 texCoord [[RECOIL_SAMPLE_PERSPECTIVE]],mapColor [[RECOIL_SAMPLE_PERSPECTIVE]],params [[RECOIL_SAMPLE_PERSPECTIVE]];};
float GrassChannel(float4 c,uint n){return n==0u?0.f:n==1u?1.f:n==0x1903u?c.r:n==0x1904u?c.g:n==0x1905u?c.b:c.a;}
float4 GrassSwizzle(float4 c,uint4 s){return float4(GrassChannel(c,s.x),GrassChannel(c,s.y),GrassChannel(c,s.z),GrassChannel(c,s.w));}
float2 GrassUV(float2 uv,uint slot,constant GrassUniform& u){return u.imageFlags[slot].x?float2(uv.x,1.f-uv.y):uv;}
float4 GrassVertexSample(texture2d<float> t,sampler s,float2 uv,uint slot,constant GrassUniform& u){return GrassSwizzle(t.sample(s,GrassUV(uv,slot,u),level(clamp(0.f,u.imageLod[slot].x,u.imageLod[slot].y))),u.swizzle[slot]);}
#if GRASS_SHADOWS
float GrassShadow(float3 p,depth2d<float> tex,sampler sampling,constant GrassUniform& u){
 float4 q=u.shadowView*float4(p,1.f);q.xy+=.5f;
 float2 uv=GrassUV(q.xy/q.w,3,u);float reference=q.z/q.w;
 if(u.imageFlags[3].w)reference=clamp(reference,0.f,1.f);
 return clamp(tex.sample_compare(sampling,uv,reference,level(0.f)),u.defines[8],1.f);
}
#endif
vertex GrassOut GrassVertex(uint vid [[vertex_id]],uint iid [[instance_id]],
 device const float* vertices [[buffer(0)]],device const float4* instances [[buffer(1)]],constant GrassUniform& u [[buffer(2)]]
#if GRASS_BEND
 ,device const float4* bends [[buffer(3)]]
#endif
 ,texture2d<float> mapColorTex [[texture(1)]],texture2d<float> noiseTex [[texture(2)]]
#if GRASS_SHADOWS
 ,depth2d<float> shadowTex [[texture(3)]]
#endif
 ,texture2d<float> losTex [[texture(4)]],texture2d<float> heightTex [[texture(5)]],
 sampler mapSampler [[sampler(1)]],sampler noiseSampler [[sampler(2)]]
#if GRASS_SHADOWS
 ,sampler shadowSampler [[sampler(3)]]
#endif
 ,sampler losSampler [[sampler(4)]],sampler heightSampler [[sampler(5)]]){
 GrassOut out{};float4 instance=instances[iid];
 float4 initialClip=u.viewProj*float4(instance.x,0.f,instance.z,1.f);float absW=abs(initialClip.w);
 if(absW<.00001f||initialClip.z < -absW||initialClip.z>absW||instance.w<=0.f){out.position=float4(2,2,2,1);return out;}
 bool compact=u.defines[12]==1.f;uint stride=compact?8u:17u;uint start=vid*stride;
 float3 pos=float3(vertices[start],vertices[start+1],vertices[start+2]);uint normalAt=start+(compact?4u:3u);
 float3 normal=float3(vertices[normalAt],vertices[normalAt+1],vertices[normalAt+2]);
 float2 uv=compact?float2(vertices[start+3],vertices[start+7]):float2(vertices[start+12],vertices[start+13]);
 float scale=instance.w,s=sin(instance.y),c=cos(instance.y);
 float3 world=pos*scale;world.xz=float2(c*world.x+s*world.z,-s*world.x+c*world.z)+instance.xz;
 float3 rotatedNormal=float3(c*normal.x+s*normal.z,normal.y,-s*normal.x+c*normal.z);
 float2 heightUV=clamp(world.xz,float2(8.f),u.mapSize.xy-8.f)/u.mapSize.xy;
 float ground=GrassVertexSample(heightTex,heightSampler,heightUV,5,u).x;
 world.y=(pos.y+.5f)*scale+ground;
 float4 los=GrassVertexSample(losTex,losSampler,world.xz/u.mapSize.zw,4,u);
 out.params.z=clamp(dot(los.rgb,float3(.33f))*1.5f,0.f,1.f);
 float shadow=1.f;
#if GRASS_SHADOWS
 if(u.defines[10]>=4.f){
  shadow+=GrassShadow(world+float3(4,0,4),shadowTex,shadowSampler,u);
  shadow+=GrassShadow(world+float3(-4,0,-4),shadowTex,shadowSampler,u);
  shadow+=GrassShadow(world+float3(-4,0,4),shadowTex,shadowSampler,u);
  shadow+=GrassShadow(world+float3(4,0,-4),shadowTex,shadowSampler,u);shadow*=.2f;
 }else shadow=GrassShadow(world,shadowTex,shadowSampler,u);
#endif
 out.params.y=clamp(shadow,u.defines[8],1.f);
 out.mapColor=GrassVertexSample(mapColorTex,mapSampler,world.xz/u.mapSize.xy,1,u);
 float4 noise=GrassVertexSample(noiseTex,noiseSampler,(world.xz+u.wind.xy*u.defines[4])*u.defines[5],2,u);
 noise=(noise-.5f).xzyw;float shade=(noise.y*2.f-.66f)*3.f;noise.y-=.4f;
 out.params.w=mix(1.f,shade,uv.y);world+=noise.rgb*pos.y*scale*u.defines[3]*u.wind.z;
#if GRASS_BEND
 float2 totalBend=0.f;float maxShrink=0.f;
 for(int i=0;i<u.unitBendCount;++i){float4 unit=bends[i];float2 diff=instance.xz-unit.xy;float distSq=dot(diff,diff);float radius=unit.z;
  if(distSq<radius*radius&&distSq>.0001f){float dist=sqrt(distSq);float t=1.f-dist/radius;float strength=pow(t,u.defines[15])*unit.w;totalBend+=(diff/dist)*strength;maxShrink=max(maxShrink,strength);}}
 float shrink=1.f-clamp(maxShrink,0.f,1.f)*u.defines[16];world.y=(world.y-ground)*shrink+ground;
 float bendLen=length(totalBend);if(bendLen>.001f){float bendAngle=min(bendLen,1.f)*u.defines[14]*.175f;float bladeHeight=max(0.f,pos.y+.5f)*scale;
  world.xz+=(totalBend/bendLen)*sin(bendAngle)*bladeHeight;world.y-=bladeHeight*(1.f-cos(bendAngle));}
#endif
 out.mapColor.a=1.f;float3 eye=u.viewInv[3].xyz;
 float dist=length(world-eye);out.params.x=clamp((u.defines[7]*u.distanceMult-dist)/(u.defines[7]*u.distanceMult-u.defines[6]*u.distanceMult),0.f,1.f);
 out.texCoord.w=dot(rotatedNormal,normalize(eye-world));out.texCoord.xy=uv;
 out.position=u.viewProj*float4(world,1.f);if(!u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;return out;
}
bool GrassAlphaPass(float a,constant GrassUniform& u){if(!u.options.z)return true;float b=u.alpha.x;switch(u.options.w){case 0u:return false;case 1u:return a<b;case 2u:return a==b;case 3u:return a<=b;case 4u:return a>b;case 5u:return a!=b;case 6u:return a>=b;default:return true;}}
fragment float4 GrassFragment(GrassOut in [[stage_in]],constant GrassUniform& u [[buffer(2)]],texture2d<float> blade [[texture(0)]],sampler bladeSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 color=GrassSwizzle(blade.sample(bladeSampler,GrassUV(in.texCoord.xy,0,u),bias(u.imageLod[0].z)),u.swizzle[0]);
 color.rgb=mix(color.rgb,color.rgb*(in.mapColor.rgb*2.f),u.defines[0]);
 color.rgb=mix(color.rgb,in.mapColor.rgb,(1.f-in.texCoord.y)*u.defines[1]);color.rgb=mix(u.fogColor.rgb,color.rgb,in.mapColor.a);
 color.a*=u.wind.w*in.params.x;color.rgb*=in.params.y*in.params.z*in.params.w*u.defines[11];
 color.a=clamp((color.a-.5f)*1.5f+.5f,0.f,1.f);if(u.defines[17]==1.f)color.a*=clamp(in.texCoord.w*3.f,0.f,1.f);
 color.rgb*=u.night.rgb;if(color.a<u.defines[2]||!GrassAlphaPass(color.a,u))discard_fragment();return color;
}
)MSL";
}
