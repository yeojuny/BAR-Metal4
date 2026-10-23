/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeLegacyWaterShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Vertex {packed_float3 position;packed_float2 uv;uchar4 color;};
struct Uniforms {float4x4 viewProjection;float4x4 view;float4 forward;float4 fogColor;float4 fogParameters;uint4 flags;};
struct Varying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];float2 bumpUV [[RECOIL_SAMPLE_PERSPECTIVE]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float fogCoordinate [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex Varying LegacyWaterVertex(uint id [[vertex_id]],device const Vertex* vertices [[buffer(0)]],constant Uniforms& u [[buffer(1)]]){
 const Vertex v=vertices[id];const float4 world=float4(float3(v.position),1);Varying o;
 o.position=u.viewProjection*world;if(u.flags.x)o.position.z=(o.position.z+o.position.w)*.5f;
 o.uv=float2(v.uv);o.bumpUV=world.xz*.02f;o.color=float4(v.color)/255.f;
 o.fogCoordinate=abs((u.view*world).z);return o;
}
fragment float4 LegacyWaterFragment(Varying v [[stage_in]],constant Uniforms& u [[buffer(1)]],texture2d<float> reflection [[texture(0)]],texture2d<float> bump [[texture(1)]],sampler reflectSampler [[sampler(0)]],sampler bumpSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
 float2 bumpUV=v.bumpUV;if(u.flags.z)bumpUV.y=1-bumpUV.y;
 const float2 b=bump.sample(bumpSampler,bumpUV).xy-.5f;
 const float2 displacement=float2(dot(b,u.forward.xy),dot(b,float2(-u.forward.y,u.forward.x)));
 float2 uv=v.uv+displacement*.02f;if(u.flags.y)uv.y=1-uv.y;
 float4 color=v.color;color.a=displacement.y*.03f+color.a;
 color*=reflection.sample(reflectSampler,uv);
 // OPTION ARB_fog_linear: source fog modifies RGB only, using the original eye-depth coordinate.
 color=clamp(color,0.f,1.f);
 color.rgb=mix(u.fogColor.rgb,color.rgb,clamp((u.fogParameters.y-v.fogCoordinate)*u.fogParameters.w,0.f,1.f));
 return color;
}
)MSL";
}

/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeLegacyRefractiveWaterShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Vertex {packed_float3 position;packed_float2 uv;uchar4 color;};
struct Uniforms {float4x4 viewProjection;float4x4 view;float4 forward;float4 fogColor;float4 fogParameters;uint4 flags;float4 refractExtent;float4 mapExtent;uint4 refractFlags;};
struct Varying {float4 position [[position]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];float2 bumpUV [[RECOIL_SAMPLE_PERSPECTIVE]];float2 depthUV [[RECOIL_SAMPLE_PERSPECTIVE]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float fogCoordinate [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex Varying LegacyWaterVertex(uint id [[vertex_id]],device const Vertex* vertices [[buffer(0)]],constant Uniforms& u [[buffer(1)]]){
 const Vertex v=vertices[id];const float4 world=float4(float3(v.position),1);Varying o;
 o.position=u.viewProjection*world;if(u.flags.x)o.position.z=(o.position.z+o.position.w)*.5f;
 o.uv=float2(v.uv);o.bumpUV=world.xz*.02f;o.color=float4(v.color)/255.f;
 o.depthUV=world.xz*u.mapExtent.xy;
 o.fogCoordinate=abs((u.view*world).z);return o;
}
fragment float4 LegacyWaterFragment(Varying v [[stage_in]],constant Uniforms& u [[buffer(1)]],texture2d<float> reflection [[texture(0)]],texture2d<float> bump [[texture(1)]],sampler reflectSampler [[sampler(0)]],sampler bumpSampler [[sampler(1)]],texture2d<float> subsurface [[texture(2)]],texture2d<float> shading [[texture(3)]],sampler subsurfaceSampler [[sampler(2)]],sampler shadingSampler [[sampler(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(v.position,get_sample_position(recoilOriginalSampleID));
 float2 bumpUV=v.bumpUV;if(u.flags.z)bumpUV.y=1-bumpUV.y;
 const float2 b=bump.sample(bumpSampler,bumpUV).xy-.5f;
 const float2 displacement=float2(dot(b,u.forward.xy),dot(b,float2(-u.forward.y,u.forward.x)));
 float2 uv=v.uv+displacement*.02f;if(u.flags.y)uv.y=1-uv.y;
 float4 color=v.color;if(!u.refractFlags.x)color.a=displacement.y*.03f+color.a;
 color*=reflection.sample(reflectSampler,uv);
 if(u.refractFlags.x){
  float2 depthUV=v.depthUV;if(u.refractFlags.z)depthUV.y=1-depthUV.y;
  const float depth=shading.sample(shadingSampler,depthUV).a;
  // ARB fragment.position is full original window coordinates, NOT viewport-relative.
  const float2 window=float2(v.position.x,u.refractExtent.z-v.position.y);
  float2 sceneUV;
  if(u.refractFlags.y)sceneUV=(window+displacement*(1-depth)*(10.f*u.refractExtent.xy)*v.position.w)/u.refractExtent.xy;
  else sceneUV=window/u.refractExtent.xy+displacement*(1-depth)*10.f*v.position.w;
  sceneUV.y=1-sceneUV.y;
  const float4 scene=float4(subsurface.sample(subsurfaceSampler,sceneUV).rgb,1.f);
  color=mix(color,scene,float4(float3(depth*.5f+.4f),0.f));
 }
 // OPTION ARB_fog_linear: source fog modifies RGB only, using the original eye-depth coordinate.
 color=clamp(color,0.f,1.f);
 color.rgb=mix(u.fogColor.rgb,color.rgb,clamp((u.fogParameters.y-v.fogCoordinate)*u.fogParameters.w,0.f,1.f));
 return color;
}
)MSL";
}
