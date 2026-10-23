/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Exact test30770 gfx_volumetric_clouds.lua active shader equations. Its
// #ifdef DEPTH_CLIP01 branch is selected even when the original define is 0;
// its #ifndef CLAMP_TO_MAP horizon branch is never selected. Do not silently
// "correct" these source-selected behaviors while translating the family.
inline constexpr const char* NATIVE_VOLUMETRIC_CLOUD_SHADER = R"CLOUDMSL(
#include <metal_stdlib>
using namespace metal;
struct CloudUniforms {
	float4x4 inverseViewProjection;
	float4 eye,offset,sunDirection,sunColor;
	float4 scaleHeightBottomFade;
	float4 fogColorOpacity;
	float4 mapSunTime;
	uint4 mode;
};
struct CloudVarying {
	float4 position [[position]];
	float2 uv;
	float3 cameraMin,cameraMax;
};
vertex CloudVarying CloudVertex(uint ordinal [[vertex_id]],constant CloudUniforms& u [[buffer(0)]])
{
	constexpr uint indices[6]={0,1,2,0,2,3};
	constexpr float2 corners[4]={float2(-1,-1),float2(1,-1),float2(1,1),float2(-1,1)};
	constexpr float4 frustumCorners[8]={
		float4(-1,1,-1,1),float4(1,1,-1,1),float4(1,-1,-1,1),float4(-1,-1,-1,1),
		float4(-1,1,1,1),float4(1,1,1,1),float4(1,-1,1,1),float4(-1,-1,1,1)};
	CloudVarying o;
	o.cameraMin=float3(1e20);o.cameraMax=float3(-1e20);
	for(int i=0;i<8;++i){
		float4 point=u.inverseViewProjection*frustumCorners[i];
		point/=point.w;
		o.cameraMin=min(o.cameraMin,point.xyz);o.cameraMax=max(o.cameraMax,point.xyz);
	}
	float2 point=corners[indices[ordinal]];
	o.position=float4(point,0,1);o.uv=point*.5+.5;return o;
}
struct CloudRay {float3 origin,direction;};
bool CloudIntersectBox(CloudRay r,float3 boxMin,float3 boxMax,thread float& t0,thread float& t1)
{
	float3 invR=1.0/r.direction;
	float3 bottom=invR*(boxMin-r.origin),top=invR*(boxMax-r.origin);
	float3 tmin=min(top,bottom),tmax=max(top,bottom);
	float2 t=max(tmin.xx,tmin.yz);
	t0=max(0.0,max(t.x,t.y));t=min(tmax.xx,tmax.yz);t1=min(t.x,t.y);
	return abs(t0)<=t1;
}
float CloudNoise(float3 p,texture3d<float> volume,sampler volumeFilter)
{
	return volume.sample(volumeFilter,p*.2).r;
}
float CloudMap(float3 p,constant CloudUniforms& u,texture3d<float> volume,sampler volumeFilter)
{
	float factor=1.0-smoothstep(u.scaleHeightBottomFade.w,u.scaleHeightBottomFade.y,p.y);
	p+=u.offset.xyz;p*=1.0/u.scaleHeightBottomFade.x;p+=u.mapSunTime.w*.07;
	float f=CloudNoise(p,volume,volumeFilter);
	// GLSL mat3 constructor arguments specify columns, as does MSL.
	const float3x3 m=float3x3(float3(0,.8,.6),float3(-.8,.36,-.48),float3(-.6,-.48,.64))*2.01;
	p=m*p+u.mapSunTime.w*.3;f+=.4*CloudNoise(1.01*p,volume,volumeFilter);
	p=m*p-u.mapSunTime.w*.6;f+=.2*CloudNoise(1.03*p,volume,volumeFilter);
	return mix(0.0,f,factor);
}
float4 CloudRaymarch(float3 start,float3 finish,constant CloudUniforms& u,texture3d<float> volume,sampler volumeFilter)
{
	float distance=length(finish-start);
	const float tstep=1.0/10.0;
	float depth=min(distance/(u.scaleHeightBottomFade.y-u.scaleHeightBottomFade.z),1.5);
	float fogContrib=0,sunContrib=0,alpha=0;
	// Keep the original floating loop, including its original endpoint behavior.
	for(float t=0;t<=1;t+=tstep){
		float3 pos=mix(start,finish,t);
		float fog=CloudMap(pos,u,volume,volumeFilter);fogContrib+=fog;
		float3 lightPos=u.sunDirection.xyz*u.mapSunTime.z+pos;
		float lightFog=CloudMap(lightPos,u,volume,volumeFilter);
		sunContrib+=clamp(fog-lightFog,0.0,1.0)*6.0;
		float b=smoothstep(1.0,.7,abs((t-.5)*2.0));alpha+=b;
	}
	fogContrib*=tstep;sunContrib*=tstep;alpha*=tstep*u.fogColorOpacity.w*depth;
	float3 ndir=(finish-start)/distance;
	float sun=pow(clamp(dot(u.sunDirection.xyz,ndir),0.0,1.0),100.0);
	sunContrib+=sun*clamp(1.0-fogContrib*alpha,.2,1.0)*1.0;
	return float4(sunContrib*u.sunColor.xyz+u.fogColorOpacity.xyz,fogContrib*alpha);
}
float3 CloudWorldPosition(float depth,float2 uv,constant CloudUniforms& u)
{
	float4 point=u.inverseViewProjection*float4(uv*2.0-1.0,depth,1);
	point.xyz/=point.w;
	if(depth==1){
		float3 forward=normalize(point.xyz-u.eye.xyz);
		float a=max(u.scaleHeightBottomFade.y-u.eye.y,u.eye.y-u.scaleHeightBottomFade.z)/forward.y;
		return u.eye.xyz+forward*abs(a);
	}
	return point.xyz;
}
fragment float4 CloudFragment(CloudVarying input [[stage_in]],constant CloudUniforms& u [[buffer(0)]],
	depth2d<float> depthImage [[texture(0)]],texture3d<float> volume [[texture(1)]],
	sampler depthFilter [[sampler(0)]],sampler volumeFilter [[sampler(1)]])
{
	// Apply original nearest/repeat selection in GL bottom-left texel space
	// BEFORE reflecting the integer row into the native top-down allocation.
	// Normalized (1-v) sampling chooses the adjacent row at exact boundaries;
	// quarter-resolution fog lands on precisely these source boundaries.
	int2 extent=int2(depthImage.get_width(),depthImage.get_height());
	int2 logical=int2(floor(input.uv*float2(extent)));
	logical=((logical%extent)+extent)%extent;
	float depth=depthImage.read(uint2(logical.x,extent.y-1-logical.y));
	float3 worldPos=CloudWorldPosition(depth,input.uv,u);
	float3 sourceMin=u.mode.x?float3(1,u.scaleHeightBottomFade.z,1):float3(-300000,u.scaleHeightBottomFade.z,-300000);
	float3 sourceMax=u.mode.x?float3(u.mapSunTime.x-1,u.scaleHeightBottomFade.y,u.mapSunTime.y-1):float3(300000,u.scaleHeightBottomFade.y,300000);
	float3 boxMin=max(input.cameraMin,sourceMin),boxMax=min(input.cameraMax,sourceMax);
	CloudRay ray={u.eye.xyz,worldPos-u.eye.xyz};
	float first,last;
	float4 color=0;
	if(CloudIntersectBox(ray,boxMin,boxMax,first,last)){
		first=clamp(first,0.0,1.0);last=clamp(last,0.0,1.0);
		float3 start=ray.direction*first+ray.origin,finish=ray.direction*last+ray.origin;
		float4 marched=CloudRaymarch(start,finish,u,volume,volumeFilter);
		// Original Blend(vec4(0),marched); then the later World draw applies
		// original SRC_ALPHA/ONE_MINUS_SRC_ALPHA again. Preserve both stages.
		color.rgb=color.rgb*color.a+marched.rgb*marched.a;color.a+=marched.a;
	}
	return color;
}
)CLOUDMSL";
} // namespace rendering::metal4
