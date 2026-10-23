/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "../Backend/RenderTypes.h"
#include "../Backend/ScreenDrawList.h"

namespace rendering {

// Bounded planar screen path with actual source textures/font coverage and
// ordered RGBA8 cached panels. Matrix projection belongs to the CPU recorder;
// arbitrary z projection is unsupported. All borrowed views remain immutable
// and alive through synchronous retirement and return.
struct ScreenRenderRequest {
	const ScreenDrawList* drawList = nullptr;
	uint32_t width = 128;
	uint32_t height = 96;
	ScreenColor clearColor{0, 0, 0, 0};
	uint32_t measuredFrames = 1;
	bool shaderValidation = true;
	std::string capturePath;
	std::span<const ScreenRenderTargetPass> targetPasses;
};

using ScreenRenderResult = RenderReadbackResult;

// Uses the same actual serialized Metal4 runtime as RenderMetal4Asset.
// Explicit pixel dimensions; z==0 required; indexed geometry/order/degeneracy
// retained; color interpolation smooth; exact separate color/alpha blend
// factors; signed bottom-left scissors clamped to the framebuffer. No window.
ScreenRenderResult RenderMetal4Screen(const ScreenRenderRequest& request);

} // namespace rendering
