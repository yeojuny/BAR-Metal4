/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include <string_view>
namespace rendering::metal4::snow {
inline constexpr std::string_view SHADER=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"metal(
#include <metal_stdlib>
using namespace metal;
struct SnowUniform {
	float4x4 modelView, projection;
	float4 snowColor, speed, camPos;
	float4 params; // time, scale, sizeMult, inherited alpha reference
	float4 viewport, target; // actual source viewport and full attachment extent
	uint4 options; // clip convention, alpha-test enabled, compare, pointCoord flip
};
struct SnowOutput {
	float4 position [[position]];
	float2 pointCoord [[RECOIL_SAMPLE_PERSPECTIVE]];
	float4 vertexColor [[flat]];
};
float3 SnowMod(float3 x,float y) { return x-y*floor(x/y); }
vertex SnowOutput SnowVertexMain(uint vertexID [[vertex_id]],device const float4* points [[buffer(0)]],constant SnowUniform& u [[buffer(1)]])
{
	const float4 source=points[vertexID/6];
	const float time=u.params.x, scale=u.params.y;
	const float3 scalePos=source.xyz*scale;
	SnowOutput output;
	output.vertexColor=float4(u.snowColor.rgb,u.snowColor.a*cos(scalePos.y));
	float3 pos=scalePos-SnowMod(u.camPos.xyz,scale);
	pos.y-=time*0.5*(u.speed.x*(2.0+source.w));
	pos.x+=(sin(time*1.5)*3.0)+u.speed.y;
	pos.z+=(cos(time*1.5)*3.0)+u.speed.z;
	if(pos.x>=1.0) pos.x-=1.0;
	if(pos.x<0.0) pos.x+=1.0;
	if(pos.z>=1.0) pos.z-=1.0;
	if(pos.z<0.0) pos.z+=1.0;
	pos=SnowMod(pos,scale)-(scale*0.5)+u.camPos.xyz;
	const float4 eyePos=u.modelView*float4(pos,1.0);
	// Shader point-size mode bypasses fixed PointSize and distance attenuation.
	// Family-local expansion preserves footprints beyond Metal's511-pixel point
	// primitive limit, without adding an aesthetic/driver-independent maximum.
	const float size=(1.0+source.w)*5000.0*u.params.z/length(eyePos);
	float4 center=u.projection*eyePos;
	const bool visible=center.w>0.0&&abs(center.x)<=center.w&&abs(center.y)<=center.w&&
		center.z<=(center.w)&&center.z>=(u.options.x!=0?-center.w:0.0);
	const float2 corners[6]={float2(0,0),float2(0,1),float2(1,0),float2(1,0),float2(0,1),float2(1,1)};
	output.pointCoord=corners[vertexID%6];
	if(!visible||size<=0.0) {output.position=float4(2,2,2,1);return output;}
	if(u.options.x!=0) center.z=(center.z+center.w)*0.5;
	const float2 centerPixel=float2(u.viewport.x+(center.x/center.w+1.0)*u.viewport.z*0.5,
		u.viewport.y+(1.0-center.y/center.w)*u.viewport.w*0.5);
	const float2 pixel=centerPixel+(output.pointCoord-0.5)*size;
	// Use the full attachment viewport after explicit original center clipping.
	// GL point footprints may extend outside their source viewport rectangle.
	output.position=float4((pixel.x/u.target.x*2.0-1.0)*center.w,
		(1.0-pixel.y/u.target.y*2.0)*center.w,center.z,center.w);
	return output;
}
fragment float4 SnowFragmentMain(SnowOutput input [[stage_in]],constant SnowUniform& u [[buffer(1)]],texture2d<float> tex0 [[texture(0)]],sampler originalSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT)
{
	RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
	float2 pointCoord=input.pointCoord;
	if(u.options.w!=0) pointCoord.y=1.0-pointCoord.y;
	const float4 result=tex0.sample(originalSampler,pointCoord)*input.vertexColor;
	if(u.options.y!=0) {
		const float a=result.a,r=u.params.w; bool pass=false;
		switch(u.options.z) {
			case 0: pass=false;break; case 1: pass=a<r;break; case 2: pass=a==r;break;
			case 3: pass=a<=r;break; case 4: pass=a>r;break; case 5: pass=a!=r;break;
			case 6: pass=a>=r;break; case 7: pass=true;break;
		}
		if(!pass) discard_fragment();
	}
	return result;
}
)metal";
}
