/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ScreenNative.h"
#include "Rendering/Backend/NativePiPData.h"
#include <memory>

namespace rendering::metal4 {

// This leaf never opens a render pass or submits. It joins the original
// Screen/RTT command at its source offset and uses the caller's attachments.
class PreparedPiPDraw {
public:
	~PreparedPiPDraw();
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	void Encode(id<MTL4RenderCommandEncoder>) const;
	void DidComplete() const;
	bool Empty() const;
private:
	PreparedPiPDraw();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class PiPGPU;
};

class PiPGPU {
public:
	PiPGPU();
	~PiPGPU();
	// Before preparing the root screen frame and all its nested targets.
	// The preceding serialized submission must already have retired.
	void BeginFrame();
	void ClearResources();
	std::unique_ptr<PreparedPiPDraw> Prepare(RuntimeLease&, const NativePiPDraw&,
		ScreenGPU& sharedImages, const PreparedScreenFrame* producers,
		MTLViewport viewport, MTLScissorRect scissor,
		MTLPixelFormat colorFormat, MTLPixelFormat depthFormat, MTLPixelFormat stencilFormat,
		bool validation, double& shaderMs,
		std::optional<NativePostprocessConsumer> consumer = std::nullopt,
		const PiPSourceImageProvider& sourceImages = {}, std::uint32_t samples=1,
		std::uint32_t targetWidth=0,std::uint32_t targetHeight=0);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace rendering::metal4
