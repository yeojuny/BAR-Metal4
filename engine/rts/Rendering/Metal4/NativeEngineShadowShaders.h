/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Appended to the existing original rigid vertex ABI/TRS/sampling helpers.
// Original ShadowGenVertProg{,GL4}, not a normal lighting shader substitute.
inline constexpr const char* NativeEngineShadowShaders=R"MSL(
struct OriginalShadowVarying {float4 position [[position]];float clipDistance [[clip_distance]] [2];float2 uv [[user(locn0)]];};
struct OriginalShadowFragmentInput {float2 uv [[user(locn0)]];};
vertex OriginalShadowVarying OriginalShadowVertex(uint id [[vertex_id]],device const EngineVertex* vertices [[buffer(0)]],constant EngineUniform& u [[buffer(1)]]){
 auto v=vertices[id];OriginalShadowVarying o;float4 light;float3 normal;
 if(u.flags.y){
  float a=clamp(u.samplerParameters.z,0.f,1.f);
  float4 pq=OriginalSLerp(u.piece0q,u.piece1q,a),pt=mix(u.piece0t,u.piece1t,a);
  float4 model=float4(OriginalRotate(pq,float3(v.position)*pt.w)+pt.xyz,1);normal=OriginalRotate(pq,float3(v.normal)*pt.w);
  // Original rigid branch suppresses nonpositive piece scale by a zero
  // weight followed by its original zero weight-sum division.
  float weight=float(pt.w>0);model=model*weight/weight;normal=normal*weight/weight;
  float4 rq=OriginalSLerp(u.root0q,u.root1q,a),rt=mix(u.root0t,u.root1t,a);
  float4 world=float4(OriginalRotate(rq,model.xyz*rt.w)+rt.xyz*model.w,model.w);normal=OriginalRotate(rq,normal);
  o.clipDistance[0]=dot(model,u.clip0);o.clipDistance[1]=dot(model,u.clip1);
  light=u.world*world;normal=normalize((u.world*float4(normal,0)).xyz);
  float nl=clamp(normal.z,0.f,1.f);light.xy+=.5f;light.z+=1e-5f*clamp(tan(acos(nl)),0.f,30.f);
 }else{
  float4 sourceVertex=float4(float3(v.position),1);float4x4 mv=u.world*u.piece;
  light=mv*sourceVertex;normal=normalize((mv*float4(float3(v.normal),0)).xyz);
  // Original legacy shader explicitly assigns raw gl_Vertex to gl_ClipVertex.
  o.clipDistance[0]=dot(sourceVertex,u.clip0);o.clipDistance[1]=dot(sourceVertex,u.clip1);
  float nl=clamp(normal.z,0.f,1.f);light.xy+=.5f;light.z+=clamp(1e-6f*tan(acos(nl)),0.f,100e-6f);
 }
 o.position=u.viewProjection*light;if(u.flags.x&1u)o.position.z=(o.position.z+o.position.w)*.5f;if(u.flags.x&2u)o.position.y=-o.position.y;
 o.uv=float2(v.uv[0]);return o;
}
fragment void OriginalShadowGL4(OriginalShadowFragmentInput in [[stage_in]],constant EngineUniform& u [[buffer(1)]] ENGINE_IMAGE0_ARGS){
 float alpha=ENGINE_SAMPLE0(in.uv).a;
 if(float(alpha>u.alphaControl.x)*u.alphaControl.y+float(alpha<u.alphaControl.x)*u.alphaControl.z+u.alphaControl.w==0)discard_fragment();
}
// Original ShadowGenFragProg has its alpha-mask discard compiled out and no
// color store. The accepted Mesa fixed-alpha lowering adds discards at color
// stores only, so this exact stage has no color write or fixed-alpha discard.
fragment void OriginalShadowLegacy(){}
// Original ShadowGenVertMapProg: selected local Basic/ROAM positions and the
// retained heightMapTex, including the original border marker and texel offset.
struct OriginalTerrainShadowUniform {
 float4x4 modelView,projection;
 float4 mapSize,parameters;
 float4 clips[3];
 uint4 flags;
};
struct OriginalTerrainShadowVarying {
 float4 position [[position]];
 float clipDistance [[clip_distance]] [3];
};
vertex OriginalTerrainShadowVarying OriginalTerrainShadowVertex(uint id [[vertex_id]],
 device const packed_float3* vertices [[buffer(0)]],constant OriginalTerrainShadowUniform& u [[buffer(1)]],
 texture2d<float> heightMap [[texture(0)]],sampler heightSampler [[sampler(0)]]) {
 float4 world=float4(float3(vertices[id]),1);
 world.xz+=u.parameters.xy*1024.0f;
 float2 wxz=world.xz;
 wxz+=-float2(8.0f)*(wxz*u.mapSize.zw)+float2(4.0f);
 float2 uv=clamp(wxz,float2(8.0f),u.mapSize.xy-float2(8.0f))*u.mapSize.zw;
 if(u.flags.z)uv.y=1.0f-uv.y;
 float height=Channel(heightMap.sample(heightSampler,uv,level(0.0f)),u.flags.w);
 world.y=mix(u.parameters.z,height,float(world.y==0.0f));
 float4 light=u.modelView*world;light.xy+=.5f;
 OriginalTerrainShadowVarying out;out.position=u.projection*light;
 if(u.flags.x)out.position.z=(out.position.z+out.position.w)*.5f;
 if(u.flags.y)out.position.y=-out.position.y;
 for(uint plane=0;plane<3;++plane)out.clipDistance[plane]=dot(world,u.clips[plane]);
 return out;
}
)MSL";
}
