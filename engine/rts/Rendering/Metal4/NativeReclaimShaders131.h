/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr char ReclaimFieldShader131[]=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Uniform {float4x4 viewProj,viewInv;float4 fillMetal,fillEnergy,edgeMetal,edgeEnergy,togglesOffsetGradient,fadeEnergy;uint4 options;float4 alpha;};
struct Out {float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];};
inline Out OriginalReclaimVertex131(uint id,device const float4* vertices,device const float4* params,constant Uniform& u){
	uint chunk=id/u.options.x;float4 p0=params[2*chunk],p1=params[2*chunk+1];float4 posKind=vertices[id];
	bool energy=p1.y>0.5f;float alpha=p1.x*(energy?u.togglesOffsetGradient.y:u.togglesOffsetGradient.x);
	float3 pos=posKind.xyz;pos.xz=p0.xz+(pos.xz-p0.xz)*p0.w;pos.y+=u.togglesOffsetGradient.z;
	float dist=distance(u.viewInv[3].xyz,p0.xyz);float fade=clamp(1.0f-(dist-u.fadeEnergy.x)/(u.fadeEnergy.y-u.fadeEnergy.x),0.0f,1.0f);fade=max(fade,p1.z);
	float4 fill=energy?u.fillEnergy:u.fillMetal,edge=energy?u.edgeEnergy:u.edgeMetal;
	float kind=posKind.w;float baseAlpha=kind<0.5f?fill.a:kind<1.5f?u.togglesOffsetGradient.w:edge.a;
	alpha*=baseAlpha*fade*(energy?u.fadeEnergy.z:1.0f);Out out;out.color=float4(kind<1.5f?fill.rgb:edge.rgb,alpha);
	out.position=alpha<=0.0005f?float4(-2,-2,-2,1):u.viewProj*float4(pos,1);
	if(!u.options.y)out.position.z=(out.position.z+out.position.w)*0.5f;
	return out;
}
vertex Out ReclaimFieldVertex(uint id [[vertex_id]],device const float4* vertices [[buffer(0)]],device const float4* params [[buffer(1)]],constant Uniform& u [[buffer(2)]]){return OriginalReclaimVertex131(id,vertices,params,u);}

fragment float4 ReclaimFieldFragment(Out in [[stage_in]],constant Uniform& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	if(u.options.z){float a=in.color.a,b=u.alpha.x;bool pass;
		switch(u.options.w){case 0u:pass=false;break;case 1u:pass=a<b;break;case 2u:pass=a==b;break;case 3u:pass=a<=b;break;case 4u:pass=a>b;break;case 5u:pass=a!=b;break;case 6u:pass=a>=b;break;default:pass=true;break;}
		if(!pass)discard_fragment();}
	return in.color;
}
)MSL";
}
