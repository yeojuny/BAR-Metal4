/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSTextureHistory.h"
#include <stdexcept>

namespace rendering {
// CDynWater's external TEX 2D and SHADOW2D inputs only. Storage and sampler
// ownership/readability remain with the existing typed image resolvers.
struct DynamicWaterTextureChannels {
	std::array<uint32_t,4> channels{0,1,2,3}; // RGBA, ZERO=4, ONE=5
	bool incompleteDefault = false;
};
inline DynamicWaterTextureChannels DynamicWaterExternalChannels(
	const NativeCUSTextureBinding& binding, bool shadow, bool depthImage)
{
	const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
	DynamicWaterTextureChannels result;
	if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject){
		// ARB_fragment_program 3.11.6: incomplete input returns (0,0,0,1).
		// This is not a substituted image. A storage write clears the observation.
		require(!shadow && !binding.object && binding.defaultStorageKnownEmpty,
			"Dynamic hover object zero lacks observed incomplete storage");
		const auto& p=binding.defaultSampling;
		// The actual hover program dwAddSplash.fp has no shadow option:
		// its TEX treats comparison mode as NONE (ARB_fragment_program_shadow).
		require(p.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},
			"Dynamic incomplete hover swizzle requires exact source semantics");
		result.incompleteDefault=true;
		return result;
	}
	require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject && binding.object &&
		binding.object->target==NativeCUSTextureTarget::Image2D,
		"Dynamic external sample lacks its immutable 2D binding");
	const auto& p=binding.object->sampling;
	if(shadow){
		require(depthImage && p.compareMode==0x884E && p.compareFunction==0x0203,
			"Dynamic SHADOW2D lacks its actual LEQUAL depth comparison sampler");
		// Legacy depth mode precedes swizzle (ARB_texture_swizzle issue 4).
		switch(p.depthMode){
			case 0x1909:result.channels={0,0,0,5};break; // LUMINANCE
			case 0x1906:result.channels={4,4,4,0};break; // ALPHA
			case 0x8049:result.channels={0,0,0,0};break; // INTENSITY
			default:throw std::runtime_error("Dynamic shadow legacy depth mode is unsupported");
		}
	}else require(!depthImage && p.compareMode==0,
		"Dynamic color TEX received a depth/comparison image");
	const auto before=result.channels;
	for(unsigned i=0;i<4;++i){
		switch(p.swizzle[i]){
			case 0x1903:result.channels[i]=before[0];break;
			case 0x1904:result.channels[i]=before[1];break;
			case 0x1905:result.channels[i]=before[2];break;
			case 0x1906:result.channels[i]=before[3];break;
			case 0:result.channels[i]=4;break;
			case 1:result.channels[i]=5;break;
			default:throw std::runtime_error("Dynamic texture swizzle component is invalid");
		}
	}
	return result;
}
} // namespace rendering
