/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeStartBoxData.h"
#include "Rendering/Backend/NativeStartBoxImages.h"
#include "Rendering/Backend/NativePostprocessData.h"
#include <functional>

namespace rendering::metal4 {
using StartBoxImageKind = NativeStartBoxImageKind;
struct PreparedStartBoxImage {
	StartBoxImageKind kind = StartBoxImageKind::Empty;
	id<MTLTexture> texture = nil;
	id<MTLSamplerState> sampler = nil;
	std::shared_ptr<const NativeCUSTextureObject> original;
	std::shared_ptr<const void> storageOwner;
	bool flipV = false;
	// Images must be the retained version's actual storage with identity
	// swizzle. This leaf applies the observed GL swizzle exactly once.
	std::function<void()> requireReadable;
};
using StartBoxImageResolver = std::function<PreparedStartBoxImage(RuntimeLease&,
	const NativeCUSTextureBinding&, std::optional<NativePostprocessConsumer>)>;

class PreparedStartBoxDraw {
public:
	~PreparedStartBoxDraw();
	void Encode(id<MTL4RenderCommandEncoder>) const;
	bool Empty() const;
private:
	PreparedStartBoxDraw();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class StartBoxGPU;
};

// One leaf serves both original world stages and the source-ordered minimap
// screen/RTT command. It never opens a pass, submits, copies a framebuffer or
// creates a competing map-normal producer.
class StartBoxGPU {
public:
	StartBoxGPU();~StartBoxGPU();
	void ClearResources(); // Only after serialized GPU retirement.
	std::unique_ptr<PreparedStartBoxDraw> Prepare(RuntimeLease&,const NativeStartBoxDraw&,
		const StartBoxImageResolver&,MTLViewport,MTLScissorRect,
		MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,
		unsigned samples,bool validation,double& shaderMilliseconds,
		std::optional<NativePostprocessConsumer> consumer = std::nullopt);
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
