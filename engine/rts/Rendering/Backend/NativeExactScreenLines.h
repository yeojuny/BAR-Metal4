/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeScreenGeometry.h"
namespace rendering {
inline std::shared_ptr<const NativeScreenGeometry> TryExactScreenLines(const NativeWorldDrawCommand& command,const std::array<double,4>& viewport,uint32_t rasterWidth,uint32_t rasterHeight,unsigned samples,uint32_t coordinateWidth,uint32_t coordinateHeight){
 // This path consumes the same orthographic world-shaped line input used by
 // AppendScreenRaster. Explicit ScreenLine/CPU mesh callers remain untouched.
 if(!coordinateWidth||!coordinateHeight)return {};
 for(const auto& v:command.vertices)if(v.position[3]!=1)return {};
 auto packet=exactWorldLine::TryBuild(command,viewport,rasterWidth,rasterHeight,samples);if(!packet)return {};
 auto result=std::make_shared<NativeScreenGeometry>();result->kind=NativeScreenGeometryKind::ScreenLines;
 result->vertices=exactScreenLine::Capture(packet->segments);result->instances=exactScreenLine::Capture(packet->spans);
 result->count=packet->quads*6;result->instanceCount=1;result->coordinateExtent={coordinateWidth,coordinateHeight};
 screenGeometry::Validate(*result);return result;
}
}
