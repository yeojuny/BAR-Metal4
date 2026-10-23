/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePanelShaderData.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Require(bool value, const char* reason)
{
	if (!value) throw std::invalid_argument(reason);
}
template<typename T> bool Finite(const T& values)
{
	return std::all_of(values.begin(),values.end(),[](float value){return std::isfinite(value);});
}
}

void ValidateNativePanelShaderDraw(const NativePanelShaderDraw& draw)
{
	Require(draw.kind<=NativePanelShaderKind::Pause,"Unknown original panel shader kind");
	Require(draw.targetWidth && draw.targetHeight,"Original panel shader has no actual target extent");
	Require(!draw.surface.attachmentClear,"Panel shader cannot replace an attachment clear");
	Require(Finite(draw.modelViewProjection) && Finite(draw.textureMatrix) && Finite(draw.rect) && Finite(draw.uv) &&
		Finite(draw.viewSize) && Finite(draw.tint),"Nonfinite original panel shader inputs");
	Require(std::isfinite(draw.ivsx) && std::isfinite(draw.ivsy) && std::isfinite(draw.blurScale) &&
		std::isfinite(draw.tileSize) && std::isfinite(draw.threshold) && std::isfinite(draw.alpha) &&
		std::isfinite(draw.vignette),"Nonfinite original panel shader uniforms");
	for (const auto& corner:draw.corners) Require(Finite(corner),"Nonfinite original panel corner upload");
	for (auto index:draw.indices) Require(index<draw.corners.size(),"Original panel index exceeds its corner upload");
	for (const auto& tile:draw.tiles) Require(Finite(tile),"Nonfinite original panel tile upload");
	const unsigned imageCount=draw.kind==NativePanelShaderKind::GUIBlur?3:1;
	for (unsigned i=0;i<imageCount;++i) {
		const auto& image=draw.images[i];
		Require(bool(image),"Original panel shader lacks its immutable explicit sampler binding");
		ValidateScreenSampler(draw.samplers[i]);
		// Texture zero is an actual incomplete-texture input, never a white image.
		Require(image->incomplete || (image->width && image->height),"Original panel image has no allocation extent");
		Require(image->format!=ScreenTextureFormat::Depth32Float && image->format!=ScreenTextureFormat::Depth24Unorm &&
			image->format!=ScreenTextureFormat::Depth16Unorm,"Original panel color sampler cannot consume a depth image");
	}
	if (draw.kind==NativePanelShaderKind::GUIBlur) {
		Require(draw.viewSize[0]>0 && draw.viewSize[1]>0 && draw.tileSize>0,"Original GUI blur has invalid view/tile size");
		Require(draw.instanceCount<=draw.tiles.size(),"Original GUI blur instance range exceeds its frozen tile upload");
		Require(draw.toScreen==0 || draw.toScreen==1,"Original GUI blur toScreen uniform is invalid");
		Require(!draw.intelGPU || !draw.fineDerivatives,"Intel fallback and fine-derivative programs are distinct original variants");
	} else {
		Require(draw.instanceCount==1,"Original mask/Pause draw must retain its single quad");
		if (draw.kind==NativePanelShaderKind::GUIMask)
			Require(draw.blockSize>0,"Original tile mask reduction needs a positive block size");
	}
}
} // namespace rendering
