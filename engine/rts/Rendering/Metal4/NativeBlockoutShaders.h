/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
namespace rendering::metal4 {
inline constexpr const char* NativeBlockoutShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
constant bool BlockoutHasNormal [[function_constant(0)]];
struct BlockoutVertexData{packed_float3 position,normal,tangent,bitangent;packed_float2 uv;uint piece;};
struct BlockoutTransform{float4 quaternion,translationScale;};
struct BlockoutUniform{
	float4x4 viewProjection;
	float4 positionYaw,parameters,sunDirection,ambient,diffuse;
	uint4 options; // clip conversion, model0/model1/normal V-flips; high bit normal enabled
};
struct BlockoutOut{
	float4 position [[position]];
	float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];
	float3 normal [[RECOIL_SAMPLE_PERSPECTIVE]],tangent [[RECOIL_SAMPLE_PERSPECTIVE]],bitangent [[RECOIL_SAMPLE_PERSPECTIVE]];
};
float3 BlockoutQuaternion(float4 q,float3 v){
	return 2.f*dot(q.xyz,v)*q.xyz+(q.w*q.w-dot(q.xyz,q.xyz))*v+2.f*q.w*cross(q.xyz,v);
}
vertex BlockoutOut BlockoutVertex(uint id [[vertex_id]],
	device const BlockoutVertexData* vertices [[buffer(0)]],
	device const BlockoutTransform* transforms [[buffer(1)]],
	constant BlockoutUniform& u [[buffer(2)]])
{
	const auto v=vertices[id];const auto tx=transforms[v.piece&255u];
	float3 local=BlockoutQuaternion(tx.quaternion,float3(v.position)*tx.translationScale.w)+tx.translationScale.xyz;
	local*=u.parameters.x;
	// Exact rotation3dY(-worldposyaw.w). In the original quaternion branch
	// pieceRot stays identity for normal/T/B; do not rotate them by tx.
	const float s=sin(-u.positionYaw.w),c=cos(-u.positionYaw.w);
	const float3x3 rot(c,0.f,-s,0.f,1.f,0.f,s,0.f,c);
	BlockoutOut out;out.position=u.viewProjection*float4(rot*local+u.positionYaw.xyz,1.f);
	if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;
	out.uv=float2(v.uv);out.normal=rot*float3(v.normal);
	out.tangent=rot*float3(v.tangent);out.bitangent=rot*float3(v.bitangent);return out;
}
float4 BlockoutColor(BlockoutOut in,constant BlockoutUniform& u,
	texture2d<float> tex1,sampler sampler1,texture2d<float> tex2,sampler sampler2,
	texture2d<float> normalTex,sampler normalSampler)
{
	float2 uv1=in.uv,uv2=in.uv,uvn=in.uv;
	if(u.options.y)uv1.y=1.f-uv1.y;if(u.options.z)uv2.y=1.f-uv2.y;
	if(u.options.w&1u)uvn.y=1.f-uvn.y;
	const float4 color=tex1.sample(sampler1,uv1),other=tex2.sample(sampler2,uv2);
	if(other.a<.5f)discard_fragment();
	float3 N=normalize(in.normal);
	if(BlockoutHasNormal && (u.options.w&256u)){
		const float3 tn=normalTex.sample(normalSampler,uvn).xyz*2.f-1.f;
		N=normalize(normalize(in.tangent)*tn.x+normalize(in.bitangent)*tn.y+N*tn.z);
	}
	const float ndl=max(dot(N,normalize(u.sunDirection.xyz)),0.f);
	float3 lit=color.rgb*(u.ambient.rgb+u.diffuse.rgb*ndl);
	lit+=color.rgb*other.r;return float4(lit,1.f);
}
fragment float4 BlockoutFragment(BlockoutOut in [[stage_in]],constant BlockoutUniform& u [[buffer(2)]],
	texture2d<float> tex1 [[texture(0)]],sampler sampler1 [[sampler(0)]],
	texture2d<float> tex2 [[texture(1)]],sampler sampler2 [[sampler(1)]],
	texture2d<float> normalTex [[texture(2), function_constant(BlockoutHasNormal)]],sampler normalSampler [[sampler(2), function_constant(BlockoutHasNormal)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));return BlockoutColor(in,u,tex1,sampler1,tex2,sampler2,normalTex,normalSampler);}
struct BlockoutDepthOut{float color [[color(0)]];float depth [[depth(any)]];};
fragment BlockoutDepthOut BlockoutDepthFragment(BlockoutOut in [[stage_in]],constant BlockoutUniform& u [[buffer(2)]],
	texture2d<float> tex1 [[texture(0)]],sampler sampler1 [[sampler(0)]],
	texture2d<float> tex2 [[texture(1)]],sampler sampler2 [[sampler(1)]],
	texture2d<float> normalTex [[texture(2), function_constant(BlockoutHasNormal)]],sampler normalSampler [[sampler(2), function_constant(BlockoutHasNormal)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	// Keep the same alpha cut-out and R8 color writes on the depth redraw.
	const auto color=BlockoutColor(in,u,tex1,sampler1,tex2,sampler2,normalTex,normalSampler);
	BlockoutDepthOut out;out.color=color.r;
	// The original attachment is D24. A D32 native storage type must retain
	// D24 depth quantization at each fragment before its LEQUAL/write.
	out.depth=floor(clamp(in.position.z,0.f,1.f)*16777215.f+.5f)/16777215.f;return out;
}
)MSL";
}
