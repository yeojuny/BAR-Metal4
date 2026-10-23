/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#ifndef __OBJC__
#error "Metal4PanelShader.h is private to Objective-C++ native rendering"
#endif
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativePanelShaderData.h"

namespace rendering::metal4 {

class PreparedPanelShaderDraw {
public:
	~PreparedPanelShaderDraw();
	// The existing ScreenGPU command applies its depth/stencil, viewport,
	// scissor and cull state and checks retained image readability first.
	void Encode(id<MTL4RenderCommandEncoder>) const;
private:
	PreparedPanelShaderDraw();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class PanelGPU;
};

class PanelGPU {
public:
	PanelGPU();
	~PanelGPU();
	std::unique_ptr<PreparedPanelShaderDraw> Prepare(RuntimeLease&, const NativePanelShaderDraw&,
		const std::array<id<MTLTexture>,3>&, const std::array<id<MTLSamplerState>,3>&,
		std::uint32_t actualTargetHeight, MTLPixelFormat colorFormat,
		bool validation, double& shaderMs, std::uint32_t samples=1);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace rendering::metal4
