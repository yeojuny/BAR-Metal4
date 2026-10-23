/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeBasicWaterData.h"

namespace rendering::metal4 {
class PreparedBasicWater {
public:
	~PreparedBasicWater();
	void EncodeDependencies(id<MTL4CommandBuffer>); // One-time real bitmap mip generation.
	void Encode(id<MTL4RenderCommandEncoder>); // Original between-alpha water stage.
	void DidComplete(); // Publish mip readiness only after shared submission retires.
	id<MTLTexture> Texture()const;
private:
	struct Impl;std::unique_ptr<Impl> impl;
	explicit PreparedBasicWater(std::unique_ptr<Impl>);
	friend class BasicWaterGPU;
};
class BasicWaterGPU {
public:
	BasicWaterGPU();~BasicWaterGPU();
	std::unique_ptr<PreparedBasicWater> Prepare(RuntimeLease&,std::shared_ptr<const NativeBasicWaterFrame>,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Called only after prior shared submissions retire.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
