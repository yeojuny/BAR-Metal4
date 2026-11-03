/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawGeometry.h"
#include "NativeWorldDrawRecorder.h"
#include "NativeMultisampleLine.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <optional>
namespace rendering::exactWorldLine {
using U64=std::uint64_t;using U32=std::uint32_t;using I32=std::int32_t;
struct Line120Vertex {float position[4],color[4],uv[4];};
struct Line120Segment {
 Line120Vertex a,b;
 // Raster endpoints and tag after the second, screen-space clip.
 float sa[4],sb[4];
 U64 viewport[4],nx,ny,counter;
 I32 snapped[4],majorFirst,unitFirst,direction;
 U32 width,height,count,flags,pattern,factor;
};
// One span for the whole aliased segment, one for each MSAA stipple interval.
struct Line120Span {U32 first,count,segment,reserved;U64 t0,t1;};
inline NativeWorldDrawVertex Mix(const NativeWorldDrawVertex& a,const NativeWorldDrawVertex& b,double t)
{
	NativeWorldDrawVertex r;
	for(size_t i=0;i<4;++i){r.position[i]=float(a.position[i]+(double(b.position[i])-a.position[i])*t);r.color[i]=float(a.color[i]+(double(b.color[i])-a.color[i])*t);}
	for(size_t i=0;i<4;++i)r.uv[i]=float(a.uv[i]+(double(b.uv[i])-a.uv[i])*t);
	return r;
}
inline double PlaneDistance(const NativeWorldDrawVertex& vertex,const std::array<float,4>& plane)
{
	double distance=0;
	for(size_t i=0;i<4;++i)distance+=double(vertex.position[i])*plane[i];
	return distance;
}
inline bool ClipUserPlane(NativeWorldDrawVertex& a,NativeWorldDrawVertex& b,const std::array<float,4>& plane)
{
	const double da=PlaneDistance(a,plane),db=PlaneDistance(b,plane);
	if(da<0 && db<0)return false;
	if((da<0)!=(db<0)) {
		const auto intersection=Mix(a,b,da/(da-db));
		if(da<0)a=intersection;else b=intersection;
	}
	return true;
}
inline bool Clip(NativeWorldDrawVertex& a,NativeWorldDrawVertex& b,bool depthClamp)
{
	double enter=0,leave=1;
	for(size_t axis=0;axis<(depthClamp?2u:3u);++axis)for(double sign:{-1.,1.}) {
		const double da=a.position[3]+sign*a.position[axis],db=b.position[3]+sign*b.position[axis];
		if(da<0 && db<0)return false;
		if(da<0)enter=std::max(enter,da/(da-db));
		if(db<0)leave=std::min(leave,da/(da-db));
	}
	if(enter>leave)return false;
	const auto first=a,last=b;a=Mix(first,last,enter);b=Mix(first,last,leave);
	return a.position[3]>0 && b.position[3]>0;
}
inline bool Diamond(double ax,double ay,double bx,double by,int x,int y)
{
	// GL's endpoint perturbation removes exact diamond-boundary ambiguity.
	constexpr double E=1e-7;ax-=E;ay-=E*E;bx-=E;by-=E*E;
	const std::array<double,2> start{ax+ay,ax-ay},delta{bx+by-start[0],bx-by-start[1]};
	const std::array<double,2> center{double(x+y+1),double(x-y)};
	double enter=0,leave=1;
	for(size_t axis=0;axis<2;++axis) {
		const double lo=center[axis]-.5,hi=center[axis]+.5;
		if(delta[axis]==0){if(start[axis]<=lo||start[axis]>=hi)return false;continue;}
		double a=(lo-start[axis])/delta[axis],b=(hi-start[axis])/delta[axis];
		if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);
	}
	// A fragment is generated only when the segment exits its diamond; the
	// final endpoint's diamond is excluded even if the segment enters it.
	return leave>enter && leave>=0 && enter<=1 && leave<1;
}
struct Packet {std::vector<Line120Segment> segments;std::vector<Line120Span> spans;uint32_t quads=0;};
inline U64 db(double x){return std::bit_cast<U64>(x);}
struct BudgetExceeded {};
inline Packet Build(const NativeWorldDrawCommand& d,const std::array<double,4>& vp,unsigned width,unsigned height,unsigned samples,uint64_t quadLimit=UINT32_MAX){
 ValidateNativeWorldDrawCommand(d);
 if(d.primitive!=NativeDrawPrimitive::Lines||d.surface.material!=ScreenMaterial::Solid||d.surface.secondary.texture)throw std::runtime_error("diagnostic scope");
 Packet p;uint64_t counter=0;
 const auto screen=[&](const NativeWorldDrawVertex& v){return std::array<float,4>{float(vp[0]+(v.position[0]/v.position[3]+1)*vp[2]*.5),float(double(height)-vp[1]-vp[3]+(v.position[1]/v.position[3]+1)*vp[3]*.5),v.position[2]/v.position[3],0};};
 for(size_t index=0;index<d.indices.size();index+=2){
  auto a=d.vertices[d.indices[index]],b=d.vertices[d.indices[index+1]];
  if(d.world.inheritedClipPlane&&!ClipUserPlane(a,b,*d.world.inheritedClipPlane))continue;
  if(!Clip(a,b,d.world.depthClamp))continue;
  if(d.sourcePrimitive==NativeDrawPrimitive::Lines)counter=0;
  auto sa=screen(a),sb=screen(b);sa[3]=0;sb[3]=1;
  Line120Segment s{};static_assert(sizeof(a)==sizeof(s.a));std::memcpy(&s.a,&a,sizeof(a));std::memcpy(&s.b,&b,sizeof(b));
  for(unsigned j=0;j<4;++j)s.viewport[j]=db(vp[j]);s.width=width;s.height=height;s.factor=std::clamp<int>(d.world.stippleFactor,1,256);s.pattern=d.world.stipplePattern;s.counter=counter;
  auto addSpan=[&](U32 count,double t0,double t1){if(uint64_t(p.quads)+count>quadLimit)throw BudgetExceeded{};p.spans.push_back({p.quads,count,U32(p.segments.size()),0,db(t0),db(t1)});p.quads+=count;};
  if(samples>1){
   const double dx=double(sb[0])-sa[0],dy=double(sb[1])-sa[1],length=std::hypot(dx,dy);if(length==0)continue;
   s.flags=1;s.nx=db(-dy/length*d.world.lineWidth*.5);s.ny=db(dx/length*d.world.lineWidth*.5);
   NativeMultisampleLineIntervals(length,d.world.lineStipple,d.world.stipplePattern,d.world.stippleFactor,counter,[&](double t0,double t1){addSpan(1,t0,t1);});
  }else{
   double enter=0,leave=1;
   const auto clip=[&](double a,double b,double lo,double hi){if(a==b)return a>=lo&&a<=hi;double x=(lo-a)/(b-a),y=(hi-a)/(b-a);if(x>y)std::swap(x,y);enter=std::max(enter,x);leave=std::min(leave,y);return enter<=leave;};
   if(!clip(sa[0],sb[0],0,width)||!clip(sa[1],sb[1],0,height)||!clip(sa[2],sb[2],-1,1))continue;
   const auto origA=sa,origB=sb;
   for(unsigned j=0;j<4;++j){sa[j]=float(origA[j]+(double(origB[j])-origA[j])*enter);sb[j]=float(origA[j]+(double(origB[j])-origA[j])*leave);}
   if(double(sb[0])==sa[0]&&double(sb[1])==sa[1])continue;
   const auto snap=[](double x){return std::floor(x*256+.5)/256;};
   const double ax=snap(sa[0]),ay=snap(sa[1]),bx=snap(sb[0]),by=snap(sb[1]),dx=bx-ax,dy=by-ay;if(dx==0&&dy==0)continue;
   s.snapped[0]=int(ax*256);s.snapped[1]=int(ay*256);s.snapped[2]=int(bx*256);s.snapped[3]=int(by*256);
   const bool xm=std::abs(dx)>=std::abs(dy);s.flags=(xm?2:0)|(d.world.lineStipple?4:0);s.count=std::max(1,int(std::floor(d.world.lineWidth+.5f)));
   const double ma=xm?ax:ay,mb=xm?bx:by;
   int first=std::max(0,int(std::floor(std::min(ma,mb)))-1),last=std::min(int(xm?width:height)-1,int(std::ceil(std::max(ma,mb)))+1);
   auto covered=[&](int m){double t=(m+.5-ma)/(mb-ma);int c=int(std::floor((xm?ay:ax)+t*(xm?dy:dx)));for(int n=c-1;n<=c+1;++n)if(Diamond(ax,ay,bx,by,xm?m:n,xm?n:m))return true;return false;};
   while(first<=last&&!covered(first))++first;while(last>first&&!covered(last))--last;if(first>last)continue;
   s.direction=mb>=ma?1:-1;s.unitFirst=s.direction>0?first:last;s.majorFirst=s.unitFirst;unsigned unitCount=last-first+1,majorCount=unitCount;
   if(d.world.lineStipple){s.majorFirst=s.direction>0?int(std::ceil(ma-.5)):int(std::floor(ma-.5));const int end=s.direction>0?int(std::ceil(mb-.5))-1:int(std::floor(mb-.5))+1;majorCount=unsigned(std::max(0,(end-s.majorFirst)*s.direction+1));}
   if(majorCount)addSpan(majorCount*s.count,0,0);counter+=unitCount;
  }
  std::copy(sa.begin(),sa.end(),s.sa);std::copy(sb.begin(),sb.end(),s.sb);p.segments.push_back(s);
 }
 return p;
}


