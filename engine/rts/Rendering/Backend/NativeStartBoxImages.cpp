/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeStartBoxImages.h"
#include <stdexcept>
namespace rendering {
NativeStartBoxImageKind ClassifyNativeStartBoxImage(const NativeCUSTextureBinding& binding)
{
	const auto require=[](bool value,const char* message){if(!value)throw std::logic_error(message);};
	if(binding.knowledge==NativeCUSBindingKnowledge::DefaultObject){
		require(!binding.object && binding.defaultStorageKnownEmpty,"StartBox default sampler storage was not observed");
		return NativeStartBoxImageKind::Empty;
	}
	require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject && bool(binding.object),"StartBox source texture binding is missing");
	const auto& object=*binding.object;
	require(object.identity && object.contextGeneration && object.version && object.target==NativeCUSTextureTarget::Image2D,"StartBox source texture identity or target differs");
	if(object.storage==NativeTextureStorageKnowledge::Empty)return NativeStartBoxImageKind::Empty;
	require(object.storage==NativeTextureStorageKnowledge::AllocatedUndefined || object.storage==NativeTextureStorageKnowledge::Published,"StartBox image storage is unobserved");
	const auto& allocation=object.allocation;
	if(!allocation.width || !allocation.height)return NativeStartBoxImageKind::Empty;
	require(allocation.depth==1 && allocation.samples==1 && allocation.levels,"StartBox sampler2D allocation differs");
	const auto& sampling=object.sampling;
	require(sampling.compareMode==0,"StartBox original sampler2D cannot consume an unimplemented shadow comparison");
	switch(allocation.originalInternalFormat){
		case 0x81A5:case 0x81A6:case 0x81A7:case 0x8CAC: // D16/D24/D32/D32F
			require(sampling.depthMode==0x1909 && sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"StartBox depth sampling requires the actual luminance/identity source state");
			return NativeStartBoxImageKind::Depth;
		// OpenAI Codex: retain the original unsized atlas allocation and the
		// existing UI/HDR/BC float-sampled consumers; do not rewrite storage.
		case 0x1908: // GL_RGBA: AtlasOnDemand keeps this original allocation token
		case 0x8C3A: // R11F_G11F_B10F: existing packed HDR target
		case 0x8DBB:case 0x8DBD:case 0x8E8C:case 0x8E8D: // BC4/BC5/BC7/BC7 sRGB
		case 0x8051:case 0x8058:case 0x881A:case 0x8814: // RGB8/RGBA8/RGBA16F/RGBA32F
		case 0x8229:case 0x822B:case 0x822D:case 0x822E:case 0x822F:case 0x8230: // R8/RG8/R16F/R32F/RG16F/RG32F
		case 0x83F1:case 0x83F2:case 0x83F3: // Original named BC1/2/3
			return NativeStartBoxImageKind::Color;
		default:throw std::logic_error("StartBox source allocation format needs its actual sampled-image consumer");
	}
}
}
