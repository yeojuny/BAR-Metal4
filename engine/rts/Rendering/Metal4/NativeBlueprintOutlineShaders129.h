/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4 {
// Family-local application of the accepted128 strip/diamond recipe. The
// Blueprint source has constant per-instance color and ALWAYS/no-write depth.
inline constexpr char BlueprintOutlineShader129[]=R"MSL(
struct BlueprintSourceUniform129 {
 float4x4 matrix;
 float4 map,viewport,plane,raster;
 uint4 flags; // zeroToOne, inherited plane, samples, unused
};
struct BlueprintSourceSegment129 {float4 a,b,color;bool valid;};
inline float4 BlueprintSourceEndpoint129(constant BlueprintSourceUniform129& u,float y,BlueprintOutlineInstance instance,uint corner){
 constexpr float2 corners[5]={float2(-.5f,-.5f),float2(-.5f,.5f),float2(.5f,.5f),float2(.5f,-.5f),float2(-.5f,-.5f)};
 float3 position=float3(instance.position);float2 dimensions=float2(instance.dimensions);
 float3 world=float3(position.x+corners[corner].x*dimensions.x,y,position.z+corners[corner].y*dimensions.y);
 float4 clip=u.matrix*float4(world,1);if(u.flags.x)clip.z=2.f*clip.z-clip.w;return clip;
}
inline bool BlueprintSourceClip129(float da,float db,thread float& lo,thread float& hi){
 if(da<0&&db<0)return false;
 if(da<0||db<0){float t=da/(da-db);if(da<0)lo=max(lo,t);else hi=min(hi,t);}return lo<=hi;
}
inline BlueprintSourceSegment129 BlueprintSourceLine129(constant BlueprintSourceUniform129& u,texture2d<float> heights,BlueprintOutlineInstance instance,uint segment){
 BlueprintSourceSegment129 s={};
 constexpr sampler heightSampler(coord::normalized,address::clamp_to_edge,mag_filter::linear,min_filter::nearest,mip_filter::none);
 float y=heights.sample(heightSampler,BlueprintHeightUV(float3(instance.position).xz,u.map.xy),level(0)).x;
 float4 a=BlueprintSourceEndpoint129(u,y,instance,segment),b=BlueprintSourceEndpoint129(u,y,instance,segment+1);float lo=0,hi=1;
 if(u.flags.y&&!BlueprintSourceClip129(dot(a,u.plane),dot(b,u.plane),lo,hi))return s;
 for(uint axis=0;axis<3;++axis)if(!BlueprintSourceClip129(a.w+a[axis],b.w+b[axis],lo,hi)||!BlueprintSourceClip129(a.w-a[axis],b.w-b[axis],lo,hi))return s;
 s.a=mix(a,b,lo);s.b=mix(a,b,hi);s.color=float4(instance.color);
 s.valid=s.a.w>0&&s.b.w>0&&all(isfinite(s.a))&&all(isfinite(s.b));return s;
}
inline bool BlueprintSourceLess(float3 a,float3 b){return a.x<b.x||(a.x==b.x&&(a.y<b.y||(a.y==b.y&&a.z<b.z)));}
inline bool BlueprintSourceDiamond(float2 a,float2 b,float2 center){
 a-=center;b-=center;
 float2 ra=float2(a.x+a.y,a.x-a.y),d=float2((b.x+b.y)-ra.x,(b.x-b.y)-ra.y);
 float3 enter=float3(0),leave=float3(INFINITY,0,0);
 for(uint axis=0;axis<2;++axis){
  if(d[axis]==0){if(!(ra[axis]>-.5f&&ra[axis]<=.5f))return false;continue;}
  float inv=1.f/d[axis];float3 low=float3((-.5f-ra[axis])*inv,inv,axis?-inv:inv),high=float3((.5f-ra[axis])*inv,inv,axis?-inv:inv);
  if(d[axis]<0){float3 tmp=low;low=high;high=tmp;}
  if(BlueprintSourceLess(enter,low))enter=low;if(BlueprintSourceLess(high,leave))leave=high;
 }
 return BlueprintSourceLess(enter,leave)&&BlueprintSourceLess(leave,float3(1,0,0));
}
inline bool BlueprintSourceCoverage(float2 a,float2 b,float2 center,float width){
 // API window coordinates point upwards. Replication is along the minor axis.
 bool xMajor=abs(b.x-a.x)>=abs(b.y-a.y);float2 minor=xMajor?float2(0,1):float2(1,0);
 uint w=max(1u,uint(floor(width+.5f)));float2 shift=minor*(float(w)-1.f)*.5f;
 for(uint i=0;i<w;++i)if(BlueprintSourceDiamond(a-shift,b-shift,center-minor*float(i)))return true;
 return false;
}

struct BlueprintSourceOut129 {float4 position [[position]];float4 endpoints [[flat]];float4 color [[flat]];};
inline BlueprintSourceOut129 BlueprintSourceCorner129(BlueprintSourceSegment129 line,constant BlueprintSourceUniform129& u,uint corner){
 BlueprintSourceOut129 out={};out.position=float4(2,2,2,1);if(!line.valid)return out;
 float2 an=line.a.xy/line.a.w,bn=line.b.xy/line.b.w;
 float2 a=u.viewport.xy+(float2(an.x,-an.y)+1.f)*.5f*u.viewport.zw,b=u.viewport.xy+(float2(bn.x,-bn.y)+1.f)*.5f*u.viewport.zw;
 float2 delta=b-a;float len=length(delta);if(!(len>0))return out;float2 axis=delta/len,normal=float2(-axis.y,axis.x);
 bool end=(corner&2)!=0,positive=(corner&1)!=0;float radius=u.flags.z>1?u.raster.z*.5f:floor(u.raster.z+.5f)*.5f+1.5f,extension=u.flags.z>1?0.f:1.f;
 float2 p=(end?b:a)+axis*(end?extension:-extension)+normal*(positive?radius:-radius);
 out.position=float4(2.f*p.x/u.raster.x-1.f,1.f-2.f*p.y/u.raster.y,.5f,1);
 out.endpoints=float4(a.x,u.raster.y-a.y,b.x,u.raster.y-b.y);out.color=line.color;return out;
}
vertex BlueprintSourceOut129 BlueprintSourceVertex129(uint corner [[vertex_id]],uint id [[instance_id]],device const BlueprintOutlineInstance* instances [[buffer(0)]],constant BlueprintSourceUniform129& u [[buffer(2)]],texture2d<float> heights [[texture(0)]]){
 return BlueprintSourceCorner129(BlueprintSourceLine129(u,heights,instances[id/4],id%4),u,corner);
}
vertex BlueprintOutlineOut BlueprintSourceVertexMSAA129(uint corner [[vertex_id]],uint id [[instance_id]],device const BlueprintOutlineInstance* instances [[buffer(0)]],constant BlueprintSourceUniform129& u [[buffer(2)]],texture2d<float> heights [[texture(0)]]){
 auto source=BlueprintSourceCorner129(BlueprintSourceLine129(u,heights,instances[id/4],id%4),u,corner);BlueprintOutlineOut out;out.position=source.position;out.color=source.color;return out;
}
fragment float4 BlueprintSourceFragment129(BlueprintSourceOut129 in [[stage_in]],constant BlueprintRasterUniform& alpha [[buffer(1)]],constant BlueprintSourceUniform129& u [[buffer(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 float2 p=float2(in.position.x,u.raster.y-in.position.y);if(!BlueprintSourceCoverage(in.endpoints.xy,in.endpoints.zw,p,u.raster.z))discard_fragment();
 if(alpha.options.x&&!BlueprintAlpha(in.color.a,alpha.alphaState.x,alpha.options.y))discard_fragment();return in.color;
}
)MSL";
}
