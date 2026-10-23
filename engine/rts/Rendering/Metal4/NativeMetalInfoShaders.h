/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4::metalinfo {
// Exact test30770 shaders/glsl/infometal.lua and original engine auxiliary stages.
inline constexpr const char* SHADER=R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Options {uint4 flags;};
kernel void MetalGroundLOS(uint2 p [[thread_position_in_grid]],constant Options& u [[buffer(0)]],texture2d<uint,access::read> counts [[texture(0)]],texture2d<float,access::write> result [[texture(1)]]) {
	if(p.x>=result.get_width()||p.y>=result.get_height())return;
	result.write(float4(float(u.flags.x!=0||counts.read(p).r>0)),p);
}
kernel void MetalExtraction(uint2 p [[thread_position_in_grid]],texture2d<float,access::read> raw [[texture(0)]],texture2d<float> los [[texture(1)]],texture2d<float,access::write> result [[texture(2)]]) {
	if(p.x>=result.get_width()||p.y>=result.get_height())return;
	constexpr sampler filter(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
	const float2 uv=(float2(p)+.5f)/float2(result.get_width(),result.get_height());
	const float lod=max(log2(max(float(los.get_width())/result.get_width(),float(los.get_height())/result.get_height())),0.f);
	// Original GL_ZERO/GL_SRC_COLOR blends raw destination * (sampled LOS*800).
	const float extraction=raw.read(p).r*(los.sample(filter,uv,level(round(lod))).r*800.f);
	result.write(float4(extraction,0,0,1),p);
}
struct Varying {float4 position [[position]];float2 texCoord;};
vertex Varying MetalInfoVertex(uint id [[vertex_id]]) {
	const float2 uv=id==0?float2(0,0):id==1?float2(2,0):float2(0,2);
	return {float4(uv.x*2-1,1-uv.y*2,0,1),uv};
}
struct Parameters {float4 values;};
float4 bicubicSample(texture2d<float> tex,float2 UV) {
	constexpr sampler densityFilter(coord::normalized,address::clamp_to_edge,filter::linear);
	float2 texSize=float2(tex.get_width(),tex.get_height());
	float2 texSizeInv=.33f/texSize;
	float4 result=float4(0);
	result+=tex.sample(densityFilter,float2(UV.x+texSizeInv.x,UV.y+texSizeInv.y));
	result+=tex.sample(densityFilter,float2(UV.x-texSizeInv.x,UV.y+texSizeInv.y));
	result+=tex.sample(densityFilter,float2(UV.x-texSizeInv.x,UV.y-texSizeInv.y));
	result+=tex.sample(densityFilter,float2(UV.x+texSizeInv.x,UV.y-texSizeInv.y));
	return result*.25f;
}
fragment float4 MetalInfoFragment(Varying input [[stage_in]],constant Parameters& u [[buffer(0)]],texture2d<float> tex0 [[texture(0)]],texture2d<float> tex1 [[texture(1)]]) {
	constexpr sampler extractionFilter(coord::normalized,address::clamp_to_edge,min_filter::nearest,mag_filter::linear);
	float4 color=float4(0,0,0,1);
	float4 metal=bicubicSample(tex0,input.texCoord);
	float4 extraction=tex1.sample(extractionFilter,input.texCoord);
	color.rgb=mix(float3(0,1,.6),float3(.9,0,0),extraction.r);
	color.rgb=mix(float3(0),color.rgb,metal.r);
	color.rgb=mix(float3(0),color.rgb,u.values.x*1.f);
	color.a=.25f;
	return color;
}
)MSL";
}
