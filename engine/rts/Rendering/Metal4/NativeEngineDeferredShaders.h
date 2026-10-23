/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
// Original rigid-model vertex semantics and five-output fragment equations.
// GL4 interpolation reads only retained actual uploader publications.
inline constexpr const char* NativeEngineDeferredShaders=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct EngineVertex {
 packed_float3 position,normal,sTangent,tTangent;
 packed_float2 uv[2]; ushort boneIds[4]; uchar boneWeights[4];
};
struct EngineUniform {
 float4x4 viewProjection,world,piece,normal;
	float4 team,nano,colorMultiplier,alphaControl,clip0,clip1,clip2,clip3;
 uint4 flags,image0,swizzle0,image1,swizzle1;
 float4 samplerParameters;
 float4 root0q,root0t,root1q,root1t,piece0q,piece0t,piece1q,piece1t;
 float4x4 shadow;
 float4 camera,sunDirection,sunAmbient,sunDiffuse,sunSpecular,fogColor,fogParameters;
 float4 lightingParameters;uint4 lightingFlags;uint4 lightImage,lightSwizzle;float4 lightSampler;
};
static_assert(sizeof(EngineUniform)==864,"Original engine uniform transport size differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,clip3)==368,"Original alpha plane3 uniform offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,flags)==384,"Original engine flags offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,samplerParameters)==464,"Original engine sample offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,root0q)==480,"Original engine transform offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,shadow)==608,"Original engine shadow offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,camera)==672,"Original engine camera offset differs from CPU");
static_assert(__builtin_offsetof(EngineUniform,lightSampler)==848,"Original engine sampler offset differs from CPU");
// Preserve clipping at vertex output without an array in fragment input.
struct EngineVarying {
 float4 position [[position]];
	float clipDistance [[clip_distance]] [4];
 float2 uv [[user(locn0), centroid_perspective]];
 float3 normal [[user(locn1)]];
 float4 worldPosition [[user(locn2)]];
 float4 shadowPosition [[user(locn3)]];
 float3 cameraDirection [[user(locn4)]];
 float fogFactor [[user(locn5)]];
};
struct EngineFragmentInput {
 float4 position [[position]];
 float2 uv [[user(locn0), centroid_perspective]];
 float3 normal [[user(locn1), RECOIL_SAMPLE_PERSPECTIVE]];
 float4 worldPosition [[user(locn2), RECOIL_SAMPLE_PERSPECTIVE]];
 float4 shadowPosition [[user(locn3), RECOIL_SAMPLE_PERSPECTIVE]];
 float3 cameraDirection [[user(locn4), RECOIL_SAMPLE_PERSPECTIVE]];
 float fogFactor [[user(locn5), RECOIL_SAMPLE_PERSPECTIVE]];
};
struct EngineOutputs {
 float4 normal [[color(0)]],diffuse [[color(1)]],specular [[color(2)]],emission [[color(3)]],misc [[color(4)]];
};
float3 OriginalRotate(float4 q,float3 v){return 2*dot(q.xyz,v)*q.xyz+(q.w*q.w-dot(q.xyz,q.xyz))*v+2*q.w*cross(q.xyz,v);}
float4 OriginalSLerp(float4 qa,float4 qb,float a){
 float c=dot(qa,qb),s=sign(c);qb*=s;c*=s;
 if(c>=1-.005f)return normalize(mix(qa,qb,a));
 float h=acos(c);return normalize(qa*sin((1-a)*h)+qb*sin(a*h));
}
vertex EngineVarying NativeEngineDeferredVertex(uint id [[vertex_id]],
 device const EngineVertex* vertices [[buffer(0)]],constant EngineUniform& u [[buffer(1)]]) {
 EngineVarying o;auto v=vertices[id];float4 model,world;float3 normal;
 if(u.flags.y){
  float a=0;float4 pq=u.piece0q,pt=u.piece0t;
  // Original static mode directly applies transforms[instData.w+bone0].
  // Only live prev/current transforms perform time interpolation/normalization.
  if(!u.flags.z){a=clamp(u.samplerParameters.z,0.f,1.f);pq=OriginalSLerp(u.piece0q,u.piece1q,a);pt=mix(u.piece0t,u.piece1t,a);}
  model=float4(OriginalRotate(pq,float3(v.position)*pt.w)+pt.xyz,1);normal=OriginalRotate(pq,float3(v.normal)*pt.w);
  if(!u.flags.z&&pt.w<=0){model=float4(0);normal=float3(0);}
		if(u.flags.z==1u){world=u.world*model;normal=(u.world*float4(normal,0)).xyz;}
		else if(u.flags.z==2u){world=float4(OriginalRotate(u.root0q,model.xyz*u.root0t.w)+u.root0t.xyz*model.w,model.w);normal=OriginalRotate(u.root0q,normal);}
  else{float4 rq=OriginalSLerp(u.root0q,u.root1q,a),rt=mix(u.root0t,u.root1t,a);world=float4(OriginalRotate(rq,model.xyz*rt.w)+rt.xyz*model.w,model.w);normal=OriginalRotate(rq,normal);}
  o.clipDistance[0]=dot(model,u.clip0);o.clipDistance[1]=dot(model,u.clip1);
 }else{
  model=u.piece*float4(float3(v.position),1);world=u.world*model;normal=(u.normal*float4(float3(v.normal),0)).xyz;
  o.clipDistance[0]=dot(world,u.clip0);o.clipDistance[1]=dot(world,u.clip1);
 }
 // Original GL4 clipPlane2 is WORLD-space; legacy retains actual GL eye equations.
 o.clipDistance[2]=dot(world,u.clip2);
	// Exact CWorldDrawer plane3 for the main below/above-water alpha halves.
	o.clipDistance[3]=dot(world,u.clip3);
 o.position=u.viewProjection*world;if(u.flags.x&1u)o.position.z=(o.position.z+o.position.w)*.5f;if(u.flags.x&2u)o.position.y=-o.position.y;
 o.uv=float2(v.uv[0]);o.normal=normal;o.worldPosition=world;
 o.shadowPosition=u.shadow*world;o.shadowPosition.xy+=.5f;
 o.cameraDirection=u.flags.y?u.camera.xyz-world.xyz:world.xyz-u.camera.xyz;
 o.fogFactor=clamp((u.fogParameters.y-length(o.cameraDirection))*u.fogParameters.w,0.f,1.f);return o;
}
float Channel(float4 c,uint code) {
 switch(code){case 0:return 0;case 1:return 1;case 0x1903:return c.r;case 0x1904:return c.g;case 0x1905:return c.b;default:return c.a;}
}
float4 OriginalSample(float2 uv,uint4 image,uint4 swizzle,float lodBias,
 texture2d<float> color,sampler state) {
 if(image.y)uv.y=1-uv.y;
 float4 value=color.sample(state,uv,bias(lodBias));
 return float4(Channel(value,swizzle.x),Channel(value,swizzle.y),Channel(value,swizzle.z),Channel(value,swizzle.w));
}
float4 OriginalSample(float2 uv,uint4 image,uint4 swizzle,float lodBias,
 depth2d<float> depth,sampler state) {
 if(image.y)uv.y=1-uv.y;
 float d=depth.sample(state,uv,bias(lodBias));
 float4 value=image.z==0x1906u?float4(0,0,0,d):image.z==0x8049u?float4(d):image.z==0x1903u?float4(d,0,0,1):float4(d,d,d,1);
 return float4(Channel(value,swizzle.x),Channel(value,swizzle.y),Channel(value,swizzle.z),Channel(value,swizzle.w));
}
bool OriginalAlphaCompare(float a,float b,uint op){switch(op){case 0:return false;case 1:return a<b;case 2:return a==b;case 3:return a<=b;case 4:return a>b;case 5:return a!=b;case 6:return a>=b;default:return true;}}
EngineOutputs EngineDeferred(EngineFragmentInput in,constant EngineUniform& u
 ENGINE_IMAGE0_PARAMS ENGINE_IMAGE1_PARAMS,bool modern) {
 float4 texture1=ENGINE_SAMPLE0(in.uv);
 float4 texture2=ENGINE_SAMPLE1(in.uv);
 float alpha=u.team.a*(modern?float(texture2.a>=.5f):texture2.a);
 if(modern && (float(alpha>u.alphaControl.x)*u.alphaControl.y+float(alpha<u.alphaControl.x)*u.alphaControl.z+u.alphaControl.w)==0)discard_fragment();
 float3 diffuse=mix(texture1.rgb,u.team.rgb,texture1.a);diffuse=mix(diffuse,u.nano.rgb,u.nano.a);
 EngineOutputs out;
 // Legacy alpha test sees attachment0 alpha=1; it must NOT discard tex2.a=0.
 out.normal=modern?float4(in.normal*.5f+.5f,1):float4((normalize(in.normal)+1)*.5f,1);
 out.diffuse=float4(diffuse,alpha)*(modern?u.colorMultiplier:float4(1));
 out.specular=float4(texture2.rgb,alpha);out.emission=float4(0);out.misc=float4(0);
 if(u.lightingFlags.y&&!OriginalAlphaCompare(out.normal.a,u.lightingParameters.y,u.lightingFlags.z))discard_fragment();return out;
}
fragment EngineOutputs NativeEngineDeferredGL4(EngineFragmentInput in [[stage_in]],constant EngineUniform& u [[buffer(1)]]
 ENGINE_IMAGE0_ARGS ENGINE_IMAGE1_ARGS RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 return EngineDeferred(in,u ENGINE_IMAGE0_PASS ENGINE_IMAGE1_PASS,true);
}
fragment EngineOutputs NativeEngineDeferredLegacy(EngineFragmentInput in [[stage_in]],constant EngineUniform& u [[buffer(1)]]
 ENGINE_IMAGE0_ARGS ENGINE_IMAGE1_ARGS RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 return EngineDeferred(in,u ENGINE_IMAGE0_PASS ENGINE_IMAGE1_PASS,false);
}
)MSL";
} // namespace rendering::metal4
