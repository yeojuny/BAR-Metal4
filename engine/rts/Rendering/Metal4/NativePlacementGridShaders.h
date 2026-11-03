/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
inline constexpr const char* PlacementGridShader128=R"MSL(
#include <metal_stdlib>
using namespace metal;
// Source: pinned gui_building_grid_gl4.lua, plus OpenGL 4.6 sections 13.8/14.5.
// Four vertices per source segment. No pixel geometry, prefix, compute pass,
// software double precision, output cache, or GPU-to-CPU validation word.
struct GridSourceUniform {
 float4x4 matrix;
 float4 mouse; // xyz, original CPU camera fade
 float4 mapLines; // map size x/z, strong/weak opacity
 float4 viewport; // Metal top-left x/y, width/height
 float4 plane; // inherited GL homogeneous clip plane
 float4 raster; // target width/height, line width, original minimum height
 uint4 flags; // first source vertex, zeroToOne, inherited plane, samples
};
struct GridSourceEndpoint {float4 clip; float4 worldAlpha;};
struct GridSourceSegment {GridSourceEndpoint a,b;bool valid;};
inline GridSourceEndpoint GridSourceEndpointAt(constant GridSourceUniform& u,texture2d<float> heights,uint id) {
 GridSourceEndpoint v={};v.clip=float4(2,2,2,1);
 int segment=int(id>>1);bool alongX=segment<24*23;if(!alongX)segment-=24*23;
 int line=segment/23,along=segment-line*23;
 int2 origin=int2(floor(u.mouse.xz/48.f))-11;
 int2 start=origin+(alongX?int2(along,line):int2(line,along));
 int2 dir=alongX?int2(1,0):int2(0,1);
 int2 lastStart=alongX?int2(ceil(u.mapLines.x/48.f)-1,floor(u.mapLines.y/48.f)):
                         int2(floor(u.mapLines.x/48.f),ceil(u.mapLines.y/48.f)-1);
 float2 segStart=float2(start)*48.f,segEnd=float2(start+dir)*48.f;
 float2 toClosest=clamp(u.mouse.xz,segStart,segEnd)-u.mouse.xz;
 if(any(start<0)||any(start>lastStart)||dot(toClosest,toClosest)>=480.f*480.f)return v;
 float2 xz=(id&1)?segEnd:segStart;
 float2 inverseMapSize=1.f/u.mapLines.xy;
 float2 uv=clamp(xz+(-8.f*(xz*inverseMapSize)+4.f),float2(8),u.mapLines.xy-8.f)*inverseMapSize;
 constexpr sampler heightSampler(coord::normalized,address::clamp_to_edge,mag_filter::linear,min_filter::nearest,mip_filter::none);
 float y=max(u.raster.w,heights.sample(heightSampler,uv,level(0)).x);
 v.worldAlpha=float4(xz.x,y,xz.y,((alongX?start.y:start.x)%4==0)?u.mapLines.z:u.mapLines.w);
 v.clip=u.matrix*float4(v.worldAlpha.xyz,1);
 if(u.flags.y)v.clip.z=2.f*v.clip.z-v.clip.w;
 return v;
}
inline bool GridSourceClipPlane(float da,float db,thread float& lo,thread float& hi){
 if(da<0&&db<0)return false;
 if(da<0||db<0){float t=da/(da-db);if(da<0)lo=max(lo,t);else hi=min(hi,t);}
 return lo<=hi;
}
inline GridSourceSegment GridSourceLine(constant GridSourceUniform& u,texture2d<float> heights,uint segment){
 GridSourceSegment s={};s.a=GridSourceEndpointAt(u,heights,u.flags.x+2*segment);s.b=GridSourceEndpointAt(u,heights,u.flags.x+2*segment+1);
 float4 a=s.a.clip,b=s.b.clip;float lo=0,hi=1;
 if(u.flags.z&&!GridSourceClipPlane(dot(a,u.plane),dot(b,u.plane),lo,hi))return s;
 for(uint i=0;i<3;++i)if(!GridSourceClipPlane(a.w+a[i],b.w+b[i],lo,hi)||!GridSourceClipPlane(a.w-a[i],b.w-b[i],lo,hi))return s;
 s.a.clip=mix(a,b,lo);s.b.clip=mix(a,b,hi);
 float4 wa=s.a.worldAlpha,wb=s.b.worldAlpha;
 s.a.worldAlpha=float4(mix(wa.xyz,wb.xyz,lo),wb.w);s.b.worldAlpha=float4(mix(wa.xyz,wb.xyz,hi),wb.w);
 s.valid=s.a.clip.w>0&&s.b.clip.w>0&&all(isfinite(s.a.clip))&&all(isfinite(s.b.clip));return s;
}
inline float2 GridSourceWindow(float4 clip,constant GridSourceUniform& u){
 float2 ndc=clip.xy/clip.w;return u.viewport.xy+(float2(ndc.x,-ndc.y)+1.f)*.5f*u.viewport.zw;
}
// Symbolic epsilon ties implement the GL diamond endpoint rule without
// shifting ordinary coordinates or reproducing a CPU raster implementation.
// (x,y) -> (x-epsilon,y-epsilon^2), so the rotated axes have coefficients (1,+/-1).
inline bool GridSourceLess(float3 a,float3 b){return a.x<b.x||(a.x==b.x&&(a.y<b.y||(a.y==b.y&&a.z<b.z)));}
inline bool GridSourceDiamond(float2 a,float2 b,float2 center){
 a-=center;b-=center;
 float2 ra=float2(a.x+a.y,a.x-a.y),d=float2((b.x+b.y)-ra.x,(b.x-b.y)-ra.y);
 float3 enter=float3(0),leave=float3(INFINITY,0,0);
 for(uint axis=0;axis<2;++axis){
  if(d[axis]==0){if(!(ra[axis]>-.5f&&ra[axis]<=.5f))return false;continue;}
  float inv=1.f/d[axis];float3 low=float3((-.5f-ra[axis])*inv,inv,axis?-inv:inv),high=float3((.5f-ra[axis])*inv,inv,axis?-inv:inv);
  if(d[axis]<0){float3 tmp=low;low=high;high=tmp;}
  if(GridSourceLess(enter,low))enter=low;if(GridSourceLess(high,leave))leave=high;
 }
 return GridSourceLess(enter,leave)&&GridSourceLess(leave,float3(1,0,0));
}
inline bool GridSourceCoverage(float2 a,float2 b,float2 center,float width){
 // API window coordinates point upwards. Replication is along the minor axis.
 bool xMajor=abs(b.x-a.x)>=abs(b.y-a.y);float2 minor=xMajor?float2(0,1):float2(1,0);
 uint w=max(1u,uint(floor(width+.5f)));float2 shift=minor*(float(w)-1.f)*.5f;
 for(uint i=0;i<w;++i)if(GridSourceDiamond(a-shift,b-shift,center-minor*float(i)))return true;
 return false;
}
struct GridSourceVarying {
 float4 position [[position]];
 float4 endpoints [[flat]]; // original centerline screen a.xy,b.xy, bottom-left
 float4 worldA [[flat]],worldB [[flat]]; // xyz/w, 1/w
 float alpha [[flat]];
};
inline GridSourceVarying GridSourceCorner(GridSourceSegment s,constant GridSourceUniform& u,uint corner){
 GridSourceVarying out={};out.position=float4(2,2,2,1);if(!s.valid)return out;
 float2 a=GridSourceWindow(s.a.clip,u),b=GridSourceWindow(s.b.clip,u),delta=b-a;
 float len=length(delta);if(!(len>0))return out;
 float2 axis=delta/len,normal=float2(-axis.y,axis.x);
 bool end=(corner&2)!=0,positive=(corner&1)!=0;
 // MSAA uses the exact perpendicular rectangle. Aliased coverage is evaluated
 // on a small conservative strip, never a screen-sized bounding rectangle.
 float radius=u.flags.w>1?u.raster.z*.5f:(floor(u.raster.z+.5f)*.5f+1.5f);
 float extension=u.flags.w>1?0.f:1.f;
 float2 p=(end?b:a)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 out.position=float4(p.x/u.raster.x*2.f-1.f,1.f-p.y/u.raster.y*2.f,.5f,1.f);
 out.endpoints=float4(a.x,u.raster.y-a.y,b.x,u.raster.y-b.y);
 out.worldA=float4(s.a.worldAlpha.xyz,1)/s.a.clip.w;out.worldB=float4(s.b.worldAlpha.xyz,1)/s.b.clip.w;
 out.alpha=s.b.worldAlpha.w;return out;
}
vertex GridSourceVarying GridSourceVertex(uint corner [[vertex_id]],uint segment [[instance_id]],constant GridSourceUniform& u [[buffer(0)]],texture2d<float> heights [[texture(0)]]){
 return GridSourceCorner(GridSourceLine(u,heights,segment),u,corner);
}
// MSAA's source contract is a perpendicular rectangle, so native interpolation
// and the existing fragment are sufficient; no coverage helper runs per sample.
vertex PlacementGridVarying GridSourceVertexMSAA(uint corner [[vertex_id]],uint segment [[instance_id]],constant GridSourceUniform& u [[buffer(0)]],texture2d<float> heights [[texture(0)]]){
 auto line=GridSourceLine(u,heights,segment);PlacementGridVarying out={};out.position=float4(2,2,2,1);if(!line.valid)return out;
 auto p=GridSourceCorner(line,u,corner);auto endpoint=(corner&2)?line.b:line.a;
 out.position=p.position*endpoint.clip.w;out.world=endpoint.worldAlpha.xyz;out.color=float4(.7f,1.f,.7f,endpoint.worldAlpha.w);return out;
}
fragment float4 GridSourceFragment(GridSourceVarying in [[stage_in]],constant GridSourceUniform& u [[buffer(0)]],constant float4* alphaTest [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 p=float2(in.position.x,u.raster.y-in.position.y),a=in.endpoints.xy,b=in.endpoints.zw;
 if(u.flags.w==1&&!GridSourceCoverage(a,b,p,u.raster.z))discard_fragment();
 float2 d=b-a;float t=dot(p-a,d)/dot(d,d);
 float4 world=mix(in.worldA,in.worldB,t);float3 worldPos=world.xyz/world.w;
 // Literal original generated31657 fragment body. Camera fade remains CPU Lua.
 float dist=distance(worldPos,u.mouse.xyz);
 float alpha=smoothstep(0.f,1.f,(480.f/(dist/2.5f))-2.5f);
 float resultAlpha=(alpha-u.mouse.w)*in.alpha;
 float4 test=alphaTest[2];
 if(test.x!=0){float reference=test.z;uint comparison=uint(test.y);bool pass=false;
  switch(comparison){case 0:break;case 1:pass=resultAlpha<reference;break;case 2:pass=resultAlpha==reference;break;case 3:pass=resultAlpha<=reference;break;case 4:pass=resultAlpha>reference;break;case 5:pass=resultAlpha!=reference;break;case 6:pass=resultAlpha>=reference;break;case 7:pass=true;break;}if(!pass)discard_fragment();}
 return float4(.7f,1.f,.7f,resultAlpha);
}
)MSL";
}
