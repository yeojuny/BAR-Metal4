/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace rendering {
// OpenGL 2.1 sections 3.4.2/3.4.4: MSAA uses the unrounded-width rectangle
// perpendicular to the segment, with one-pixel-long stipple rectangles.
// The native target's rasterizer evaluates these edges at its actual sample
// positions. Never substitute 1x diamond coverage or full-pixel masks here.
template<typename Emit>void NativeMultisampleLineIntervals(double length,bool stippled,
	uint16_t pattern,int factor,uint64_t& counter,Emit emit)
{
	if(!std::isfinite(length)||length<0||length>1e7)throw std::runtime_error("Native multisample line extent invalid");
	if(length==0)return;
	const auto rectangles=uint64_t(std::ceil(length));
	if(!stippled){emit(0.,1.);counter+=rectangles;return;}
	const auto repeat=uint64_t(std::clamp(factor,1,256));
	for(uint64_t n=0;n<rectangles;++n){
		if((pattern>>(((counter+n)/repeat)%16))&1)
			emit(double(n)/length,std::min(double(n+1),length)/length);
	}
	counter+=rectangles;
}
}
