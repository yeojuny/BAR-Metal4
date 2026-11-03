/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDepthStencilState.h"

namespace rendering {
// Descriptor identity only. Dynamic front/back stencil references remain on
// the source draw and are always bound by the encoder for that draw.
inline ScreenDepthStencilState NativeEffectiveDepthStencilKey(
	const ScreenDepthStencilState& source, bool hasDepth, bool hasStencil)
{
	ScreenDepthStencilState key;
	key.depthTest = true;
	key.depthCompare = hasDepth && source.depthTest ? source.depthCompare : ScreenAlphaCompare::Always;
	key.depthWrite = hasDepth && source.depthTest && source.depthWrite;
	key.stencilTest = hasStencil && source.stencilTest;
	if (key.stencilTest) {
		key.frontStencil = source.frontStencil;
		key.backStencil = source.backStencil;
		key.frontStencil.reference = key.backStencil.reference = 0;
	}
	return key;
}
} // namespace rendering
