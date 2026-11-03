/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeFontRecycle169.h"
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace rendering {
inline bool RetainedFont170Enabled() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_RETAINED_FONT170");return !value||value[0]!='0'||value[1]!='\0';}();
 return enabled;
}
// Moving a previously recycled large batch into a tiny retained list can
// increase long-lived capacity relative to169's size-only deep copy. Preserve
// the move gate when unused capacity exceeds one payload extent. The171
// oversized path keeps the existing size-only copy instead. This is NOT an RSS cap.
inline bool RetainedFontCapacity170(const std::vector<ScreenVertex>& v,const std::vector<ScreenTexCoord>& uv,const std::vector<uint32_t>& i) noexcept {
 size_t payload=0,capacity=0;
 if(!fontRecycle169::AddCapacity(v.size(),sizeof(ScreenVertex),payload)||!fontRecycle169::AddCapacity(uv.size(),sizeof(ScreenTexCoord),payload)||
  !fontRecycle169::AddCapacity(i.size(),sizeof(uint32_t),payload)||!fontRecycle169::AddCapacity(v.capacity(),sizeof(ScreenVertex),capacity)||
  !fontRecycle169::AddCapacity(uv.capacity(),sizeof(ScreenTexCoord),capacity)||!fontRecycle169::AddCapacity(i.capacity(),sizeof(uint32_t),capacity))return false;
 return payload&&capacity>=payload&&capacity-payload<=payload;
}
// Only positive, representable over-capacity input selects the171 size copy.
// Empty/invalid extents retain the prior path and its deferred validation.
inline bool RetainedFontOversized171(const std::vector<ScreenVertex>& v,const std::vector<ScreenTexCoord>& uv,const std::vector<uint32_t>& i) noexcept {
 size_t payload=0,capacity=0;
 if(!fontRecycle169::AddCapacity(v.size(),sizeof(ScreenVertex),payload)||!fontRecycle169::AddCapacity(uv.size(),sizeof(ScreenTexCoord),payload)||
  !fontRecycle169::AddCapacity(i.size(),sizeof(uint32_t),payload)||!fontRecycle169::AddCapacity(v.capacity(),sizeof(ScreenVertex),capacity)||
  !fontRecycle169::AddCapacity(uv.capacity(),sizeof(ScreenTexCoord),capacity)||!fontRecycle169::AddCapacity(i.capacity(),sizeof(uint32_t),capacity))return false;
 return payload&&capacity>=payload&&capacity-payload>payload;
}
// Only the recorder owns mutable access before this object's construction.
// Construction/compilation performs NO screen validation: world rendering and
// exceptional screen fallback have their own, different original contracts.
class NativeRetainedFont170 {
public:
 using Ref=std::shared_ptr<const NativeRetainedFont170>;
 NativeRetainedFont170(std::vector<ScreenVertex>&& v,std::vector<ScreenTexCoord>&& uv,std::vector<uint32_t>&& indices) noexcept:
  source(std::move(v),std::move(uv),std::move(indices)) {}
 // Copy the same three vector sizes once as the old compiled Source capture.
 // The original producer keeps its capacity for its normal169 pool return.
 explicit NativeRetainedFont170(const fontRecycle169::Source& input):source(input) {}
 NativeRetainedFont170(const NativeRetainedFont170&)=delete;
 NativeRetainedFont170& operator=(const NativeRetainedFont170&)=delete;
 const auto& Vertices()const noexcept{return source.streams.vertices;}
 const auto& UV()const noexcept{return source.streams.uv;}
 const auto& Indices()const noexcept{return source.streams.indices;}
 // Recorder-thread memoization only. No GPU packet is published until screen
 // validation completes; afterwards these fields never change. CPU source data
 // is always immutable. World consumers do not access this certificate.
 bool CaptureBounds(std::array<double,3>& output)const {
  if(!boundsReady){
   bounds={};boundsFinite=true;
   for(const auto& v:Vertices()){
    const float p[]{v.x,v.y,v.z};
    for(unsigned k=0;k<3;++k){if(!std::isfinite(p[k])){boundsFinite=false;break;}bounds[k]=std::max(bounds[k],std::abs(double(p[k])));}
    if(!boundsFinite)break;
   }
   boundsReady=true;
  }
  output=bounds;return boundsFinite;
 }
 void ValidateScreen()const {
  if(screenValidated)return;
  if(Vertices().size()!=UV().size()||Indices().size()%3)throw std::invalid_argument("ScreenDrawList triangle/UV extent mismatch");
  for(const auto& v:Vertices()){
   for(float x:{v.x,v.y,v.z})if(!std::isfinite(x))throw std::invalid_argument("Nonfinite retained screen font position");
   for(float x:v.color)if(!std::isfinite(x))throw std::invalid_argument("Nonfinite retained screen font color");
  }
  for(const auto& uv:UV())for(float x:uv)if(!std::isfinite(x))throw std::invalid_argument("Nonfinite retained screen font UV");
  for(uint32_t index:Indices())if(index>=Vertices().size())throw std::invalid_argument("ScreenDrawList local index exceeds vertices");
  if(Vertices().size()>UINT32_MAX||Indices().size()>UINT32_MAX)throw std::length_error("ScreenDrawList exceeds uint32 index range");
  std::array<double,3> unused;CaptureBounds(unused);screenValidated=true;
 }
 bool ScreenValidated()const noexcept{return screenValidated;}
 const auto& ScreenBounds()const noexcept{return bounds;}
 size_t PayloadBytes()const noexcept{return Vertices().size()*sizeof(ScreenVertex)+UV().size()*sizeof(ScreenTexCoord)+Indices().size()*sizeof(uint32_t);}
private:
 fontRecycle169::Source source;
 mutable std::array<double,3> bounds{};
 mutable bool boundsReady=false,boundsFinite=false,screenValidated=false;
};
// Checked aggregate equivalent to the old flattened frame, counting EACH
// replay, not unique owners. Run before any GPU allocation/command preparation.
inline void AddRetainedFontExtent170(size_t vertices,size_t indices,size_t& totalVertices,size_t& totalIndices) {
 constexpr size_t VertexLimit=2'000'000,IndexLimit=12'000'000;
 if(totalVertices>VertexLimit||vertices>VertexLimit-totalVertices||totalIndices>IndexLimit||indices>IndexLimit-totalIndices)
  throw std::length_error("Native UI geometry/UV extent invalid");
 totalVertices+=vertices;totalIndices+=indices;
}
} // namespace rendering
