/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
// Original per-draw fixed-function input. No projected CPU positions, pixel
// spans, expanded triangles or GPU output geometry is retained here.
struct NativeSourceLineTransform133 {
 std::array<float,16> modelView{},projection{},texture{};
 std::array<float,4> clipPlane{};
 std::array<uint32_t,4> flags{}; // clipZeroToOne, depthClamp, clipPlane, textureMatrix
};
struct NativeSourceLineSegment133 {
 uint32_t a=0,b=0,transform=0,head=1;
 float width=1;uint32_t pattern=0xffff,factor=1,stipple=0;
};
struct NativeSourceLineInput133 {
 std::vector<NativeWorldDrawVertex> vertices;
 std::vector<NativeSourceLineSegment133> segments;
 std::vector<NativeSourceLineTransform133> transforms;
 bool connectedStipple=false;
 uint32_t sourceKind=0; // world, screen primitive, explicit pixel line
};
static_assert(sizeof(NativeSourceLineTransform133)==224);
static_assert(sizeof(NativeSourceLineSegment133)==32);
namespace sourceLine133 {
inline void Check(bool ok,const char* text){if(!ok)throw std::invalid_argument(text);}
inline bool Material(const ScreenDrawState& s){return s.material==ScreenMaterial::Solid||s.material==ScreenMaterial::RGBA||s.material==ScreenMaterial::FontCoverage;}
inline std::array<float,16> Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
inline void ValidateTransform(const NativeSourceLineTransform133& d){
 for(const auto* m:{&d.modelView,&d.projection,&d.texture})for(float x:*m)Check(std::isfinite(x),"Source line matrix is nonfinite");
 for(float x:d.clipPlane)Check(std::isfinite(x),"Source line clip plane is nonfinite");
 for(auto f:d.flags)Check(f<=1,"Source line transform flags differ");
 // Existing fixed-function world texture support is affine; projective q
 // requires a separately owned original program and is not silently changed.
 Check(!d.flags[3]||(d.texture[3]==0&&d.texture[7]==0&&d.texture[15]==1),"Source line projective texture matrix is unsupported");
}
inline void Validate(const NativeSourceLineInput133& d){
 Check(d.sourceKind<=2,"Source line origin differs");
 Check(!d.vertices.empty()&&d.vertices.size()<=4u*1024u*1024u&&!d.segments.empty()&&d.segments.size()<=4u*1024u*1024u&&!d.transforms.empty(),"Source line input extent differs");
 for(const auto& t:d.transforms)ValidateTransform(t);
 for(const auto& v:d.vertices)for(const auto* a:{&v.position,&v.color,&v.uv})for(float x:*a)Check(std::isfinite(x),"Source line raw attribute is nonfinite");
 bool connected=false;
 for(size_t i=0;i<d.segments.size();++i){const auto& s=d.segments[i];Check(s.a<d.vertices.size()&&s.b<d.vertices.size()&&s.transform<d.transforms.size(),"Source line original index exceeds input");Check(s.head<=1&&s.stipple<=1&&(i||s.head)&&std::isfinite(s.width)&&s.width>0&&s.width<=64&&s.pattern<=0xffff&&s.factor>=1&&s.factor<=256,"Source line original raster state differs");connected|=!s.head&&bool(s.stipple);}
 Check(connected==d.connectedStipple,"Source line prefix ownership differs");
}
inline void AddSegment(NativeSourceLineInput133& d,uint32_t a,uint32_t b,uint32_t transform,bool head,float width,bool stipple,uint32_t pattern,uint32_t factor){
 d.connectedStipple|=!head&&stipple;d.segments.push_back({a,b,transform,uint32_t(head),width,pattern,factor,uint32_t(stipple)});
}
inline void Topology(NativeSourceLineInput133& d,NativeDrawPrimitive mode,const NativeWorldDrawState& s){
 const uint32_t n=d.vertices.size();Check(mode==NativeDrawPrimitive::Lines||mode==NativeDrawPrimitive::LineStrip||mode==NativeDrawPrimitive::LineLoop,"Source line topology differs");
 if(mode==NativeDrawPrimitive::Lines){for(uint32_t i=0;i+1<n;i+=2)AddSegment(d,i,i+1,0,true,s.lineWidth,s.lineStipple,s.stipplePattern,s.stippleFactor);}
 else {for(uint32_t i=0;i+1<n;++i)AddSegment(d,i,i+1,0,i==0,s.lineWidth,s.lineStipple,s.stipplePattern,s.stippleFactor);if(mode==NativeDrawPrimitive::LineLoop&&n>=2)AddSegment(d,n-1,0,0,false,s.lineWidth,s.lineStipple,s.stipplePattern,s.stippleFactor);}
}
inline void Append(NativeSourceLineInput133& to,NativeSourceLineInput133&& from){
 Check(to.vertices.size()+from.vertices.size()<=4u*1024u*1024u&&to.segments.size()+from.segments.size()<=4u*1024u*1024u,"Source line batch capacity exceeded");
 const uint32_t vertex=to.vertices.size(),transform=to.transforms.size();
 to.vertices.insert(to.vertices.end(),from.vertices.begin(),from.vertices.end());to.transforms.insert(to.transforms.end(),from.transforms.begin(),from.transforms.end());
 for(auto s:from.segments){s.a+=vertex;s.b+=vertex;s.transform+=transform;to.segments.push_back(s);}to.connectedStipple|=from.connectedStipple;
}
}
}
