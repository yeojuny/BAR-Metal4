/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePiPData.h"
#include "NativeExactPixelLines.h"
namespace rendering::exactPiPLine {
// Only endpoint positions are evaluated here: exact CPU admission/endpoint
// planning needs them without a GPU readback. Original color/fade, clipped
// color interpolation, per-pixel expansion and final clip conversion run on GPU.
// Small jobs and non-1x attachments retain the unchanged CPU oracle.
inline std::optional<exactWorldLine::Packet> TryBuild(const NativePiPDraw& draw,unsigned width,unsigned height,unsigned samples,bool threshold=true)
{
 if(draw.program.program!=NativePiPProgram::Line||samples!=1||!width||!height||width>16384||height>16384)return {};
 const float lineWidth=draw.raster.lineWidth;
 if(!std::isfinite(lineWidth)||lineWidth<=0||lineWidth>64)return {};
 if(draw.raster.lineStipple&&!draw.raster.stipplePattern)return {};
 const auto& u=draw.program.uniforms.values;
 for(unsigned i=0;i<7;++i)for(float v:u[i])if(!std::isfinite(v)||std::abs(v)>65536)return {};
 if(u[6][0]>.5f&&u[6][2]<1e-6f)return {};
 if(threshold&&uint64_t(draw.vertexCount/2)*(std::max(width,height)+4)*unsigned(std::max(1,int(std::floor(lineWidth+.5f))))<8192)return {};
 ValidateNativePiPDraw(draw);
 std::vector<std::array<float,6>> raw(draw.vertexCount);
 std::vector<ScreenLine> lines;lines.reserve(draw.vertexCount/2);
 const auto position=[&](const std::array<float,6>& v){
  const float x=u[1][0]+v[0]*u[0][0],y=u[1][1]+v[1]*u[0][1];
  const float dx=x-u[4][0],dy=y-u[4][1];
  const float px=u[4][0]+(dx*u[3][1]-dy*u[3][0]),py=u[4][1]+(dx*u[3][0]+dy*u[3][1]);
  const float clipX=px*u[2][0]-1.f,clipY=py*u[2][1]-1.f;
  return ScreenVertex{(clipX+1.f)*.5f*width,(clipY+1.f)*.5f*height,0,{}};
 };
 double estimate=0;
 for(uint32_t i=0;i<draw.vertexCount;++i){
  std::memcpy(raw[i].data(),draw.vertices->bytes.data()+size_t(draw.firstVertex+i)*sizeof(raw[i]),sizeof(raw[i]));
  for(float v:raw[i])if(!std::isfinite(v)||std::abs(v)>65536)return {};
  if(i%2==0)continue;
  const auto start=position(raw[i-1]),end=position(raw[i]);
  for(float v:{start.x,start.y,end.x,end.y})if(!std::isfinite(v)||std::abs(v)>65536)return {};
  estimate+=(std::ceil(std::max(std::abs(double(end.x)-start.x),std::abs(double(end.y)-start.y)))+4)*std::max(1,int(std::floor(lineWidth+.5f)));
  lines.push_back({start,end,lineWidth,draw.raster.lineStipple,draw.raster.stipplePattern,draw.raster.stippleFactor,true});
 }
 if(threshold&&estimate<8192)return {};
 exactWorldLine::Packet packet;
 try {
  packet=exactPixelLine::BuildWithSegment(lines,width,height,1,500000,[&](auto& segment,size_t i,double enter,double leave){
   // Private PiP packet uses the otherwise unused UV and viewport lanes.
   std::copy_n(raw[2*i].data()+2,4,segment.a.uv);
   std::copy_n(raw[2*i+1].data()+2,4,segment.b.uv);
   segment.viewport[0]=exactWorldLine::db(enter);segment.viewport[1]=exactWorldLine::db(leave);
  });
 }catch(const exactWorldLine::BudgetExceeded&){return {};}
 if(threshold&&packet.quads<8192)return {};
 if(threshold&&draw.raster.lineStipple&&uint64_t(packet.quads)*std::popcount(draw.raster.stipplePattern)<8192*16)return {};
 return packet;
}
} // namespace rendering::exactPiPLine
