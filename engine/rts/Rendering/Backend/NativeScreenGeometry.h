/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include "NativeRetainedFont170.h"
#include "NativeSourceLineData133.h"
#include "NativeProfilerHistogramData132.h"
#include "NativeExactScreenLinePacket.h"
#include "ScreenDrawList.h"
#include "WorldFrameData.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace rendering {
enum class NativeScreenGeometryKind : uint32_t { SpectatorHUD, FrameGrapher, TerraformModel, ScreenFont, ScreenUI, ScreenLines, ScreenPixelLines, ProfilerHistogram, CommonSourceLines };
struct NativeScreenFontInput {
// Borrowed only during synchronous AppendGeometry. The draw list copies the
 // raw streams before retaining a matrix-only packet. Never kept by the GPU.
 std::span<const ScreenVertex> vertices;
 std::span<const ScreenTexCoord> uv;
 std::span<const uint32_t> indices;
};
// Original streams remain immutable until submission retirement. No expanded
// instance x vertex array or per-vertex model transform is recorded on the CPU.
struct NativeScreenGeometry {
 NativeScreenGeometryKind kind=NativeScreenGeometryKind::SpectatorHUD;
 NativeBufferSnapshotRef vertices,instances,indices;
 uint32_t count=0,instanceCount=0;
 std::shared_ptr<const WorldModel> model;
 uint32_t piece=0;
 std::array<float,16> modelView{},projection{};
 std::array<std::array<float,4>,3> cofactors{};
 float determinant=0;
 std::shared_ptr<const NativeScreenFontInput> fontInput;
 NativeRetainedFont170::Ref retainedFont;
 std::array<uint32_t,2> coordinateExtent{};
 std::array<double,3> fontBounds{};
 // ScreenUI only: primary affine UV rows. Packed into the unused normal
 // cofactor lanes of the existing shared GPU uniform; no extra upload.
 std::array<std::array<float,4>,2> textureRows{};
 bool affineTexture=false;
 std::shared_ptr<const NativeProfilerHistogramInput132> profilerHistogram;
 std::shared_ptr<const NativeSourceLineInput133> sourceLines;
};
namespace screenGeometry {
inline bool UsesFrameVertices(NativeScreenGeometryKind kind){return kind==NativeScreenGeometryKind::ScreenFont||kind==NativeScreenGeometryKind::ScreenUI;}
inline bool UsesFrameVertices(const NativeScreenGeometry& d){return UsesFrameVertices(d.kind)&&!d.retainedFont;}
inline bool UsesPackedScreenUniforms170(const NativeScreenGeometry& d){return UsesFrameVertices(d.kind);}
inline void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
inline std::array<float,4> Row(const NativeBufferSnapshotRef& s,size_t offset,unsigned components=4){
 Check(s&&offset<=s->bytes.size()&&16<=s->bytes.size()-offset,"Typed screen vec4 exceeds immutable storage");
 std::array<float,4> row;std::memcpy(row.data(),s->bytes.data()+offset,16);
 for(unsigned k=0;k<components;++k)Check(std::isfinite(row[k]),"Typed screen source is nonfinite");return row;
}
inline void Stream(const NativeBufferSnapshotRef& s,NativeBufferKind kind,size_t stride){
 Check(s&&s->object&&s->revision&&s->layout.kind==kind&&s->layout.stride==stride&&s->bytes.size()==CheckedNativeBufferByteSize(s->layout),"Typed screen original stream extent differs");
}
inline std::array<float,4> Transform(const std::array<float,16>& m,std::array<float,4> p){
 std::array<float,4> result{};for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)result[r]+=m[k*4+r]*p[k];return result;
}
inline void NormalMatrix(NativeScreenGeometry& d){
 const auto& m=d.modelView;
 const auto cross=[](std::array<float,3> a,std::array<float,3> b){return std::array<float,4>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0],0};};
 const std::array<float,3> a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]};
 d.cofactors={cross(b,c),cross(c,a),cross(a,b)};
 d.determinant=a[0]*d.cofactors[0][0]+a[1]*d.cofactors[0][1]+a[2]*d.cofactors[0][2];
 Check(std::isfinite(d.determinant)&&d.determinant!=0,"Terraform Capture singular modelview normal matrix");
}
inline bool CaptureAffineTexture(NativeScreenGeometry& d,const std::array<float,16>& m){
 d.affineTexture=false;
 // Original q must be exactly one. Projective and exceptional matrices stay
 // on ProjectUV, including its validation and original world-q rejection.
 if(m[3]!=0||m[7]!=0||m[15]!=1)return false;
 for(float x:m)if(!(std::abs(x)<=1e10f))return false;
 // Metal arithmetic flushes subnormals even with safe math. Nonzero factors
 // and inputs in [1e-10,1e10] keep all products and cancellation residues
 // normal (binary32 products have at most24 significant bits). Tiny inputs
 // retain the CPU path, including its gradual underflow behavior.
 for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col){const float x=m[col*4+row];if(x!=0&&std::abs(x)<1e-10f)return false;}
 for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col)d.textureRows[row][col]=m[col*4+row];
 return d.affineTexture=true;
}
inline bool AffineTextureUVSafe(std::span<const ScreenTexCoord> uv){
 for(const auto& t:uv)for(unsigned k=0;k<2;++k)if(!(std::abs(t[k])<=1e10f)||(t[k]!=0&&std::abs(t[k])<1e-10f))return false;
 return true;
}
inline ScreenTexCoord TransformAffineTexture(const NativeScreenGeometry& d,const ScreenTexCoord& t){
 ScreenTexCoord out=t;const float p[]{t[0],t[1],0,1};
 for(unsigned row=0;row<2;++row){float value=0;for(unsigned col=0;col<4;++col)value+=d.textureRows[row][col]*p[col];
  Check(std::isfinite(value),"Native UI invalid texture coordinate");out[row]=value;}
 return out;
}
inline bool FontProjectionSafe(const NativeScreenGeometry& d){
 if(!d.coordinateExtent[0]||!d.coordinateExtent[1])return false;
 std::array<double,4> bounds{d.fontBounds[0],d.fontBounds[1],d.fontBounds[2],1};
 const double limit=std::numeric_limits<float>::max()/16.0;
 for(double x:bounds)if(!std::isfinite(x)||x<0||x>limit)return false;
 for(const auto* m:{&d.modelView,&d.projection}){
  for(float x:*m)if(!std::isfinite(x))return false;
  if((*m)[3]!=0||(*m)[7]!=0||(*m)[11]!=0||(*m)[15]!=1)return false;
  std::array<double,4> next{};
  for(unsigned r=0;r<4;++r){for(unsigned k=0;k<4;++k)next[r]+=std::abs(double((*m)[k*4+r]))*bounds[k];if(next[r]>limit)return false;}
  bounds=next;
 }
 for(unsigned r=0;r<2;++r)if((bounds[r]+1)*double(d.coordinateExtent[r])>limit)return false;
 return true;
}
inline bool CaptureFontBounds(NativeScreenGeometry& d,std::span<const ScreenVertex> vertices){
 d.fontBounds={};
 for(const auto& v:vertices){const float p[]{v.x,v.y,v.z};for(unsigned k=0;k<3;++k){if(!std::isfinite(p[k]))return false;d.fontBounds[k]=std::max(d.fontBounds[k],std::abs(double(p[k])));}}
 return FontProjectionSafe(d);
}
inline void Validate(const NativeScreenGeometry& d){
 Check(d.kind<=NativeScreenGeometryKind::CommonSourceLines,"Unknown typed screen geometry");
 Check(!d.retainedFont||d.kind==NativeScreenGeometryKind::ScreenFont,"Retained font owner competes with another geometry kind");
 if(d.retainedFont){
  Check(d.retainedFont->ScreenValidated()&&!d.fontInput&&!d.vertices&&!d.instances&&!d.indices&&!d.model&&!d.profilerHistogram&&!d.sourceLines&&!d.affineTexture,
   "Retained font lacks a sealed screen source or has competing geometry");
  Check(d.count==d.retainedFont->Indices().size()&&d.fontBounds==d.retainedFont->ScreenBounds(),"Retained font extent/bounds differ from its source");
 }
 if(d.kind==NativeScreenGeometryKind::CommonSourceLines){
  Check(d.sourceLines&&!d.vertices&&!d.instances&&!d.indices&&!d.model&&!d.fontInput&&!d.profilerHistogram&&!d.affineTexture&&d.coordinateExtent[0]&&d.coordinateExtent[1],"Common line source has competing geometry");
  sourceLine133::Validate(*d.sourceLines);Check(d.count==4&&d.instanceCount==d.sourceLines->segments.size(),"Common line instance extent differs");return;
 }
 Check(!d.sourceLines,"Other screen geometry has competing common lines");
 if(d.kind==NativeScreenGeometryKind::ProfilerHistogram){
  Check(d.profilerHistogram&&!d.vertices&&!d.instances&&!d.indices&&!d.model&&!d.fontInput&&!d.affineTexture,"Profiler source packet has competing geometry");
  const auto& p=*d.profilerHistogram;profilerSource132::Validate(p);
  Check(p.count>=2&&(p.samples==1||p.samples==4)&&p.lineWidth<=64&&d.count==4&&d.instanceCount==p.count-1,"Profiler source segment extent/state differs");return;
 }
 Check(!d.profilerHistogram,"Other screen geometry has competing Profiler source");
 if(d.kind==NativeScreenGeometryKind::ScreenLines||d.kind==NativeScreenGeometryKind::ScreenPixelLines){
  Check(d.instanceCount==1&&!d.indices&&!d.model&&!d.fontInput&&!d.affineTexture&&d.coordinateExtent[0]&&d.coordinateExtent[1],"Exact screen line geometry metadata differs");
  const bool pixel=d.kind==NativeScreenGeometryKind::ScreenPixelLines;
  if(pixel)for(float x:{d.projection[0],d.projection[5]})Check(x>=1&&x<=16384&&std::floor(x)==x,"Exact pixel line raster extent differs");
  exactScreenLine::Validate(d.vertices,d.instances,d.count,pixel?15u:7u);return;
 }
 if(d.affineTexture){
  Check(d.kind==NativeScreenGeometryKind::ScreenUI,"Affine UV transform requires immediate ScreenUI");
  for(const auto& row:d.textureRows)for(float x:row)Check(std::abs(x)<=1e10f&&(x==0||std::abs(x)>=1e-10f),"Affine UV transform exceeds finite envelope");
 }
 if(UsesFrameVertices(d.kind)){
  Check(d.instanceCount==1&&d.count%3==0&&FontProjectionSafe(d),"Screen font requires finite original orthographic transform");
  return;
 }
 if(d.kind==NativeScreenGeometryKind::TerraformModel){
  Check(d.model&&d.piece<d.model->pieces.size()&&d.instanceCount==1,"Terraform model piece missing");
  const auto& p=d.model->pieces[d.piece];
  Check(d.count==p.indices.size()&&d.count%3==0&&p.vertices.size()<=UINT32_MAX,"Terraform original triangle extent differs");
  for(auto x:p.indices)Check(x<p.vertices.size(),"Terraform actual model index exceeds piece");
  for(const auto* m:{&d.modelView,&d.projection})for(float x:*m)Check(std::isfinite(x),"Terraform matrix is nonfinite");
  auto expected=d;NormalMatrix(expected);
  Check(expected.cofactors==d.cofactors&&expected.determinant==d.determinant,"Terraform normal matrix differs from original modelview");
  std::array<double,4> bounds{0,0,0,1},normals{};
  for(const auto& v:p.vertices){for(unsigned k=0;k<3;++k){
   Check(std::isfinite(v.position[k])&&std::isfinite(v.normal[k]),"Terraform model source is nonfinite");
   bounds[k]=std::max(bounds[k],std::abs(double(v.position[k])));normals[k]=std::max(normals[k],std::abs(double(v.normal[k])));
  }for(float x:v.uv[0])Check(std::isfinite(x),"Terraform source UV is nonfinite");}
  // A cheap conservative bound proves finite shader arithmetic for normal
  // model data. Exceptional magnitudes retain the original exact validation;
  // they never materialize a transformed CPU vertex stream.
  bool safe=true;const double limit=std::numeric_limits<float>::max()/16.0;
  for(const auto* m:{&d.modelView,&d.projection}){std::array<double,4> next{};for(unsigned r=0;r<4;++r){for(unsigned k=0;k<4;++k)next[r]+=std::abs(double((*m)[k*4+r]))*bounds[k];safe&=next[r]<=limit;}bounds=next;}
  for(unsigned r=0;r<3;++r){double bound=0;for(unsigned k=0;k<3;++k)bound+=std::abs(double(d.cofactors[k][r]))*normals[k];safe&=bound<=limit&&bound/std::abs(double(d.determinant))<=limit;}
  if(!safe)for(const auto& v:p.vertices){
   const auto clip=Transform(d.projection,Transform(d.modelView,{v.position[0],v.position[1],v.position[2],1}));
   for(float x:clip)Check(std::isfinite(x),"Terraform Capture nonfinite model clip coordinate");
   for(unsigned r=0;r<3;++r){const float n=(d.cofactors[0][r]*v.normal[0]+d.cofactors[1][r]*v.normal[1]+d.cofactors[2][r]*v.normal[2])/d.determinant;Check(std::isfinite(n),"Terraform Capture nonfinite eye-space normal");}
  }
  return;
 }
 const bool hud=d.kind==NativeScreenGeometryKind::SpectatorHUD;
 Stream(d.vertices,NativeBufferKind::Vertex,16);Stream(d.instances,NativeBufferKind::Vertex,hud?32:16);
 Check(d.instanceCount<=d.instances->layout.elements&&d.count%3==0&&(!d.instanceCount||d.count<=UINT32_MAX/d.instanceCount),"Typed screen original instanced range differs");
 if(hud){Stream(d.indices,NativeBufferKind::Index,4);Check(d.count<=d.indices->layout.elements,"Spectator index range exceeds buffer");}
 else Check(d.count==6&&d.vertices->layout.elements==6&&!d.indices,"FrameGrapher original six vertices differ");
 if(!d.instanceCount||!d.count)return;
 std::array<float,4> lo,hi;lo.fill(std::numeric_limits<float>::max());hi.fill(-std::numeric_limits<float>::max());
 for(uint32_t j=0;j<d.count;++j){uint32_t id=j;if(hud)std::memcpy(&id,d.indices->bytes.data()+size_t(j)*4,4);Check(id<d.vertices->layout.elements,"Spectator index outside original vertex stream");
  const auto row=Row(d.vertices,size_t(id)*16,hud?4:2);for(unsigned k=0;k<(hud?4u:2u);++k){lo[k]=std::min(lo[k],row[k]);hi[k]=std::max(hi[k],row[k]);}
 }
 for(uint32_t n=0;n<d.instanceCount;++n){const auto row=Row(d.instances,size_t(n)*(hud?32:16));if(hud){(void)Row(d.instances,size_t(n)*32+16);for(unsigned k=0;k<4;++k)Check(std::isfinite(lo[k]+row[k])&&std::isfinite(hi[k]+row[k]),"Spectator position plus bias is nonfinite");}}
}
} // namespace screenGeometry
} // namespace rendering
