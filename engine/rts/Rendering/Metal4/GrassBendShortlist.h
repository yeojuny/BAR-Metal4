/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "GrassZeroScaleCompaction.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

namespace rendering::metal4 {
constexpr std::size_t GrassBendMaximum=128;

struct GrassBendBounds {
 std::array<float,2> minimum{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()};
 std::array<float,2> maximum{-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
 bool supported=true, empty=true;
};

inline std::array<float,4> GrassBendRow(std::span<const uint8_t> bytes,std::size_t row)
{
 std::array<float,4> value;
 std::memcpy(value.data(),bytes.data()+row*sizeof(GrassInstanceRow),sizeof(value));
 return value;
}

inline GrassBendBounds GrassBendInstanceBounds(std::span<const uint8_t> bytes,GrassInstanceRange range)
{
 if(bytes.size()%sizeof(GrassInstanceRow))return {.supported=false};
 const auto rows=bytes.size()/sizeof(GrassInstanceRow);
 if(range.base>rows||range.count>rows-range.base)return {.supported=false};
 GrassBendBounds result;
 for(std::size_t i=range.base;i<range.base+range.count;++i){
  const auto row=GrassBendRow(bytes,i);
  // The original vertex shader always returns before bending for either zero.
  if((std::bit_cast<uint32_t>(row[3])&0x7fffffffU)==0)continue;
  if(!std::isfinite(row[1])||!std::isfinite(row[3])||
     !(std::abs(row[0])<=0x1p30f)||!(std::abs(row[2])<=0x1p30f))return {.supported=false};
  result.empty=false;
  for(std::size_t axis=0;axis<2;++axis){
   const auto coordinate=row[axis*2];
   result.minimum[axis]=std::min(result.minimum[axis],coordinate);
   result.maximum[axis]=std::max(result.maximum[axis],coordinate);
  }
 }
 return result;
}

struct GrassBendSelection {
 std::array<uint8_t,GrassBendMaximum> ordinals{};
 std::size_t count=0;
 bool supported=false;
};

inline GrassBendSelection GrassBendSelect(const GrassBendBounds& bounds,std::span<const uint8_t> rows)
{
 GrassBendSelection selected;
 if(!bounds.supported||rows.size()%sizeof(GrassInstanceRow)||rows.size()/sizeof(GrassInstanceRow)>GrassBendMaximum)
  return selected;
 selected.supported=true;
 for(std::size_t i=0;i<rows.size()/sizeof(GrassInstanceRow);++i){
  const auto row=GrassBendRow(rows,i);
  if(!(std::abs(row[0])<=0x1p30f)||!(std::abs(row[1])<=0x1p30f)||
     !(std::abs(row[2])<=0x1p30f)||!std::isfinite(row[3])){
   selected.supported=false;
   return selected;
  }
  if(bounds.empty)continue;
  const auto distance=[](double lower,double upper,double center){
   return std::max({lower-center,center-upper,0.0});
  };
  const double dx=distance(bounds.minimum[0],bounds.maximum[0],row[0]);
  const double dz=distance(bounds.minimum[1],bounds.maximum[1],row[1]);
  const double radius=row[2];
  // For eligible finite coordinates, the strict float32 shader calculation
  // cannot cross this outward margin through rounded subtraction, square,
  // sum/FMA or radius-square. Contributions also require distSq > .0001f,
  // so any possible flushing of subnormal terms is below that domain.
  constexpr double margin=1.0+0x1p-18;
  if(dx*dx+dz*dz>radius*radius*margin)continue;
  selected.ordinals[selected.count++]=static_cast<uint8_t>(i);
 }
 return selected;
}
}
