/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cmath>
namespace rendering::metal4 {
// The target's finite extent is distinct from the source's virtual viewport.
// 2^19 pixel corner bound keeps 8-bit quantized endpoints <2^28, signed
// endpoint deltas <2^29, transformed diamond coordinates <2^30 and all
// 64-bit cross-products / doubled rational numerators strictly in range,
// with ample margin for float rounding and supported <=64px line width.
// Both corners must pass; a large size alone is not a texture allocation.
inline constexpr double WeaponRangeVirtualCornerLimit=524288.0;
struct WeaponRangeViewportContract {
 bool valid=false, rebaseFill=false, extended=false;
};
inline WeaponRangeViewportContract ClassifyWeaponRangeViewport(const std::array<double,4>& v,bool line) noexcept {
 for(double x:v)if(!std::isfinite(x))return {};
 if(v[2]<=0||v[3]<=0)return {};
 const double right=v[0]+v[2],bottom=v[1]+v[3];
 for(double x:{v[0],v[1],right,bottom})if(!std::isfinite(x)||std::abs(x)>WeaponRangeVirtualCornerLimit)return {};
 bool extended=false;for(double x:v)extended|=std::abs(x)>16384.0;
 return {true,extended&&!line,extended};
}
}
