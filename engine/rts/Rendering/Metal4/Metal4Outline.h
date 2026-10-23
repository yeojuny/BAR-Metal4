/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4CUSOutlineInputs.h"
#include "Rendering/Backend/NativeOutlineData.h"
#include "Rendering/Backend/WorldFrameData.h"

#include <functional>
#include <memory>

namespace rendering::metal4 {
using OutlineImage = CUSOutlineImage;
struct OutlineTarget {
	MTLPixelFormat color = MTLPixelFormatInvalid;
	MTLPixelFormat depth = MTLPixelFormatInvalid;
	MTLPixelFormat stencil = MTLPixelFormatInvalid;
	uint32_t width = 0, height = 0, logicalWidth = 0, logicalHeight = 0;
	WorldCamera camera;
	unsigned samples = 1;
};
class PreparedOutline {
public:
	~PreparedOutline();
	size_t OperationCount() const;
	uint64_t SequenceAt(size_t) const;
	void EncodeDependencies(id<MTL4CommandBuffer>);
	void EncodeOperation(size_t, id<MTL4RenderCommandEncoder>);
	void DidComplete();
private:
	PreparedOutline();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class OutlineGPU;
};
class OutlineGPU {
public:
	// Original shader slots: 0 map depth, 1 model depth, 2 model misc MRT4.
	// The resolver retains exact native producer and sampling state. Its caller
	// closes over the actual submission for ResolveCUSOutlineInput's fourth arg.
	using ImageResolver = std::function<OutlineImage(RuntimeLease&, const NativeCUSTextureBinding&, unsigned samplerSlot)>;
	OutlineGPU();
	~OutlineGPU();
	std::shared_ptr<const NativeOutlineProgramConsumer> LinkOriginalStages(
		RuntimeLease&, const NativeOutlineProgramSource&, double& milliseconds);
	std::unique_ptr<PreparedOutline> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeOutlineFrame>, const OutlineTarget&,
		const ImageResolver&, bool validation, double& milliseconds);
	void ClearResources();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
