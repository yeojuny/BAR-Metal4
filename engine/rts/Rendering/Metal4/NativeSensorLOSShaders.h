/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include "NativeMultisampleLineShaders.h"
namespace rendering::metal4 {
// Original31657 LOS R8 stencil-fan and thinned-outline shader translation.
// Original shaders: MIT, Copyright (c) 2026 Beherith (mysterme@gmail.com).
// The original widget retains its center/rim LINE_LOOP including radial edges.
inline constexpr const char* NativeSensorLOSShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct SensorLOSUniform {
	float4x4 cameraViewProjection,cameraViewInverse,cameraViewProjectionInverse;
	float4 timeInfo,viewGeometry,mapSize,rangeColor,viewport,fragmentToLogical,values,parameters;
	uint4 options,counts;
	float4 distanceFade;
};
struct SensorLOSInstance { float4 radiusParams; uint4 instData; };
struct SensorLOSValue { float4 clip,data;float halfWidth; };
inline float SensorLOSVisibility(float3 center,float radius,constant SensorLOSUniform& u) {
	const float4 c=u.cameraViewProjection*float4(center,1.0f);
	if(c.w<=0.0f)return -1.0f;
	const float2 ndc=c.xy/c.w;
	const float4 x=c+radius*u.cameraViewProjection[0];
	const float r=length(x.xy/x.w-ndc);
	return max(max(-(ndc.x+1.0f)-r,(ndc.x-1.0f)-r),max(-(ndc.y+1.0f)-r,(ndc.y-1.0f)-r));
}
inline SensorLOSValue SensorLOSOriginalVertex(float4 point,SensorLOSInstance instance,float4 drawPos,
	constant SensorLOSUniform& u,texture2d<float> height,sampler heightSampler) {
	float4 center=float4(drawPos.xyz,1.0f);
	const float2 front=float2(sin(drawPos.w),cos(drawPos.w));
	center.xz+=front*instance.radiusParams.w+float2(-front.y,front.x)*instance.radiusParams.z;
	const float now=u.timeInfo.x+u.timeInfo.w;
	const float grow=instance.radiusParams.y<-1.0f?15.0f-(now+instance.radiusParams.y):now-instance.radiusParams.y;
	const float radius=instance.radiusParams.x*clamp(grow/15.0f,0.0f,1.0f);
	if(SensorLOSVisibility(center.xyz,radius,u)>0.0f)return {float4(2,0,0,1),float4(0),0.f};
	center.y=max(0.0f,center.y);
	float4 world=center;world.xz+=point.xy*radius;
	// Original heightmapUVatWorldPos, including its corner-to-texel correction.
	float2 w=world.xz;w+=float2(-8.0f)*(w/u.mapSize.xy)+float2(4.0f);
	float2 uv=clamp(w,float2(8.0f),u.mapSize.xy-float2(8.0f))/u.mapSize.xy;
	if(u.options.y)uv.y=1.0f-uv.y;
	const float ground=height.sample(heightSampler,uv,level(0.0f)).r;
	world.y=max(0.0f,ground);

 if(u.options.z){
  float centerDistance=0.f;
  if(dot(point.xy,point.xy)<.0001f)centerDistance=radius+32.f;
  else world.xyz+=normalize(world.xyz-center.xyz)*16.f;
  return {u.cameraViewProjection*world,float4(centerDistance,0,0,0),0.f};
 }
 float4 clip=u.cameraViewProjection*world;
 const float3 toCamera=normalize(u.cameraViewInverse[3].xyz-world.xyz);
 const float4 depth=u.cameraViewProjection*float4(world.xyz+toCamera*16.0f,1.0f);clip.z=depth.z/depth.w*clip.w;
 const float camDist=distance(u.cameraViewInverse[3].xyz,world.xyz);
 const float thin=mix(1.f,.4f,smoothstep(3000.f,8000.f,camDist));
 const float halfWidth=.5f*max(u.values.w*thin,min(u.values.w,.7f));
 const float2 edge=min(world.xz,u.mapSize.xy-world.xz);
 float inside=1.f-clamp(min(edge.x,edge.y)*-.02f,0.f,1.f);
 inside*=1.f-u.distanceFade.z*smoothstep(u.distanceFade.x,max(u.distanceFade.y,u.distanceFade.x+1.f),camDist);
 return {clip,float4(0,radius*point.z*5.2345f,ground,inside),halfWidth};
}
inline bool SensorLOSClipLine(thread float4& a,thread float4& b,thread float& enter,thread float& leave) {
	enter=0.0f;leave=1.0f;const float4 start=a,delta=b-a;
	const float av[6]={a.x+a.w,a.w-a.x,a.y+a.w,a.w-a.y,a.z,a.w-a.z};
	const float bv[6]={b.x+b.w,b.w-b.x,b.y+b.w,b.w-b.y,b.z,b.w-b.z};
	for(uint i=0;i<6;++i){if(av[i]<0.0f&&bv[i]<0.0f)return false;if(av[i]<0.0f)enter=max(enter,av[i]/(av[i]-bv[i]));if(bv[i]<0.0f)leave=min(leave,av[i]/(av[i]-bv[i]));}
	if(enter>leave)return false;a=start+enter*delta;b=start+leave*delta;return a.w>0.0f&&b.w>0.0f;
}
struct SensorLOSVarying {
	float4 position [[position]];
	float4 dataA [[flat]],dataB [[flat]],lineA [[flat]],lineB [[flat]],lineNDC [[flat]],color [[flat]];float2 halfWidths [[flat]];
};
vertex SensorLOSVarying OriginalSensorLOSVertex(uint vertexIndex [[vertex_id]],uint instanceIndex [[instance_id]],
	const device float4* vertices [[buffer(0)]],const device SensorLOSInstance* instances [[buffer(1)]],
	const device float4* positions [[buffer(2)]],constant SensorLOSUniform& u [[buffer(3)]],
	const device float4* colors [[buffer(4)]],texture2d<float> height [[texture(0)]],sampler heightSampler [[sampler(0)]]) {
	const uint segment=vertexIndex/6,corner=vertexIndex%6;
	const auto instance=instances[u.counts.z+instanceIndex];const auto pos=positions[instanceIndex];
	const auto va=SensorLOSOriginalVertex(vertices[u.counts.y+segment],instance,pos,u,height,heightSampler);
	const auto vb=SensorLOSOriginalVertex(vertices[u.counts.y+(segment+1)%u.counts.x],instance,pos,u,height,heightSampler);
	float4 a=va.clip,b=vb.clip;
	if(u.options.x){a.z=(a.z+a.w)*0.5f;b.z=(b.z+b.w)*0.5f;}
	SensorLOSVarying out{};out.color=float4(mix(u.rangeColor.rgb,colors[instanceIndex].rgb,u.values.z),u.rangeColor.a);out.position=float4(2,2,2,1);out.dataA=out.dataB=out.lineA=out.lineB=out.lineNDC=float4(0);
	float enter,leave;if(!all(isfinite(a))||!all(isfinite(b))||!SensorLOSClipLine(a,b,enter,leave))return out;
	out.dataA=mix(va.data,vb.data,enter);out.dataB=mix(va.data,vb.data,leave);out.halfWidths=float2(mix(va.halfWidth,vb.halfWidth,enter),mix(va.halfWidth,vb.halfWidth,leave));
	const float3 av=a.xyz/a.w,bv=b.xyz/b.w;out.lineNDC=float4(av.xy,bv.xy);
	out.lineA=float4(u.viewport.x+(av.x+1.0f)*0.5f*u.viewport.z,-u.viewport.y-(1.0f-av.y)*0.5f*u.viewport.w,av.z,1.0f/a.w);
	out.lineB=float4(u.viewport.x+(bv.x+1.0f)*0.5f*u.viewport.z,-u.viewport.y-(1.0f-bv.y)*0.5f*u.viewport.w,bv.z,1.0f/b.w);
	const float extent=max(1.0f,floor(u.parameters.x+0.5f))*0.5f+2.0f;
	const float2 low=min(out.lineA.xy,out.lineB.xy)-extent,high=max(out.lineA.xy,out.lineB.xy)+extent;
	constexpr uint indices[6]={0,1,2,0,2,3};constexpr float2 corners[4]={float2(0,0),float2(1,0),float2(1,1),float2(0,1)};
	const float2 pixel=mix(low,high,corners[indices[corner]]);
	out.position=float4((pixel.x-u.viewport.x)/u.viewport.z*2.0f-1.0f,1.0f+2.0f*(pixel.y+u.viewport.y)/u.viewport.w,0.5f,1.0f);return out;
}
// Existing original GL wide-line/diamond-exit raster functions follow here.
struct SensorFraction {long n,d,e1,e2;};
inline int FractionCompare(SensorFraction a,SensorFraction b) {
 const long x=a.n*b.d-b.n*a.d;if(x)return x<0?-1:1;
 const long y=a.e1*b.d-b.e1*a.d;if(y)return y<0?-1:1;
 const long z=a.e2*b.d-b.e2*a.d;return z==0?0:z<0?-1:1;
}
inline SensorFraction Fraction(long n,long d,long e1,long e2) {if(d<0)return {-n,-d,-e1,-e2};return {n,d,e1,e2};}
inline bool SensorDiamond(int2 a,int2 b,int2 pixel) {
 const long2 start=long2(long(a.x)+a.y,long(a.x)-a.y),delta=long2(long(b.x)+b.y,long(b.x)-b.y)-start;
 const long2 center=long2(long(pixel.x)+pixel.y+1,long(pixel.x)-pixel.y)*256;
 const SensorFraction zero={0,1,0,0},one={1,1,0,0};SensorFraction enter=zero,leave=one;
 for(uint axis=0;axis<2;++axis){const long low=center[axis]-128,high=center[axis]+128;
  if(delta[axis]==0){if(start[axis]<=low||start[axis]>high)return false;continue;}
  SensorFraction lo=Fraction(low-start[axis],delta[axis],1,axis==0?1:-1),hi=Fraction(high-start[axis],delta[axis],1,axis==0?1:-1);
  if(FractionCompare(lo,hi)>0){auto swap=lo;lo=hi;hi=swap;}
  if(FractionCompare(lo,enter)>0)enter=lo;if(FractionCompare(hi,leave)<0)leave=hi;
 }
 return FractionCompare(leave,enter)>0&&FractionCompare(leave,zero)>=0&&FractionCompare(enter,one)<=0&&FractionCompare(leave,one)<0;
}
inline long FloorDivide(long n,long d) {return n>=0?n/d:-((-n+d-1)/d);}
inline bool SensorLineCovered(float3 originalA,float3 originalB,float2 fragmentPosition,float width,thread float& depth) {
 const int2 a=int2(floor(originalA.xy*256.0f+0.5f)),b=int2(floor(originalB.xy*256.0f+0.5f));
 const int2 delta=b-a;if(all(delta==0))return false;const bool xMajor=abs(delta.x)>=abs(delta.y);
 const int2 pixel=int2(floor(fragmentPosition));const int major=xMajor?pixel.x:pixel.y;
 const long ma=xMajor?a.x:a.y,mi=xMajor?a.y:a.x,dm=xMajor?delta.x:delta.y,di=xMajor?delta.y:delta.x;
 long numerator=mi*dm+(long(major)*256+128-ma)*di,denominator=dm*256;
 if(denominator<0){denominator=-denominator;numerator=-numerator;}
 const int center=int(FloorDivide(numerator,denominator));bool covered=false;
 for(int minor=center-1;minor<=center+1;++minor)if(SensorDiamond(a,b,xMajor?int2(major,minor):int2(minor,major))){covered=true;break;}
 if(!covered)return false;
 const int count=max(1,int(floor(width+0.5f)));
 // minor+(1-count)/2-epsilon, exactly as NativeScreenLine's GL wide-line
 // replication. Unit-width major coverage is retained for every width.
 const int lower=int(FloorDivide(2*numerator-long(count-1)*denominator-1,2*denominator));
 const int actual=xMajor?pixel.y:pixel.x;if(actual<lower||actual>=lower+count)return false;
 const float2 d=originalB.xy-originalA.xy;
 const float t=xMajor?(fragmentPosition.x-originalA.x)/d.x:(fragmentPosition.y-originalA.y)/d.y;
 depth=originalA.z+(originalB.z-originalA.z)*t;return depth>=0.0f&&depth<=1.0f;
}
inline bool SensorAlpha(float a,float reference,uint packed) {
 if(!(packed&256))return true;
 switch(packed&255){case 0:return false;case 1:return a<reference;case 2:return a==reference;case 3:return a<=reference;case 4:return a>reference;case 5:return a!=reference;case 6:return a>=reference;default:return true;}
}
struct SensorLOSFragment {float4 color [[color(0)]];float depth [[depth(any)]];};
fragment SensorLOSFragment OriginalSensorLOSFragment(SensorLOSVarying in [[stage_in]],
	constant SensorLOSUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]],sampler maskSampler [[sampler(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	const float2 p=in.position.xy;float coverageDepth;
	const bool viewportCovered=p.x>=u.viewport.x&&p.x<u.viewport.x+u.viewport.z&&p.y>=u.viewport.y&&p.y<u.viewport.y+u.viewport.w;
	const float2 glPixel=float2(p.x,-p.y);
	const bool lineCovered=SensorLineCovered(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,coverageDepth);
	const float2 delta=in.lineB.xy-in.lineA.xy;const float t=dot(glPixel-in.lineA.xy,delta)/dot(delta,delta);
	const float reciprocalW=mix(in.lineA.w,in.lineB.w,t);
	const float4 data=mix(in.dataA*in.lineA.w,in.dataB*in.lineB.w,t)/reciprocalW;
	const float2 sourceFragment=float2(p.x*u.fragmentToLogical.x,u.fragmentToLogical.y-p.y*u.fragmentToLogical.z);

 float2 maskUV=sourceFragment/u.values.xy;if(u.parameters.z>.5f)maskUV.y=1.f-maskUV.y;
 const float maskValue=mask.sample(maskSampler,maskUV).r;
 const float outside=1.f-smoothstep(.49f,.51f,maskValue);
 float4 color=float4(in.color.rgb,in.color.a*outside*data.w);
 const float halfWidth=mix(in.halfWidths.x*in.lineA.w,in.halfWidths.y*in.lineB.w,t)/reciprocalW;
 const float2 centre=(mix(in.lineNDC.xy,in.lineNDC.zw,t)*.5f+.5f)*u.values.xy;
 const float thinCoverage=clamp(halfWidth+.5f-distance(sourceFragment,centre),0.f,1.f);
 color.a*=thinCoverage;
	// Keep texture implicit derivatives before coverage/alpha discard; do not mark hidden lines in stencil.
	if(!viewportCovered||!lineCovered||color.a<=0.0f||!SensorAlpha(color.a,u.parameters.y,u.options.w))discard_fragment();
	return {color,mix(in.lineA.z,in.lineB.z,t)};
}

struct SensorLOSFanVarying {float4 position [[position]];float distance [[RECOIL_SAMPLE_PERSPECTIVE]];};
vertex SensorLOSFanVarying OriginalSensorLOSFanVertex(uint vid [[vertex_id]],uint iid [[instance_id]],
 const device float4* vertices [[buffer(0)]],const device SensorLOSInstance* instances [[buffer(1)]],const device float4* positions [[buffer(2)]],constant SensorLOSUniform& u [[buffer(3)]],texture2d<float> height [[texture(0)]],sampler heightSampler [[sampler(0)]]){
 const uint corner=vid%3,segment=vid/3;const uint index=corner==0?0:segment+corner;
 const auto value=SensorLOSOriginalVertex(vertices[u.counts.y+index],instances[u.counts.z+iid],positions[iid],u,height,heightSampler);
 float4 clip=value.clip;if(u.options.x)clip.z=(clip.z+clip.w)*.5f;return {clip,value.data.x};
}
fragment float4 OriginalSensorLOSFanFragment(SensorLOSFanVarying in [[stage_in]],constant SensorLOSUniform& u [[buffer(3)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 if(!SensorAlpha(1.f,u.parameters.y,u.options.w))discard_fragment();return float4(float3(in.distance/32.f),1.f);
}
)MSL"
RECOIL_NATIVE_MULTISAMPLE_LINE_MSL
RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
// Actual per-sample rectangle coverage; original one-sample entry points above are unchanged.
fragment SensorLOSFragment OriginalSensorLOSFragmentMSAA(SensorLOSVarying in [[stage_in]],
	constant SensorLOSUniform& u [[buffer(3)]],texture2d<float> mask [[texture(1)]],sampler maskSampler [[sampler(1)]],
	uint nativeSampleID [[sample_id]]) {
	const float2 nativeSamplePosition=get_sample_position(nativeSampleID);
	RECOIL_SAMPLE_FIX_POSITION(in.position,nativeSamplePosition);
	const float2 p=OriginalMultisampleFragmentPosition(in.position,nativeSamplePosition);float coverageDepth;
	const bool viewportCovered=p.x>=u.viewport.x&&p.x<u.viewport.x+u.viewport.z&&p.y>=u.viewport.y&&p.y<u.viewport.y+u.viewport.w;
	const float2 glPixel=float2(p.x,-p.y);
	const bool lineCovered=OriginalMultisampleLineCovered(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,coverageDepth);
	const float2 delta=in.lineB.xy-in.lineA.xy;const float t=dot(glPixel-in.lineA.xy,delta)/dot(delta,delta);
	const float reciprocalW=mix(in.lineA.w,in.lineB.w,t);
	const float4 data=mix(in.dataA*in.lineA.w,in.dataB*in.lineB.w,t)/reciprocalW;
	const float2 sourceFragment=float2(p.x*u.fragmentToLogical.x,u.fragmentToLogical.y-p.y*u.fragmentToLogical.z);

 float2 maskUV=sourceFragment/u.values.xy;if(u.parameters.z>.5f)maskUV.y=1.f-maskUV.y;
 const float maskValue=mask.sample(maskSampler,maskUV).r;
 const float outside=1.f-smoothstep(.49f,.51f,maskValue);
 float4 color=float4(in.color.rgb,in.color.a*outside*data.w);
 const float halfWidth=mix(in.halfWidths.x*in.lineA.w,in.halfWidths.y*in.lineB.w,t)/reciprocalW;
 const float2 centre=(mix(in.lineNDC.xy,in.lineNDC.zw,t)*.5f+.5f)*u.values.xy;
 const float thinCoverage=clamp(halfWidth+.5f-distance(sourceFragment,centre),0.f,1.f);
 color.a*=thinCoverage;
	// Keep texture implicit derivatives before coverage/alpha discard; do not mark hidden lines in stencil.
	if(!viewportCovered||!lineCovered||color.a<=0.0f||!SensorAlpha(color.a,u.parameters.y,u.options.w))discard_fragment();
	return {color,mix(in.lineA.z,in.lineB.z,t)};
}
)MSL";
}
