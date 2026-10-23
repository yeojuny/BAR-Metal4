/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "ScreenDrawList.h"
#include <cmath>
#include <stdexcept>
namespace rendering {
inline std::weak_ptr<const void>& NativeDecalCaptureOwner(){static std::weak_ptr<const void> owner;return owner;}
struct NativeDecalCaptureUniforms {
	std::array<float,16> viewProjection{};
	std::array<float,4> originExtent{0,0,1,1};
	std::array<float,4> mapFeather{1,1,.85f,0};
};
static_assert(sizeof(NativeDecalCaptureUniforms)==96);
struct NativeDecalCaptureInput {
	NativeDecalCaptureUniforms uniforms;
	std::shared_ptr<const void> compiled;
	std::array<ScreenSecondaryTexture,2> images;
};
inline void ValidateNativeDecalCapture(const NativeDecalCaptureInput& input){
	if(!input.compiled)throw std::runtime_error("Decal Capture actual compiled program missing");
	for(const auto& image:input.images){
		if(!image.texture || image.texture->incomplete || image.uvMode!=ScreenUVMode::Normalized ||
			image.texture->format==ScreenTextureFormat::Depth16Unorm || image.texture->format==ScreenTextureFormat::Depth24Unorm || image.texture->format==ScreenTextureFormat::Depth32Float)
			throw std::runtime_error("Decal Capture requires its original complete color/height images");
		ValidateScreenSampler(image.sampler);
	}
	for(float v:input.uniforms.viewProjection)if(!std::isfinite(v))throw std::runtime_error("Decal Capture nonfinite view projection");
	for(float v:input.uniforms.originExtent)if(!std::isfinite(v))throw std::runtime_error("Decal Capture nonfinite world extent");
	for(float v:input.uniforms.mapFeather)if(!std::isfinite(v))throw std::runtime_error("Decal Capture nonfinite map/feather");
}
} // namespace rendering
