/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeExactWorldLine.h"
#include "NativeScreenLine.h"
#include "NativeScreenGeometry.h"
namespace rendering::exactPixelLine {
using namespace exactWorldLine;
template<class SegmentCallback>
inline Packet BuildWithSegment(std::span<const ScreenLine> lines,unsigned width,unsigned height,unsigned samples,uint64_t budget,SegmentCallback&& finish){
 if(!samples||samples>32||!width||!height||width>16384||height>16384)throw std::invalid_argument("pixel122 extent");
 Packet p;uint64_t counter=0;
 for(const auto& original:lines){
  auto line=original;if(line.resetStipple)counter=0;
  for(float value:{line.start.x,line.start.y,line.start.z,line.end.x,line.end.y,line.end.z,line.width})if(!std::isfinite(value))throw std::invalid_argument("pixel122 nonfinite");
  if(line.width<=0||line.width>64)throw std::invalid_argument("pixel122 width");
  for(float c:line.start.color)if(!std::isfinite(c))throw std::invalid_argument("pixel122 color");
  for(float c:line.end.color)if(!std::isfinite(c))throw std::invalid_argument("pixel122 color");
  double enter=0,leave=1;
  const auto clip=[&](double a,double b,double low,double high){if(a==b)return a>=low&&a<=high;double lo=(low-a)/(b-a),hi=(high-a)/(b-a);if(lo>hi)std::swap(lo,hi);enter=std::max(enter,lo);leave=std::min(leave,hi);return enter<=leave;};
  if(!clip(line.start.x,line.end.x,0,width)||!clip(line.start.y,line.end.y,0,height)||!clip(line.start.z,line.end.z,-1,1))continue;
  const auto at=[&](double t){ScreenVertex v;v.x=float(original.start.x+(double(original.end.x)-original.start.x)*t);v.y=float(original.start.y+(double(original.end.y)-original.start.y)*t);v.z=float(original.start.z+(double(original.end.z)-original.start.z)*t);for(unsigned c=0;c<4;++c)v.color[c]=float(original.start.color[c]+(double(original.end.color[c])-original.start.color[c])*t);return v;};
  line.start=at(enter);line.end=at(leave);
  const double dx=double(line.end.x)-line.start.x,dy=double(line.end.y)-line.start.y,length=dx*dx+dy*dy;if(length==0)continue;
  Line120Segment s{};s.width=width;s.height=height;s.factor=std::clamp(line.factor,1,256);s.pattern=line.pattern;s.counter=counter;
  const auto set=[](Line120Vertex& v,const ScreenVertex& a){v.position[0]=a.x;v.position[1]=a.y;v.position[2]=a.z;v.position[3]=1;std::copy(a.color.begin(),a.color.end(),v.color);};set(s.a,line.start);set(s.b,line.end);
  std::copy_n(s.a.position,4,s.sa);std::copy_n(s.b.position,4,s.sb);
  const auto span=[&](uint32_t count,double a,double b){if(uint64_t(p.quads)+count>budget)throw BudgetExceeded{};p.spans.push_back({p.quads,count,uint32_t(p.segments.size()),0,db(a),db(b)});p.quads+=count;};
  if(samples>1){
   const double distance=std::sqrt(length);s.flags=1;s.nx=db(-dy/distance*line.width*.5);s.ny=db(dx/distance*line.width*.5);
   NativeMultisampleLineIntervals(distance,line.stippled,line.pattern,line.factor,counter,[&](double a,double b){span(1,a,b);});
  }else{
   const auto snap=[](double x){return std::floor(x*256+.5)/256;};
   const double ax=snap(line.start.x),ay=snap(line.start.y),bx=snap(line.end.x),by=snap(line.end.y),rdx=bx-ax,rdy=by-ay;if(rdx==0&&rdy==0)continue;
   s.snapped[0]=int(ax*256);s.snapped[1]=int(ay*256);s.snapped[2]=int(bx*256);s.snapped[3]=int(by*256);
   const bool xm=std::abs(rdx)>=std::abs(rdy);s.flags=(xm?2:0)|(line.stippled?4:0)|(line.polygonEdge?8:0);s.count=std::max(1,int(std::floor(line.width+.5f)));
   const double ma=xm?ax:ay,mb=xm?bx:by;int first=std::max(0,int(std::floor(std::min(ma,mb)))-1),last=std::min(int(xm?width:height)-1,int(std::ceil(std::max(ma,mb)))+1);
   const auto covered=[&](int m){double t=(m+.5-ma)/(mb-ma);int c=int(std::floor((xm?ay:ax)+t*(xm?rdy:rdx)));for(int n=c-1;n<=c+1;++n)if(Diamond(ax,ay,bx,by,xm?m:n,xm?n:m))return true;return false;};
   while(first<=last&&!covered(first))++first;while(last>first&&!covered(last))--last;if(first>last)continue;
   s.direction=mb>=ma?1:-1;s.unitFirst=s.direction>0?first:last;s.majorFirst=s.unitFirst;unsigned unitCount=last-first+1,majorCount=unitCount;
   if(line.stippled||line.polygonEdge){s.majorFirst=s.direction>0?int(std::ceil(ma-.5)):int(std::floor(ma-.5));const int end=s.direction>0?int(std::ceil(mb-.5))-1:int(std::floor(mb-.5))+1;majorCount=unsigned(std::max(0,(end-s.majorFirst)*s.direction+1));}
   if(majorCount)span(majorCount*s.count,0,0);counter+=unitCount;
  }
  finish(s,size_t(&original-lines.data()),enter,leave);
  p.segments.push_back(s);
 }
 return p;
}
inline Packet Build(std::span<const ScreenLine> lines,unsigned width,unsigned height,unsigned samples,uint64_t budget=500000){
 return BuildWithSegment(lines,width,height,samples,budget,[](auto&,size_t,double,double){});
}
} // namespace rendering::exactPixelLine

