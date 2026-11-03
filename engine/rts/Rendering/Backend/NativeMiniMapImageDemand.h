/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePiPData.h"

namespace rendering {
using NativeMiniMapImageMask = std::uint8_t;
namespace MiniMapImage {
inline constexpr NativeMiniMapImageMask Height=1, Shading=2, GroundLOS=4, Radar=8, All=15;
}
constexpr NativeMiniMapImageMask NativeMiniMapImageClosure(NativeMiniMapImageMask requested)
{
	if(requested & MiniMapImage::Shading) requested |= MiniMapImage::Height;
	if(requested & MiniMapImage::Radar) requested |= MiniMapImage::GroundLOS;
	return requested;
}
constexpr NativeMiniMapImageMask NativePiPImageDemand(NativePiPEngineImage image)
{
	switch(image) {
		case NativePiPEngineImage::Height: return MiniMapImage::Height;
		case NativePiPEngineImage::Shading: return MiniMapImage::Shading;
		case NativePiPEngineImage::GroundLOS: return MiniMapImage::GroundLOS;
		case NativePiPEngineImage::Radar: return MiniMapImage::Radar;
		case NativePiPEngineImage::None: return 0;
	}
	return 0;
}
// Select only linked active samplers and their actual bound images. Uniform
// multipliers and branches never remove an original shader image dependency.
// Resolve receives the original texture unit and the logical sampler slot.
template<class Resolve>
NativeMiniMapImageMask NativePiPImageDemand(const NativePiPProgramSnapshot& program,Resolve&& resolve)
{
	NativeMiniMapImageMask requested=0;
	for(const auto& parameter:GetNativePiPProgramSpec(program.program).uniforms) {
		if(!parameter.sampler || !parameter.active) continue;
		requested |= NativePiPImageDemand(resolve(program.samplerUnits[parameter.slot],parameter.slot));
	}
	return NativeMiniMapImageClosure(requested);
}
}
