/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldFontGeometry.h"
#include "NativeScreenGeometry.h"
namespace rendering {
inline bool IsWorldMatrixGPU(NativeDrawPrimitive p){return p==NativeDrawPrimitive::GPUFontTriangles||p==NativeDrawPrimitive::GPUFillTriangles;}
// Optional affine UV rows occupy one extra existing vertex slot. Ordinary
// identity packets and every font retain their original144-byte header.
using NativeWorldTextureRows=std::array<std::array<float,4>,2>;
inline bool WorldTextureRowsSafe(const NativeWorldTextureRows& rows){
 for(const auto& row:rows)for(float x:row)if(!(std::abs(x)<=1e10f)||(x!=0&&std::abs(x)<1e-10f))return false;
 return true;
}
inline bool WorldFillTextureGPU(const NativeWorldDrawCommand& c){
 return c.sourcePrimitive==NativeDrawPrimitive::GPUFillTriangles&&WorldFontHeader(c).options[1]==1;
}
inline size_t WorldMatrixHeaderVertices(const NativeWorldDrawCommand& c){return WorldFillTextureGPU(c)?4:3;}
inline NativeWorldTextureRows WorldFillTextureRows(const NativeWorldDrawCommand& c){
 if(c.vertices.size()<4)throw std::runtime_error("World GPU texture header missing");
 NativeWorldTextureRows rows;std::memcpy(&rows,&c.vertices[3],sizeof(rows));return rows;
}
inline ScreenTexCoord WorldTextureTransform(const NativeWorldTextureRows& rows,const ScreenTexCoord& t){
 ScreenTexCoord out=t;const float p[]{t[0],t[1],0,1};
 for(unsigned row=0;row<2;++row){float value=0;for(unsigned col=0;col<4;++col)value+=rows[row][col]*p[col];
  if(!std::isfinite(value))throw std::runtime_error("Native UI invalid texture coordinate");out[row]=value;}
 return out;
}
inline bool WorldFillMode(NativeDrawPrimitive p){return p==NativeDrawPrimitive::Triangles||p==NativeDrawPrimitive::TriangleStrip||p==NativeDrawPrimitive::TriangleFan||p==NativeDrawPrimitive::Quads||p==NativeDrawPrimitive::Polygon;}
// Depth test/write, compare, clamp, polygon offset and stencil remain the
// existing fixed-function GPU state. Only vertex matrix evaluation moves;
// preserve the precision envelope and every other eligibility restriction.
inline bool WorldFillGPUState(NativeWorldDrawStage stage,const ScreenDrawState& s,const NativeWorldDrawState& w){
 return (stage==NativeWorldDrawStage::World||stage==NativeWorldDrawStage::PreUnit)&&
  w.frontPolygonMode==NativePolygonMode::Fill&&w.backPolygonMode==NativePolygonMode::Fill&&!w.inheritedClipPlane&&
  !w.invertColorUnorm8&&!s.terraformCapture&&!s.diffusePainter&&!s.splatPainter&&!s.decalCapture&&!s.deferredVisualizer&&
  !s.secondary.texture&&(s.material==ScreenMaterial::Solid||s.material==ScreenMaterial::RGBA||s.material==ScreenMaterial::FontCoverage);
}
inline void ValidateWorldFillGPUHeader(const NativeWorldDrawCommand& c){
 if(c.sourcePrimitive!=NativeDrawPrimitive::GPUFillTriangles)return;
 if(c.primitive!=NativeDrawPrimitive::Triangles||!WorldFillGPUState(c.stage,c.surface,c.world)||
  c.originalShadowInput||c.easyFacing||c.squadHull||c.terraformOverlay||c.losView||c.waterOverlay||c.unitEffect||c.unitGlass||c.distortion||c.objectSpotlight||c.grass||c.resourceSpots||c.sensorLOS||c.reclaimField)
  throw std::runtime_error("World GPU fill has competing owner/raster state");
 const auto h=WorldFontHeader(c);
 if((h.options[0]!=0&&h.options[0]!=1)||(h.options[1]!=0&&h.options[1]!=1)||h.options[2]!=0||h.options[3]!=0||!WorldFontMatricesSafe(h))throw std::runtime_error("World GPU fill transform invalid");
 if(h.options[1]){
  if(!WorldTextureRowsSafe(WorldFillTextureRows(c)))throw std::runtime_error("World GPU texture rows invalid");
  for(float x:c.vertices[3].uv)if(x!=0)throw std::runtime_error("World GPU texture header padding invalid");
 }
}
inline NativeWorldDrawCommand MakeWorldFillGPUCommand(NativeWorldDrawStage stage,NativeDrawPrimitive mode,
 std::vector<NativeWorldDrawVertex> vertices,const ScreenDrawState& surface,const NativeWorldDrawState& world,const NativeWorldFontHeader& h,const NativeWorldTextureRows* textureRows=nullptr){
 const uint32_t headerVertices=textureRows?4:3;
 if((h.options[1]!=(textureRows?1.f:0.f))||(textureRows&&!WorldTextureRowsSafe(*textureRows)))throw std::runtime_error("World GPU texture header ownership differs");
 if(!WorldFillMode(mode)||!WorldFillGPUState(stage,surface,world)||!WorldFontMatricesSafe(h)||vertices.size()<headerVertices||vertices.size()>4u*1024u*1024u)
  throw std::runtime_error("World GPU fill outside verified state/bounds");
 NativeWorldDrawCommand c;c.stage=stage;c.primitive=NativeDrawPrimitive::Triangles;c.sourcePrimitive=NativeDrawPrimitive::GPUFillTriangles;c.surface=surface;c.world=world;
 c.vertices=std::move(vertices);std::memcpy(c.vertices.data(),&h,sizeof(h));
 if(textureRows){std::memcpy(&c.vertices[3],textureRows,sizeof(*textureRows));c.vertices[3].uv={};}
 const uint32_t n=c.vertices.size()-headerVertices;
 const auto triangle=[&](uint32_t a,uint32_t b,uint32_t d){c.indices.insert(c.indices.end(),{a+headerVertices,b+headerVertices,d+headerVertices});};
 // Original canonical winding/quad diagonal, including incomplete tail rules.
 switch(mode){
 case NativeDrawPrimitive::Triangles:for(uint32_t i=0;i+2<n;i+=3)triangle(i,i+1,i+2);break;
 case NativeDrawPrimitive::TriangleStrip:for(uint32_t i=0;i+2<n;++i)if(i&1)triangle(i+1,i,i+2);else triangle(i,i+1,i+2);break;
 case NativeDrawPrimitive::Quads:for(uint32_t i=0;i+3<n;i+=4){triangle(i,i+1,i+3);triangle(i+1,i+2,i+3);}break;
 default:for(uint32_t i=1;i+1<n;++i)triangle(0,i,i+1);break;
 }
 return c;
}
// Rare unsafe input reuses this vector and exactly the old ordered CPU math.
// Normal inputs never traverse this fallback or make another vertex copy.
inline void WorldFillCPUFallback(std::vector<NativeWorldDrawVertex>& vertices,const NativeWorldFontHeader& h,const NativeWorldTextureRows* textureRows=nullptr){
 vertices.erase(vertices.begin(),vertices.begin()+(textureRows?4:3));
 const auto transform=[](const std::array<float,16>& m,const std::array<float,4>& p){std::array<float,4> r{};for(unsigned row=0;row<4;++row)for(unsigned k=0;k<4;++k)r[row]+=m[k*4+row]*p[k];return r;};
 for(auto& v:vertices){v.position=transform(h.projection,transform(h.modelView,v.position));if(h.options[0])v.position[2]=2*v.position[2]-v.position[3];if(textureRows)v.uv=WorldTextureTransform(*textureRows,v.uv);}
}
}