// The source decoder and its errors remain authoritative. Conservative rejects
// take the old path, including small jobs where exact binary64 GPU setup costs
// more than the original CPU expansion. No new input rejection is introduced.
// Main and separately retained water/terrain/transparent-shadow owners encode
// the line-only producer before any render pass and publish completion after
// the same submission retires. Opaque original-shadow input stays separate.
inline bool SupportedOverlayStage(NativeWorldDrawStage stage) {
 switch(stage) {
  case NativeWorldDrawStage::PreDecals: case NativeWorldDrawStage::PreUnit:
  case NativeWorldDrawStage::World: case NativeWorldDrawStage::AlphaBelowWater:
  case NativeWorldDrawStage::AlphaAboveWater: case NativeWorldDrawStage::WaterPost:
  case NativeWorldDrawStage::AlphaAll:
  case NativeWorldDrawStage::WaterReflection: case NativeWorldDrawStage::WaterRefraction:
  case NativeWorldDrawStage::Ground: case NativeWorldDrawStage::TerrainReflection:
  case NativeWorldDrawStage::ShadowTransparent: return true;
  default: return false;
 }
}
inline std::optional<Packet> TryBuild(const NativeWorldDrawCommand& d,const std::array<double,4>& vp,unsigned width,unsigned height,unsigned samples){
 if(!SupportedOverlayStage(d.stage)||d.primitive!=NativeDrawPrimitive::Lines||d.surface.material!=ScreenMaterial::Solid||d.surface.secondary.texture||
  d.originalShadowInput||d.easyFacing||d.squadHull||d.terraformOverlay||d.losView||d.waterOverlay||d.unitEffect||d.unitGlass||d.distortion||d.objectSpotlight||d.grass||d.resourceSpots||d.sensorLOS||d.surface.splatPainter)return {};
 if(!width||!height||width>16384||height>16384||!samples||samples>32||d.world.lineWidth<=0||d.world.lineWidth>64)return {};
 for(double value:vp)if(!std::isfinite(value)||std::abs(value)>32768)return {};
 if(vp[2]<=0||vp[3]<=0)return {};
 if(d.world.inheritedClipPlane)for(float x:*d.world.inheritedClipPlane)if(!std::isfinite(x)||std::abs(x)>65536)return {};
 for(const auto& v:d.vertices){for(float x:v.position)if(!std::isfinite(x)||std::abs(x)>65536)return {};if(v.position[3]<1.f/65536)return {};for(float x:v.color)if(!std::isfinite(x)||std::abs(x)>65536)return {};for(float x:v.uv)if(!std::isfinite(x)||std::abs(x)>65536)return {};}
 if(d.world.lineStipple&&!d.world.stipplePattern)return {};
 // Ordinary unstippled MSAA emits only one quad per segment. Avoid building a
 // second clipped packet for the common small case that cannot use this path.
 if(samples>1&&!d.world.lineStipple&&d.indices.size()/2<8192)return {};
 if(samples==1&&std::isfinite(d.world.lineWidth)&&uint64_t(d.indices.size()/2)*(std::max(width,height)+4)*uint64_t(std::max(1,int(std::floor(d.world.lineWidth+.5f))))<8192)return {};
 // Reclaim outlines already carry the exact evaluated source vertices. Build
 // validates their PreUnit/Lines owner; fills remain with their paired renderer.
 Packet p;try{p=Build(d,vp,width,height,samples,samples>1?1048576u:500000u);}catch(const BudgetExceeded&){return {};}
 const uint64_t upper=p.quads;
 if(upper<(samples>1?8192u:8192u)||upper>(samples>1?1048576u:500000u))return {};
 if(samples==1&&d.world.lineStipple&&upper*std::popcount(d.world.stipplePattern)<8192u*16u)return {};
 // Perspective inverse is affine in the rounded tag; testing conservative
 // endpoint extrema prevents changing the existing CPU interpolation errors.
 for(const auto& span:p.spans){const auto& s=p.segments[span.segment];double t0=0,t1=1;
  if(samples==1){bool xm=s.flags&2;int last=s.majorFirst+int(span.count/s.count-1)*s.direction;double a=(s.majorFirst+.5-s.sa[xm?0:1])/(double(s.sb[xm?0:1])-s.sa[xm?0:1]);double b=(last+.5-s.sa[xm?0:1])/(double(s.sb[xm?0:1])-s.sa[xm?0:1]);t0=float(s.sa[3]+(s.sb[3]-s.sa[3])*a);t1=float(s.sa[3]+(s.sb[3]-s.sa[3])*b);}
  for(double t:{t0,t1}){double inverse=(1-t)/s.a.position[3]+t/s.b.position[3];if(!std::isfinite(inverse)||inverse<=1e-12)return {};}
 }
 return p;
}
} // namespace rendering::exactWorldLine
