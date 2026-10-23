/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"
#include "NativeMultisampleLineShaders.h"
namespace rendering::metal4 {
// Typed translation of the complete selected Radar/Sonar EXACT_CIRCLES paths
// from game31657 sensor_ranges_los.{vert,frag}.glsl. Original shaders are MIT,
// Copyright (c) 2026 Beherith (mysterme@gmail.com). Source attestation and the
// original LuaShader expansion remain with the private CPU program owner.
inline constexpr const char* NativeSensor31657CircleShaderSource=RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Sensor31657Uniform {
	float4x4 cameraViewProjection,cameraViewInverse,cameraViewProjectionInverse;
	float4 timeInfo,viewGeometry,mapSize,rangeColor,viewport,fragmentToLogical,values,parameters;
	uint4 options,counts;
};
struct Sensor31657Instance { float4 radiusParams; uint4 instData; };
struct Sensor31657Value { float4 clip,data; };
inline float Sensor31657Visibility(float3 center,float radius,constant Sensor31657Uniform& u) {
	const float4 c=u.cameraViewProjection*float4(center,1.0f);
	if(c.w<=0.0f)return -1.0f;
	const float2 ndc=c.xy/c.w;
	const float4 x=c+radius*u.cameraViewProjection[0];
	const float r=length(x.xy/x.w-ndc);
	return max(max(-(ndc.x+1.0f)-r,(ndc.x-1.0f)-r),max(-(ndc.y+1.0f)-r,(ndc.y-1.0f)-r));
}
inline Sensor31657Value Sensor31657OriginalVertex(float4 point,Sensor31657Instance instance,float4 drawPos,
	constant Sensor31657Uniform& u,texture2d<float> height,sampler heightSampler) {
	float4 center=float4(drawPos.xyz,1.0f);
	const float2 front=float2(sin(drawPos.w),cos(drawPos.w));
	center.xz+=front*instance.radiusParams.w+float2(-front.y,front.x)*instance.radiusParams.z;
	const float now=u.timeInfo.x+u.timeInfo.w;
	const float grow=instance.radiusParams.y<-1.0f?15.0f-(now+instance.radiusParams.y):now-instance.radiusParams.y;
	const float radius=instance.radiusParams.x*clamp(grow/15.0f,0.0f,1.0f);
	if(u.options.z&&Sensor31657Visibility(center.xyz,radius,u)>0.0f)return {float4(2,0,0,1),float4(0)};
	center.y=max(0.0f,center.y);
	float4 world=center;world.xz+=point.xy*radius;
	// Original heightmapUVatWorldPos, including its corner-to-texel correction.
	float2 w=world.xz;w+=float2(-8.0f)*(w/u.mapSize.xy)+float2(4.0f);
	float2 uv=clamp(w,float2(8.0f),u.mapSize.xy-float2(8.0f))/u.mapSize.xy;
	if(u.options.y)uv.y=1.0f-uv.y;
	const float ground=height.sample(heightSampler,uv,level(0.0f)).r;
	world.y=max(0.0f,ground);
	float4 clip=u.cameraViewProjection*world;
	const float3 toCamera=normalize(u.cameraViewInverse[3].xyz-world.xyz);
	const float4 depth=u.cameraViewProjection*float4(world.xyz+toCamera*16.0f,1.0f);
	clip.z=depth.z/depth.w*clip.w;
	const float2 edge=min(world.xz,u.mapSize.xy-world.xz);
	const float inside=1.0f-clamp(min(edge.x,edge.y)*-0.02f,0.0f,1.0f);
	return {clip,float4(0,radius*point.z*5.2345f,ground,inside)};
}
inline bool Sensor31657ClipLine(thread float4& a,thread float4& b,thread float& enter,thread float& leave) {
	enter=0.0f;leave=1.0f;const float4 start=a,delta=b-a;
	const float av[6]={a.x+a.w,a.w-a.x,a.y+a.w,a.w-a.y,a.z,a.w-a.z};
	const float bv[6]={b.x+b.w,b.w-b.x,b.y+b.w,b.w-b.y,b.z,b.w-b.z};
	for(uint i=0;i<6;++i){if(av[i]<0.0f&&bv[i]<0.0f)return false;if(av[i]<0.0f)enter=max(enter,av[i]/(av[i]-bv[i]));if(bv[i]<0.0f)leave=min(leave,av[i]/(av[i]-bv[i]));}
	if(enter>leave)return false;a=start+enter*delta;b=start+leave*delta;return a.w>0.0f&&b.w>0.0f;
}
struct Sensor31657Varying {
	float4 position [[position]];
	float4 dataA [[flat]],dataB [[flat]],lineA [[flat]],lineB [[flat]],lineNDC [[flat]];
};
vertex Sensor31657Varying OriginalSensor31657CircleVertex(uint vertexIndex [[vertex_id]],uint instanceIndex [[instance_id]],
	const device float4* vertices [[buffer(0)]],const device Sensor31657Instance* instances [[buffer(1)]],
	const device float4* positions [[buffer(2)]],constant Sensor31657Uniform& u [[buffer(3)]],
	texture2d<float> height [[texture(0)]],sampler heightSampler [[sampler(0)]]) {
	const uint segment=vertexIndex/6,corner=vertexIndex%6;
	const auto instance=instances[u.counts.z+instanceIndex];const auto pos=positions[instanceIndex];
	const auto va=Sensor31657OriginalVertex(vertices[u.counts.y+segment],instance,pos,u,height,heightSampler);
	const auto vb=Sensor31657OriginalVertex(vertices[u.counts.y+(segment+1)%u.counts.x],instance,pos,u,height,heightSampler);
	float4 a=va.clip,b=vb.clip;
	if(u.options.x){a.z=(a.z+a.w)*0.5f;b.z=(b.z+b.w)*0.5f;}
	Sensor31657Varying out;out.position=float4(2,2,2,1);out.dataA=out.dataB=out.lineA=out.lineB=out.lineNDC=float4(0);
	float enter,leave;if(!all(isfinite(a))||!all(isfinite(b))||!Sensor31657ClipLine(a,b,enter,leave))return out;
	out.dataA=mix(va.data,vb.data,enter);out.dataB=mix(va.data,vb.data,leave);
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
inline float Sensor31657Outside(float2 sourceFragment,float ground,constant Sensor31657Uniform& u,
	const device Sensor31657Instance* circles,const device float4* positions) {
	const float2 ndc=(sourceFragment-u.viewGeometry.zw)/u.viewGeometry.xy*2.0f-1.0f;
	const float4 rayEnd=u.cameraViewProjectionInverse*float4(ndc,1,1);
	const float3 cam=u.cameraViewInverse[3].xyz,ray=rayEnd.xyz/rayEnd.w-cam;
	const float2 pos=(cam+ray*((max(0.0f,ground)-cam.y)/ray.y)).xz;
	const float pixelSize=max(length(dfdx(pos)),length(dfdy(pos)));
	const float now=u.timeInfo.x+u.timeInfo.w,segment=6.2831853f/u.parameters.z;float nearest=1.0f;
	for(uint i=0;i<u.counts.w;++i){
		const auto radius=circles[i].radiusParams;const auto drawPos=positions[i];
		const float reach=radius.x+length(radius.zw)+pixelSize+8.0f;const float2 toUnit=pos-drawPos.xz;
		if(dot(toUnit,toUnit)>reach*reach)continue;
		const float2 front=float2(sin(drawPos.w),cos(drawPos.w));
		const float2 center=drawPos.xz+front*radius.w+float2(-front.y,front.x)*radius.z;
		const float grow=radius.y<-1.0f?15.0f-(now+radius.y):now-radius.y;
		const float r=radius.x*clamp(grow/15.0f,0.0f,1.0f);const float2 toPos=pos-center;
		const float angle=atan2(toPos.x,toPos.y);const float local=angle-segment*floor(angle/segment)-0.5f*segment;
		const float edgeRadius=(r+256.0f/(r+32.0f))*cos(0.5f*segment)/cos(local);
		const float edgeWidth=max(pixelSize,0.64f*(r+16.0f)/(r+32.0f));
		nearest=min(nearest,(length(toPos)-edgeRadius)/edgeWidth);
	}
	return smoothstep(-0.5f,0.5f,nearest);
}
struct Sensor31657Fragment {float4 color [[color(0)]];float depth [[depth(any)]];};
fragment Sensor31657Fragment OriginalSensor31657CircleFragment(Sensor31657Varying in [[stage_in]],
	constant Sensor31657Uniform& u [[buffer(3)]],const device Sensor31657Instance* circles [[buffer(4)]],const device float4* positions [[buffer(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
	const float2 p=in.position.xy;float coverageDepth;
	const bool viewportCovered=p.x>=u.viewport.x&&p.x<u.viewport.x+u.viewport.z&&p.y>=u.viewport.y&&p.y<u.viewport.y+u.viewport.w;
	const float2 glPixel=float2(p.x,-p.y);
	const bool lineCovered=SensorLineCovered(in.lineA.xyz,in.lineB.xyz,glPixel,u.parameters.x,coverageDepth);
	const float2 delta=in.lineB.xy-in.lineA.xy;const float t=dot(glPixel-in.lineA.xy,delta)/dot(delta,delta);
	const float reciprocalW=mix(in.lineA.w,in.lineB.w,t);
	const float4 data=mix(in.dataA*in.lineA.w,in.dataB*in.lineB.w,t)/reciprocalW;
	const float2 sourceFragment=float2(p.x*u.fragmentToLogical.x,u.fragmentToLogical.y-p.y*u.fragmentToLogical.z);
	const float outside=Sensor31657Outside(sourceFragment,data.z,u,circles,positions);
	float4 color=float4(u.rangeColor.rgb,u.rangeColor.a*outside);color.a*=data.w;
	if((u.options.w&512u)&&data.z>u.values.w)color.a=0.0f;
	const float phase=data.y*u.values.z+u.timeInfo.x*-0.2f;
	const float phasePerPixel=length(float2(dfdx(phase),dfdy(phase)));
	const float stipple=max(3.0f*sin(phase),0.0f);
	color.a*=mix(stipple,0.9549297f,smoothstep(1.5f,3.0f,phasePerPixel));
	const float2 centre=(mix(in.lineNDC.xy,in.lineNDC.zw,t)*0.5f+0.5f)*u.values.xy;
	const float maxOffset=max(3.14159265f/phasePerPixel,2.0f);
	color.a*=clamp(maxOffset+0.5f-distance(sourceFragment,centre),0.0f,1.0f);
	// Original derivatives run before its final alpha discard. Keep emulated
	// line-coverage helper lanes alive through those same expressions.
	if(!viewportCovered||!lineCovered||color.a<=0.0f||!SensorAlpha(color.a,u.parameters.y,u.options.w))discard_fragment();
	return {color,mix(in.lineA.z,in.lineB.z,t)};
}
)MSL"
RECOIL_NATIVE_MULTISAMPLE_LINE_MSL
RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
// Actual per-sample rectangle coverage; original one-sample entry points above are unchanged.
fragment Sensor31657Fragment OriginalSensor31657CircleFragmentMSAA(Sensor31657Varying in [[stage_in]],
	constant Sensor31657Uniform& u [[buffer(3)]],const device Sensor31657Instance* circles [[buffer(4)]],const device float4* positions [[buffer(5)]],
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
	const float outside=Sensor31657Outside(sourceFragment,data.z,u,circles,positions);
	float4 color=float4(u.rangeColor.rgb,u.rangeColor.a*outside);color.a*=data.w;
	if((u.options.w&512u)&&data.z>u.values.w)color.a=0.0f;
	const float phase=data.y*u.values.z+u.timeInfo.x*-0.2f;
	const float phasePerPixel=length(float2(dfdx(phase),dfdy(phase)));
	const float stipple=max(3.0f*sin(phase),0.0f);
	color.a*=mix(stipple,0.9549297f,smoothstep(1.5f,3.0f,phasePerPixel));
	const float2 centre=(mix(in.lineNDC.xy,in.lineNDC.zw,t)*0.5f+0.5f)*u.values.xy;
	const float maxOffset=max(3.14159265f/phasePerPixel,2.0f);
	color.a*=clamp(maxOffset+0.5f-distance(sourceFragment,centre),0.0f,1.0f);
	// Original derivatives run before its final alpha discard. Keep emulated
	// line-coverage helper lanes alive through those same expressions.
	if(!viewportCovered||!lineCovered||color.a<=0.0f||!SensorAlpha(color.a,u.parameters.y,u.options.w))discard_fragment();
	return {color,mix(in.lineA.z,in.lineB.z,t)};
}
)MSL";
}
