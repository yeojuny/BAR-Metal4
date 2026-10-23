/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUITextureQualifiers.h"
#include "Rendering/Backend/ScreenDrawList.h"

namespace rendering::ui {
struct NativeNamedImagePolicy {
	ScreenSampler sampler;
	bool generateMipmaps = false;
	bool bitmapOperations = true;
};
// Source boundary: NamedTextures::Load -> CBitmap::Create[ DDS ]Texture ->
// TextureCreationParams::GetMinFilter. DDS passes the EXTRA embedded mip count,
// not reqNumLevels; this intentionally differs from the ordinary bitmap path.
inline NativeNamedImagePolicy GetNativeNamedImagePolicy(const NativeUITextureQualifiers& q,
	bool dds, std::uint32_t width, std::uint32_t height, std::uint32_t extraDDSLevels,
	std::uint32_t actualMaximumAnisotropy)
{
	if (!width || !height) throw std::runtime_error("Original named image dimensions are empty");
	NativeNamedImagePolicy result;
	auto& sampler = result.sampler;
	sampler.filter = q.nearest ? ScreenFilter::Nearest : ScreenFilter::Linear;
	sampler.wrapS = sampler.wrapT = (!dds && q.clamped) ? ScreenWrap::ClampEdge : ScreenWrap::Repeat;
	sampler.whiteBorder = !dds && q.whiteBorder;
	if (q.anisotropic) {
		if (actualMaximumAnisotropy < 1 || actualMaximumAnisotropy > 16)
			throw std::runtime_error("Original anisotropic image requires the actual supported native sampler limit");
		sampler.anisotropy = actualMaximumAnisotropy;
	}
	std::uint32_t numberOfLevels = 1;
	if (dds) {
		numberOfLevels = extraDDSLevels;
		result.bitmapOperations = false;
		// CreateDDSTexture changes MIN_FILTER only; a new DDS texture keeps
		// the original default LINEAR magnification, including :n: images.
		sampler.magnificationFilter = ScreenFilter::Linear;
		result.generateMipmaps = extraDDSLevels == 0;
	} else {
		const bool needMipmaps = !(q.nearest || q.linear) || q.mipNear;
		if (needMipmaps) {
			for (auto side = std::max(width,height); side > 1; side >>= 1) ++numberOfLevels;
		}
		result.generateMipmaps = numberOfLevels > 1;
	}
	if (numberOfLevels != 1)
		sampler.mipFilter = q.nearest || q.mipNear ? ScreenMipFilter::Nearest : ScreenMipFilter::Linear;
	ValidateScreenSampler(sampler);
	return result;
}
// The native uploader now supplies legal RGBA8 transport for generated BC
// levels. Sampling and upload requests follow the original named-image policy
// for every format; storage format must not change wrap or mip selection.
inline NativeNamedImagePolicy GetNativeNamedImageUploadPolicy(const NativeUITextureQualifiers& q,
	ScreenTextureFormat /*format*/, bool dds, std::uint32_t width, std::uint32_t height,
	std::uint32_t extraDDSLevels, std::uint32_t actualMaximumAnisotropy)
{
	return GetNativeNamedImagePolicy(q,dds,width,height,extraDDSLevels,actualMaximumAnisotropy);
}
} // namespace rendering::ui
