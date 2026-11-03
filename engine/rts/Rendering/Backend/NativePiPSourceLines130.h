/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePiPData.h"
#include <optional>
#include <cmath>
namespace rendering::pipSource130 {
struct RasterUniform {std::array<float,4> viewport,target;std::array<uint32_t,4> options;};
static_assert(sizeof(RasterUniform)==48);
inline std::optional<RasterUniform> Admit(const NativePiPDraw& source,const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned samples){
 ValidateNativePiPDraw(source);
 if(source.program.program!=NativePiPProgram::Line||!source.vertexCount||(samples!=1&&samples!=4)||!width||!height||width>16384||height>16384)return {};
 if(source.surface.alphaTest.enabled||source.raster.inheritedClipPlane||(source.raster.lineStipple&&source.raster.stipplePattern!=0xffff))return {};
 if(source.raster.frontPolygonMode!=NativePolygonMode::Fill||source.raster.backPolygonMode!=NativePolygonMode::Fill)return {};
 if(!std::isfinite(source.raster.lineWidth)||source.raster.lineWidth<=0||source.raster.lineWidth>64)return {};
 for(double x:viewport)if(!std::isfinite(x)||std::abs(x)>32768)return {};
 if(viewport[2]<=0||viewport[3]<=0)return {};
 // Original source positions and their expressions are GPU work. No CPU
 // projection, clipped count, pixel span, output planning or minimum-size gate.
 RasterUniform out{};for(unsigned i=0;i<4;++i)out.viewport[i]=float(viewport[i]);out.target={float(width),float(height),source.raster.lineWidth,0};out.options={samples,source.firstVertex,0,0};return out;
}
}
