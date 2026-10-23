/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* AirjetsMSL=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"AIR(
#include <metal_stdlib>
using namespace metal;
struct Transform {float4 q,t;};
struct Model {uint4 metadata;float4 health;float4 drawPos,speed;float4 user[4];};
struct Published {Transform root0,root1,piece0,piece1;Model model;};
static_assert(sizeof(Published)==256,"Airjets publication ABI");
struct Instance {packed_float3 size,emitdir,color;uint piece;packed_uint4 instData;packed_float3 jet;};
static_assert(sizeof(Instance)==68,"Airjets instance ABI");
struct Uniform {float4x4 vp,reflectionVP,viewInv;float4 time,alpha;uint4 options;};
struct Out {float4 position [[position]];float4 uv [[RECOIL_SAMPLE_PERSPECTIVE]],color [[RECOIL_SAMPLE_PERSPECTIVE]];float atlas [[flat]];};
Transform TXLerp(Transform a,Transform b,float t){t=clamp(t,0.0,1.0);if(dot(a.q,b.q)<0.0)b.q=-b.q;return {normalize(mix(a.q,b.q,t)),mix(a.t,b.t,t)};}
float3 Rotate(float4 q,float3 v){return 2.0*dot(q.xyz,v)*q.xyz+(q.w*q.w-dot(q.xyz,q.xyz))*v+2.0*q.w*cross(q.xyz,v);}
Transform Compose(Transform a,Transform b){return {float4(a.q.w*b.q.xyz+b.q.w*a.q.xyz+cross(a.q.xyz,b.q.xyz),a.q.w*b.q.w-dot(a.q.xyz,b.q.xyz)),float4(a.t.xyz+Rotate(a.q,a.t.w*b.t.xyz),a.t.w*b.t.w)};}
float4x4 Matrix(Transform a){float4 q=a.q;float xx=q.x*q.x,yy=q.y*q.y,zz=q.z*q.z,xz=q.x*q.z,xy=q.x*q.y,yz=q.y*q.z,wx=q.w*q.x,wy=q.w*q.y,wz=q.w*q.z;
 return float4x4(float4(float3(1.0-2.0*(yy+zz),2.0*(xy+wz),2.0*(xz-wy))*a.t.w,0),float4(float3(2.0*(xy-wz),1.0-2.0*(xx+zz),2.0*(yz+wx))*a.t.w,0),float4(float3(2.0*(xz+wy),2.0*(yz-wx),1.0-2.0*(xx+yy))*a.t.w,0),float4(a.t.xyz,1));}
vertex Out AirjetsVertex(uint id [[vertex_id]],uint instance [[instance_id]],device const packed_float4* vertices [[buffer(0)]],device const Instance* instances [[buffer(1)]],device const Published* pubs [[buffer(2)]],constant Uniform& u [[buffer(3)]]){
 const Instance s=instances[instance];const Published pub=pubs[instance];float4 input=float4(vertices[id]);Out o={};o.color=float4(float3(s.color),clamp((u.time.x+u.time.w-s.size.z)*.053,0.0,1.0));o.atlas=s.jet.z;
 o.uv=float4(input.zw,input.zw);o.uv.w+=(u.time.x+u.time.w)*.1;
 float4x4 world=Matrix(Compose(TXLerp(pub.root0,pub.root1,u.time.w),TXLerp(pub.piece0,pub.piece1,u.time.w)));
 float2 size=float2(s.size.xy)*1.5;size.y*=clamp(pub.model.speed.y*.5*s.jet.y+1.0,.66,2.0);size.y*=clamp(length(pub.model.speed.xz)*.5*s.jet.x+1.0,.33,4.0);
 float2 offset=input.xy*float2(size.x*2.0,size.y*.66);float3 axis=world[2].xyz;float3 worldPos=world[3].xyz+offset.x*world[0].xyz+offset.y*axis;
 float3 width=cross(u.viewInv[3].xyz-worldPos,axis);width*=length(world[0].xyz)*rsqrt(dot(width,width));
 o.position=(u.options.x?u.reflectionVP:u.vp)*float4(world[3].xyz+offset.x*width+offset.y*axis,1);
 if((pub.model.metadata.x&1u)==0u||o.color.a==0.0)o.position=float4(2,2,2,1);
 if(u.options.x){o.color.a*=3.0;if(worldPos.y < -5.0)o.color=float4(0);}
 if(!u.options.y)o.position.z=(o.position.z+o.position.w)*.5;return o;
}
float4 JetSample(texture2d<float> image,sampler state,float2 uv,bool flip){if(flip)uv.y=1.0-uv.y;return image.sample(state,uv);}
fragment float4 AirjetsFragment(Out in [[stage_in]],constant Uniform& u [[buffer(3)]],texture2d<float> noise [[texture(0)]],texture2d<float> mask [[texture(1)]],sampler noiseSampler [[sampler(0)]],sampler maskSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 displacement=in.uv.zw,cell=in.uv.xy;
 float distortion=JetSample(mask,maskSampler,float2((in.atlas+cell.x)/8.0,cell.y),u.options.w&2u).g*.01;
 float2 tx=cell;tx.x+=(JetSample(noise,noiseSampler,displacement*.01*20.0,u.options.w&1u).y-.5)*40.0*distortion;
 tx.y+=JetSample(noise,noiseSampler,displacement,u.options.w&1u).x*(1.0-cell.y)*15.0*distortion;
 float2 atlas=float2((in.atlas+clamp(tx.x,0.0,1.0))/8.0,tx.y);float opac=JetSample(mask,maskSampler,atlas,u.options.w&2u).r;if(opac<=0.0)discard_fragment();
 float opac2=opac*opac;float4 color=float4(opac*in.color.rgb+opac2*opac2*opac,opac*1.5);color=clamp(color,0.0,1.0)*in.color.a;
 if(u.options.z){float a=color.a,b=u.alpha.x;bool pass;switch(uint(u.alpha.y)){case 0:pass=false;break;case 1:pass=a<b;break;case 2:pass=a==b;break;case 3:pass=a<=b;break;case 4:pass=a>b;break;case 5:pass=a!=b;break;case 6:pass=a>=b;break;default:pass=true;}if(!pass)discard_fragment();}return color;
}
)AIR";
}
