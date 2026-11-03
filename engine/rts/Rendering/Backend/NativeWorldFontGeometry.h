/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeIndexedWorldGeometry.h"
#include <cmath>
#include <cstring>
#include <bit>
#include <algorithm>
namespace rendering {
// One immutable upload carries the two original matrices and raw font vertices.
// The header occupies three existing 48-byte vertex slots; indices address only
// the following raw vertices. The explicit source tag forbids interpreting this
// typed packet as ordinary homogeneous clip geometry. No command ABI change.
struct NativeWorldFontHeader {
 std::array<float,16> modelView,projection;
 std::array<float,4> options; // zero-to-one source depth; remaining lanes reserved
};
static_assert(sizeof(NativeWorldFontHeader)==3*sizeof(NativeWorldDrawVertex));
inline NativeWorldFontHeader WorldFontHeader(const NativeWorldDrawCommand& c){
 if(c.vertices.size()<3)throw std::runtime_error("World GPU font header missing");
 NativeWorldFontHeader h;std::memcpy(&h,c.vertices.data(),sizeof(h));return h;
}
// Conservative finite-input envelope: two4x4 transforms are bounded by
// 16*1e30, far below float overflow, including the source depth conversion.
// Exceptional values take the pre-existing CPU path; normal world values fit.
inline constexpr float WorldFontInputLimit=1e10f;
// Metal may flush subnormal operands/results. Keep both matrix products and
// their cancellation residues normal, rather than treating "finite" as enough.
inline bool WorldFontPositionSafe(const std::array<float,4>& p,int minimumExponent=-126){
 for(float x:p){if(!(std::abs(x)<=WorldFontInputLimit))return false;
  const auto bits=std::bit_cast<uint32_t>(x)&0x7fffffffu;
  if(bits&&((bits>>23)==0||int(bits>>23)-127<minimumExponent))return false;
 }return true;
}
inline bool WorldFontMatricesSafe(const NativeWorldFontHeader& h){
 for(const auto* m:{&h.modelView,&h.projection})for(float x:*m){
  if(!(std::abs(x)<=WorldFontInputLimit))return false;
  const auto bits=std::bit_cast<uint32_t>(x)&0x7fffffffu;if(bits&&(bits>>23)==0)return false;
 }return true;
}
inline int WorldFontMinimumInputExponent(const NativeWorldFontHeader& h){
 const auto minimum=[](const std::array<float,16>& m){int result=34;
  for(float x:m){const auto bits=std::bit_cast<uint32_t>(x)&0x7fffffffu;if(bits)result=std::min(result,int(bits>>23)-127);}
  return result==34?0:result;
 };
 // A rounded binary32 product has a quantum >=2^(eA+eB-23).
 // Four ordered adds cannot introduce a lower quantum. Two transforms thus
 // have quantum >=2^(eInput+eMV+eProj-46). Require >=2^-125 so the final
 // depth conversion by1/2 also remains normal. The existing1e10 bounds
 // independently prevent overflow. Also protect cancellation at the first
 // transform: eInput+eMV-23 must be >=-126 before multiplying again.
 const int mv=minimum(h.modelView),projection=minimum(h.projection);
 return std::max(-103-mv,-79-mv-projection);
}
inline bool WorldFontTransformSafe(std::span<const NativeWorldDrawVertex> vertices,const NativeWorldFontHeader& h){
 if(!WorldFontMatricesSafe(h))return false;const int minimum=WorldFontMinimumInputExponent(h);for(const auto& v:vertices)if(!WorldFontPositionSafe(v.position,minimum))return false;return true;
}
inline bool WorldFontGPUState(NativeWorldDrawStage stage,const ScreenDrawState& s,const NativeWorldDrawState& w){
 return (stage==NativeWorldDrawStage::World||stage==NativeWorldDrawStage::PreUnit)&&
  w.frontPolygonMode==NativePolygonMode::Fill&&w.backPolygonMode==NativePolygonMode::Fill&&!w.inheritedClipPlane&&
  !w.depthTest&&!w.invertColorUnorm8&&!s.terraformCapture&&!s.diffusePainter&&!s.splatPainter&&!s.decalCapture&&!s.deferredVisualizer&&
  s.texture&&!s.secondary.texture&&(s.material==ScreenMaterial::FontCoverage||s.material==ScreenMaterial::RGBA);
}
inline NativeWorldDrawCommand MakeWorldFontGPUCommand(NativeWorldDrawStage stage,NativeIndexedWorldGeometry geometry,
 const ScreenDrawState& surface,const NativeWorldDrawState& world,const NativeWorldFontHeader& header){
 if(!WorldFontGPUState(stage,surface,world)||!WorldFontMatricesSafe(header)||geometry.vertices.size()<3||geometry.vertices.size()>4u*1024u*1024u)
  throw std::runtime_error("World GPU font packet outside verified state/bounds");
 // The producer reserved the header before compaction: no second vertex move.
 std::memcpy(geometry.vertices.data(),&header,sizeof(header));
 // RecordGPUFont validates raw indices and attributes before publication.
 NativeWorldDrawCommand c;c.stage=stage;c.primitive=NativeDrawPrimitive::Triangles;c.sourcePrimitive=NativeDrawPrimitive::GPUFontTriangles;
 c.vertices=std::move(geometry.vertices);c.indices=std::move(geometry.indices);c.surface=surface;c.world=world;return c;
}
inline void ValidateWorldFontGPUHeader(const NativeWorldDrawCommand& c){
 if(c.sourcePrimitive!=NativeDrawPrimitive::GPUFontTriangles)return;
 if(c.primitive!=NativeDrawPrimitive::Triangles||!WorldFontGPUState(c.stage,c.surface,c.world)||
  c.originalShadowInput||c.easyFacing||c.squadHull||c.terraformOverlay||c.losView||c.waterOverlay||c.unitEffect||c.unitGlass||c.distortion||c.objectSpotlight||c.grass||c.resourceSpots||c.sensorLOS||c.reclaimField)
  throw std::runtime_error("World GPU font has competing owner/raster state");
 const auto h=WorldFontHeader(c);
 if((h.options[0]!=0&&h.options[0]!=1)||h.options[1]!=0||h.options[2]!=0||h.options[3]!=0||!WorldFontMatricesSafe(h))throw std::runtime_error("World GPU font transform invalid");
}
// Standalone numeric tests use the complete validator. The recorder performs
// these same raw checks in its existing single attribute/index validation pass.
inline void ValidateWorldFontGPUCommand(const NativeWorldDrawCommand& c){
 ValidateWorldFontGPUHeader(c);if(c.sourcePrimitive!=NativeDrawPrimitive::GPUFontTriangles)return;
 const auto raw=std::span(c.vertices).subspan(3);
 const int minimum=WorldFontMinimumInputExponent(WorldFontHeader(c));
 for(const auto& v:raw)if(!WorldFontPositionSafe(v.position,minimum))throw std::runtime_error("World GPU font raw position outside safe envelope");
 for(auto index:c.indices)if(index<3||index>=c.vertices.size())throw std::runtime_error("World GPU font index references header/outside raw input");
 for(const auto& v:raw)if(v.position[3]!=1)throw std::runtime_error("World GPU font raw homogeneous position differs");
}
}
