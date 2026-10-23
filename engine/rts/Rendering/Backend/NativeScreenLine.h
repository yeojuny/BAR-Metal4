/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
namespace rendering {
struct ScreenLine {
	ScreenVertex start,end;float width=1;
	bool stippled=false;std::uint16_t pattern=0xffff;int factor=1;bool resetStipple=true;
	// Polygon wireframe edges retain their boundary raster convention.
	bool polygonEdge=false;
};
// Endpoints/widths are actual raster pixels, not logical/Retina units. Supports
// non-smooth GL diamond-exit coverage and ordered16-bit stipple counters.
// At 1x width rounds to integer (1..64). At N>1 emit exact-width rectangles
// for actual hardware sample coverage. No textured or perspective lines here.
// The destination list must be consumed at this exact pixel coordinate basis.
void AppendNativeScreenLines(ScreenDrawList&,std::span<const ScreenLine>,const ScreenDrawState&,
	std::uint32_t coordinateWidth,std::uint32_t coordinateHeight,unsigned samples=1);
}
