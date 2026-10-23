/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeDeferredLightData.h"
#include "Rendering/Backend/NativeDeferredPostprocessEpoch.h"
#include "Rendering/Backend/WorldFrameData.h"

#include <functional>
#include <memory>

namespace rendering::metal4 {
struct DeferredLightImage {
	CUSSourceImage image;
	NativeDeferredPostprocessEpoch epoch;
};
struct DeferredLightTarget {
	MTLPixelFormat color = MTLPixelFormatInvalid;
	MTLPixelFormat depth = MTLPixelFormatInvalid;
	MTLPixelFormat stencil = MTLPixelFormatInvalid;
	uint32_t width = 0, height = 0, logicalWidth = 0, logicalHeight = 0;
	WorldCamera camera;
	unsigned samples = 1;
};
class PreparedDeferredLights {
public:
	~PreparedDeferredLights();
	size_t DrawCount() const;
	uint64_t SequenceAt(size_t) const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeDraw(size_t, id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedDeferredLights();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class DeferredLightsGPU;
};
class DeferredLightsGPU {
public:
	using ImageResolver = std::function<DeferredLightImage(RuntimeLease&, const NativeCUSTextureBinding&, unsigned samplerSlot)>;
	DeferredLightsGPU();
	~DeferredLightsGPU();
	// CreateShader verifies both actual source stages before Lua can activate it.
	std::shared_ptr<const NativeDeferredLightProgramConsumer> LinkOriginalStages(
		RuntimeLease&, const NativeDeferredLightProgramSource&, double& milliseconds);
	std::unique_ptr<PreparedDeferredLights> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeDeferredLightFrame>, const DeferredLightTarget&,
		const ImageResolver&, bool validation, double& milliseconds);
	void ClearResources();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
