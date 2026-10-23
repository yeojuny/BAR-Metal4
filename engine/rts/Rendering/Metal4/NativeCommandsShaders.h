/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string_view>
namespace rendering::metal4 {
// Exact30770 gui_commands_fx.lua complete GLSL330 vertex/fragment arithmetic.
// Source CPU queue filtering, fading and 48-byte instance records stay in Lua.
inline constexpr std::string_view NativeCommandsShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"metal(
#include <metal_stdlib>
using namespace metal;
struct CommandsUniform{float4x4 view,projection;float4 values;uint4 options;};
struct CommandsInstance{float4 posStart,posEndW,colorTex;};
struct CommandsOutput{float4 position [[position]];float4 color [[RECOIL_SAMPLE_PERSPECTIVE]];float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex CommandsOutput OriginalCommandsVertex(uint vertexID [[vertex_id]],uint instanceID [[instance_id]],
 const device float2* corners [[buffer(0)]],const device CommandsInstance* instances [[buffer(1)]],constant CommandsUniform& u [[buffer(2)]]){
 const float2 a_corner=corners[vertexID];const auto a=instances[instanceID];
 const float3 sPos=a.posStart.xyz,ePos=float3(a.posStart.w,a.posEndW.xy);
 const float w=a.posEndW.z;const float2 dXZ=ePos.xz-sPos.xz;const float lenXZ=length(dXZ);
 const float2 perpXZ=lenXZ>0.001?float2(-dXZ.y,dXZ.x)/lenXZ:float2(0,1);
 const float side=a_corner.x*2.0-1.0,along=a_corner.y;
 float3 pos;pos.xz=mix(sPos.xz,ePos.xz,along)+perpXZ*w*0.5*side;pos.y=mix(sPos.y,ePos.y,along);
 CommandsOutput o;o.position=u.projection*u.view*float4(pos,1);
 if(u.options.z)o.position.z=(o.position.z+o.position.w)*0.5;
 o.color=float4(a.colorTex.rgb,a.posEndW.w);
 const float dist3D=length(ePos-sPos),texLen=dist3D/(u.values.x*u.values.y);
 o.uv=float2((1.0-along)*(texLen+1.0)-a.colorTex.w,a_corner.x);return o;
}
fragment float4 OriginalCommandsFragment(CommandsOutput in [[stage_in]],constant CommandsUniform& u [[buffer(2)]]
#if COMMANDS_IMAGE==1
 ,texture2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]]
#elif COMMANDS_IMAGE==2
 ,depth2d<float> image [[texture(0)]],sampler imageSampler [[sampler(0)]]
#endif
  RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float4 result=in.color;
 if(u.options.x){float2 uv=in.uv;if(u.options.y)uv.y=1.0-uv.y;
#if COMMANDS_IMAGE==1
 result*=image.sample(imageSampler,uv);
#elif COMMANDS_IMAGE==2
 const float d=image.sample(imageSampler,uv);result*=float4(d,d,d,1);
#else
 result*=float4(0,0,0,1);
#endif
 }
 if(u.options.w&256){const float a=result.a,ref=u.values.z;const uint op=u.options.w&255;
 const bool pass=op==0?false:op==1?a<ref:op==2?a==ref:op==3?a<=ref:op==4?a>ref:op==5?a!=ref:op==6?a>=ref:true;
 if(!pass)discard_fragment();}
 return result;
}
)metal";
}
