/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4WorldViewport.h"
namespace rendering::metal4 {
// The source view is in original GL logical coordinates. The same-frame
// terrain-only depth image is an unresampled copy of its physical crop.
struct GroundDecalDepthMapping {
 std::array<uint32_t,4> crop;
 std::array<float,4> fragmentToLogical;
 std::array<float,4> logicalOrigin;
 bool legacyExact;
};
inline std::optional<GroundDecalDepthMapping> MatchGroundDecalDepthMapping(
 const std::array<int32_t,4>& view,const std::array<uint32_t,2>& logical,const std::array<uint32_t,2>& render,
 const std::array<uint32_t,4>& actualCrop)
{
 if(!logical[0]||!logical[1]||!render[0]||!render[1]||view[2]<=0||view[3]<=0)return {};
 const auto mapped=MapWorldViewport(view,logical[0],logical[1],render[0],render[1]);
 if(!mapped||mapped->scissor!=actualCrop)return {};
 // Legacy path exactly matches the original unoffset bottom-left texel
 // convention only for unit-scale, uncropped, zero-origin original views.
 const bool legacy=logical==render&&view[0]==0&&view[1]==0&&
  actualCrop[0]==0&&actualCrop[1]+actualCrop[3]==render[1]&&
  actualCrop[2]==uint32_t(view[2])&&actualCrop[3]==uint32_t(view[3]);
 return GroundDecalDepthMapping{actualCrop,{float(logical[0])/render[0],-float(logical[1])/render[1],0,float(logical[1])},
  {float(view[0]),float(view[1]),0,0},legacy};
}
}
