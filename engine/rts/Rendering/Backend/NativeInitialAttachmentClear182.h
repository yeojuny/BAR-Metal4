/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
#include <bit>
#include <optional>

namespace rendering {
// An entire original clear event can move to the first attachment load only
// when every effective write is full. Partial channels/scissor/stencil retain
// their original raster clear. Formats and the FIRST source position are
// checked separately by the prepared native consumer.
struct NativeInitialAttachmentClear182 {
	unsigned colors=0;
	bool depth=false,stencil=false;
	std::array<float,4> color{};
	float depthValue=1;
	std::uint32_t stencilValue=0;
};
inline std::optional<NativeInitialAttachmentClear182> PlanNativeInitialAttachmentClear182(
	const NativeWorldAttachmentClear& clear,std::uint32_t width,std::uint32_t height,
	unsigned colorCount,bool combined,bool hasStencil) noexcept
{
	if(!clear.sequence||!width||!height||(colorCount!=1&&colorCount!=5)||(combined&&colorCount!=5))return {};
	const unsigned mask=unsigned(clear.mask);
	if(mask&~unsigned(ScreenAttachmentAll))return {};
	if(clear.scissor){const auto& s=*clear.scissor;
		if(s.x>0||s.y>0||std::int64_t(s.x)+s.width<width||std::int64_t(s.y)+s.height<height)return {};
	}
	const auto binary=[](float value){const auto bits=std::bit_cast<std::uint32_t>(value);return bits==0||bits==0x3f800000u;};
	NativeInitialAttachmentClear182 out;out.color=clear.values.color;
	const auto colorMask=[](const auto& channels){unsigned bits=0;for(unsigned n=0;n<4;++n)if(channels[n])bits|=1u<<n;return bits;};
	if(mask&ScreenAttachmentColor){
		const unsigned ordinary=colorMask(clear.colorWrite),extra=combined?colorMask(clear.combinedColorWrite):0;
		if((ordinary&&ordinary!=15)||(extra&&extra!=15))return {};
		if(ordinary)out.colors=(1u<<colorCount)-1;
		if(extra)out.colors|=32u;
		// Match the accepted SSAO150 policy: do not assume hardware and shader
		// quantization agree for arbitrary float clear values (including -0).
		if(out.colors)for(float value:out.color)if(!binary(value))return {};
	}
	if((mask&ScreenAttachmentDepth)&&clear.depthWrite){
		if(!binary(clear.values.depth))return {};
		out.depth=true;out.depthValue=clear.values.depth;
	}
	if((mask&ScreenAttachmentStencil)&&hasStencil){
		const unsigned write=clear.frontStencilWriteMask&255u;
		if(write&&write!=255u)return {};
		if(write){out.stencil=true;out.stencilValue=clear.values.stencil&255u;}
	}
	if(!out.colors&&!out.depth&&!out.stencil)return {};
	return out;
}
} // namespace rendering