namespace rendering {
inline std::shared_ptr<const NativeScreenGeometry> TryExactPixelLines(std::span<const ScreenLine> lines,unsigned width,unsigned height,unsigned samples,unsigned logicalWidth,unsigned logicalHeight){
 if(!width||!height||width>16384||height>16384||!logicalWidth||!logicalHeight||logicalWidth>16384||logicalHeight>16384||!samples||samples>32)return {};
 double estimate=0;
 for(const auto& line:lines){
  for(float v:{line.start.x,line.start.y,line.start.z,line.end.x,line.end.y,line.end.z,line.width})if(!std::isfinite(v)||std::abs(v)>65536)return {};
  for(float c:line.start.color)if(!std::isfinite(c)||std::abs(c)>65536)return {};
  for(float c:line.end.color)if(!std::isfinite(c)||std::abs(c)>65536)return {};
  if(line.width<=0||line.width>64)return {};
  const double dx=std::abs(double(line.end.x)-line.start.x),dy=std::abs(double(line.end.y)-line.start.y);
  estimate+=samples>1?(line.stippled?std::ceil(dx+dy)+1:1):(std::ceil(std::max(dx,dy))+4)*std::max(1,int(std::floor(line.width+.5f)));
 }
 if(estimate<8192)return {};
 exactWorldLine::Packet packet;try{packet=exactPixelLine::Build(lines,width,height,samples);}catch(const exactWorldLine::BudgetExceeded&){return {};}
 if(packet.quads<8192)return {};
 uint64_t visible=0;for(const auto& span:packet.spans){const auto& s=packet.segments[span.segment];visible+=uint64_t(span.count)*((s.flags&4)?std::popcount(s.pattern):16);}
 if(visible<8192*16)return {};
 auto g=std::make_shared<NativeScreenGeometry>();g->kind=NativeScreenGeometryKind::ScreenPixelLines;g->vertices=exactScreenLine::Capture(packet.segments);g->instances=exactScreenLine::Capture(packet.spans);g->count=packet.quads*6;g->instanceCount=1;g->coordinateExtent={logicalWidth,logicalHeight};
 // This kind carries exact raster extents in otherwise unused matrix lanes.
 g->projection[0]=float(width);g->projection[5]=float(height);screenGeometry::Validate(*g);return g;
}
}
