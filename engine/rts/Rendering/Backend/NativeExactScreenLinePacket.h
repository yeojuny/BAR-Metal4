/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeExactWorldLine.h"
#include "NativeBufferData.h"
namespace rendering::exactScreenLine {
// Reuse the exact120 line kernel. The two existing typed geometry storage
// carriers hold immutable segments/spans; no ScreenDrawCommand/class ABI grows.
template<class T> NativeBufferSnapshotRef Capture(const std::vector<T>& input){
 NativeBufferLayout layout;layout.kind=NativeBufferKind::Storage;layout.elements=input.size();layout.stride=sizeof(T);
 NativeBufferAttribute attribute;attribute.id=0;attribute.scalarType=NativeScalarType::UInt8;attribute.declaredCount=sizeof(T);attribute.byteSize=sizeof(T);layout.attributes.push_back(attribute);
 NativeBufferSnapshotSource source;source.Define(layout);
 return source.Capture({reinterpret_cast<const uint8_t*>(input.data()),input.size()*sizeof(T)});
}
template<class T> T Read(const NativeBufferSnapshotRef& source,size_t index){T value;std::memcpy(&value,source->bytes.data()+index*sizeof(T),sizeof(T));return value;}
inline void Validate(const NativeBufferSnapshotRef& segments,const NativeBufferSnapshotRef& spans,uint32_t indices,uint32_t allowedFlags=7){
 const auto check=[](bool ok){if(!ok)throw std::invalid_argument("Exact screen line packet bounds differ");};
 const auto storage=[&](const auto& source,size_t stride){check(source&&source->object&&source->revision&&source->layout.kind==NativeBufferKind::Storage&&source->layout.stride==stride&&CheckedNativeBufferByteSize(source->layout)==source->bytes.size());};
 storage(segments,sizeof(exactWorldLine::Line120Segment));storage(spans,sizeof(exactWorldLine::Line120Span));
 check(indices&&indices%6==0&&indices/6<=1048576);
 uint64_t end=0;
 for(size_t i=0;i<spans->layout.elements;++i){const auto span=Read<exactWorldLine::Line120Span>(spans,i);check(span.first==end&&span.count&&span.segment<segments->layout.elements);end+=span.count;check(end<=indices/6);}
 check(end==indices/6);
 for(size_t i=0;i<segments->layout.elements;++i){const auto s=Read<exactWorldLine::Line120Segment>(segments,i);check(s.width&&s.height&&s.width<=16384&&s.height<=16384&&s.factor>=1&&s.factor<=256&&(s.flags&~allowedFlags)==0);
  check(s.a.position[3]==1&&s.b.position[3]==1);
  for(const auto* v:{&s.a,&s.b}){for(float x:v->position)check(std::isfinite(x));for(float x:v->color)check(std::isfinite(x));for(float x:v->uv)check(std::isfinite(x));}
  if(!(s.flags&1))check(s.count&&s.count<=64&&(s.direction==1||s.direction==-1));
 }
}
} // namespace rendering::exactScreenLine
