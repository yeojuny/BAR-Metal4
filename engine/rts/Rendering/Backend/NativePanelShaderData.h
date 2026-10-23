/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "ScreenDrawList.h"

namespace rendering {

// Family-local replacement for the exact GUI Shader and Pause Screen programs.
// No Lua object, GL name, mutable binding or native GPU handle crosses this seam.
enum class NativePanelShaderKind : std::uint8_t { GUIBlur, GUIMask, Pause };

struct NativePanelShaderDraw {
	NativePanelShaderKind kind = NativePanelShaderKind::GUIBlur;
	// Explicit GLSL bindings, independent of fixed-function texture enables:
	// 0 = tex0/source/screencopy, 1 = tileMask, 2 = tex2 (stencil alpha).
	std::array<std::shared_ptr<const ScreenTexture>,3> images;
	std::array<ScreenSampler,3> samplers;
	// Owned copies of the actual corner/index/instance uploads at DrawElements.
	std::array<std::array<float,2>,4> corners{{{0,0},{1,0},{0,1},{1,1}}};
	std::array<std::uint32_t,6> indices{0,1,2,2,1,3};
	std::vector<std::array<float,2>> tiles;
	std::uint32_t instanceCount = 1;
	// Original TexRect arguments and its resolved default/explicit UV endpoints.
	// Pause uses the original quad triangulation, independently of the tile VAO.
	std::array<float,4> rect{};
	std::array<float,4> uv{0,1,1,0};
	float ivsx = 0, ivsy = 0, blurScale = 1, tileSize = 32;
	std::array<float,2> viewSize{};
	std::int32_t toScreen = 1, blockSize = 8;
	float threshold = 0.01f, alpha = 0, vignette = 0;
	bool intelGPU = false, intelNormalized = true, fineDerivatives = false;
	bool perceivedLuma = false, smoothTone = false, tintEnabled = false;
	std::array<float,3> tint{1,1,1};
	// Filled at actual recorder replay, including a RenderToTexture callback.
	ScreenDrawState surface;
	std::array<float,16> modelViewProjection{};
	std::array<float,16> textureMatrix{};
	std::uint32_t targetWidth = 0, targetHeight = 0;
};

void ValidateNativePanelShaderDraw(const NativePanelShaderDraw& draw);

} // namespace rendering
