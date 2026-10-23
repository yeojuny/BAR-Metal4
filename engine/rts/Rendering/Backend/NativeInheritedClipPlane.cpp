/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeInheritedClipPlane.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace rendering {
std::optional<NativeInheritedClipPlaneSource> NativeInheritedWaterClipPlaneSource(NativeWorldDrawStage stage)
{
 switch(stage){case NativeWorldDrawStage::WaterReflection:return NativeInheritedClipPlaneSource{2,1};case NativeWorldDrawStage::WaterRefraction:return NativeInheritedClipPlaneSource{2,-1};case NativeWorldDrawStage::AlphaAboveWater:return NativeInheritedClipPlaneSource{3,1};case NativeWorldDrawStage::AlphaBelowWater:return NativeInheritedClipPlaneSource{3,-1};default:return {};}
}
void DisableNativeInheritedWaterClipPlane(NativeWorldDrawState&s,NativeWorldDrawStage stage,unsigned index){const auto source=NativeInheritedWaterClipPlaneSource(stage);if(source&&source->index==index)s.inheritedClipPlane.reset();}
std::optional<std::array<float,4>> NativeInheritedWaterClipPlane(NativeWorldDrawStage stage,
 const std::array<float,16>& p,bool zeroToOneDepth)
{
 const auto source=NativeInheritedWaterClipPlaneSource(stage);
 if(!source)return {};
 const double sign=source->sign;
 // Solve P^T * plane_clip = plane_eye without transforming source vertices
 // back to world space. Fixed-function generic Lua retains eye-space plane2/3.
 double a[4][5]{};
 for(unsigned row=0;row<4;++row){
  for(unsigned col=0;col<4;++col){a[row][col]=p[row*4+col];if(!std::isfinite(a[row][col]))throw std::runtime_error("Non-finite original water callback projection");}
  a[row][4]=row==1?sign:0;
 }
 for(unsigned col=0;col<4;++col){
  unsigned pivot=col;for(unsigned row=col+1;row<4;++row)if(std::abs(a[row][col])>std::abs(a[pivot][col]))pivot=row;
  if(a[pivot][col]==0)throw std::runtime_error("Singular original projection needs per-vertex inherited clip distances");
  if(pivot!=col)for(unsigned i=col;i<5;++i)std::swap(a[col][i],a[pivot][i]);
  const double scale=a[col][col];for(unsigned i=col;i<5;++i)a[col][i]/=scale;
  for(unsigned row=0;row<4;++row)if(row!=col){const double scale2=a[row][col];for(unsigned i=col;i<5;++i)a[row][i]-=scale2*a[col][i];}
 }
 std::array<float,4> result{};
 for(unsigned i=0;i<4;++i)result[i]=float(a[i][4]);
 // Recorder emits z_GL = 2*z_zero-w. Thus z_zero=(z_GL+w)/2.
 if(zeroToOneDepth){result[2]*=.5f;result[3]+=result[2];}
 for(float value:result)if(!std::isfinite(value))throw std::runtime_error("Original inherited clip-plane transform overflow");
 return result;
}
}
