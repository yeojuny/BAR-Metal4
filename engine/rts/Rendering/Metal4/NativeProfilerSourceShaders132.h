/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr char ProfilerSourceShader132[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ProfilerUniform132 {float4 box,view,color,viewport,target;uint4 options;};
inline float4 ProfilerOriginalVertex132(float2 coords,constant ProfilerUniform132& u){
 float2 screenpos=float2(coords.x*(u.box.z-u.box.x)+u.box.x,coords.y*(u.box.w-u.box.y)+u.box.y);
 screenpos=screenpos/u.view.xy;
 screenpos-=0.5f;
 return float4(screenpos,u.options.w?0.5f:0.75f,1.0f);
}
struct ProfilerSegment132 {float4 a,b;bool valid;};
inline bool ProfilerClip132(float a,float b,thread float& lo,thread float& hi){
 if(a<0&&b<0)return false;if(a<0||b<0){float t=a/(a-b);if(a<0)lo=max(lo,t);else hi=min(hi,t);}return lo<=hi;
}
inline ProfilerSegment132 ProfilerLine132(uint segment,device const float2* source,constant ProfilerUniform132& u){
 ProfilerSegment132 out={};float4 a=ProfilerOriginalVertex132(source[segment],u),b=ProfilerOriginalVertex132(source[segment+1],u);
 if(!all(isfinite(a))||!all(isfinite(b)))return out;float lo=0,hi=1;
 for(uint k=0;k<2;++k)if(!ProfilerClip132(1+a[k],1+b[k],lo,hi)||!ProfilerClip132(1-a[k],1-b[k],lo,hi))return out;
 out.a=mix(a,b,lo);out.b=mix(a,b,hi);out.valid=true;return out;
}
struct ProfilerOut132 {float4 position [[position]];float4 endpoints [[flat]];};
inline ProfilerOut132 ProfilerCorner132(ProfilerSegment132 line,constant ProfilerUniform132& u,uint corner){
 ProfilerOut132 out={};out.position=float4(2,2,2,1);if(!line.valid)return out;
 float2 a=u.viewport.xy+(float2(line.a.x,-line.a.y)+1.f)*.5f*u.viewport.zw,b=u.viewport.xy+(float2(line.b.x,-line.b.y)+1.f)*.5f*u.viewport.zw;
 float2 delta=b-a;float len=length(delta);if(!(len>0))return out;float2 axis=delta/len,normal=float2(-axis.y,axis.x);
 bool end=(corner&2)!=0,positive=(corner&1)!=0;
 float radius=u.options.x>1?u.target.z*.5f:floor(u.target.z+.5f)*.5f+1.5f,extension=u.options.x>1?0.f:1.f;
 float2 p=(end?b:a)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 out.position=float4(2*p.x/u.target.x-1,1-2*p.y/u.target.y,line.a.z,1);out.endpoints=float4(a.x,u.target.y-a.y,b.x,u.target.y-b.y);return out;
}
vertex ProfilerOut132 ProfilerSourceVertex132(uint corner [[vertex_id]],uint segment [[instance_id]],device const float2* source [[buffer(0)]],constant ProfilerUniform132& u [[buffer(1)]]){
 return ProfilerCorner132(ProfilerLine132(segment,source,u),u,corner);
}
)MSL";
// The verified diamond coverage helper is concatenated before this fragment.
inline constexpr char ProfilerSourceFragment132[]=R"MSL(
fragment float4 ProfilerSourceFragment132(ProfilerOut132 in [[stage_in]],constant ProfilerUniform132& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 if(u.options.x==1&&!SourceLineCoverage130(in.endpoints.xy,in.endpoints.zw,float2(in.position.x,u.target.y-in.position.y),u.target.z))discard_fragment();
 if(u.options.y){float a=u.color.a,b=u.target.w;bool pass;
  switch(u.options.z){case 0u:pass=false;break;case 1u:pass=a<b;break;case 2u:pass=a==b;break;case 3u:pass=a<=b;break;case 4u:pass=a>b;break;case 5u:pass=a!=b;break;case 6u:pass=a>=b;break;default:pass=true;break;}if(!pass)discard_fragment();}
 return u.color;
}
)MSL";
}
