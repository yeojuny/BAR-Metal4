/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeBumpWaterData.h"

namespace rendering::metal4 {
// Images from the real ordered world producers. A cube reflection, terrain-only
// depth, previous presentation or neutral texture is not a valid replacement.
struct BumpWaterSceneImage {
	__strong id<MTLTexture> texture = nil;
	bool flipV = false;
};
struct BumpWaterSceneImages {
	uint64_t sourceDrawFrame = 0;
	uint32_t renderWidth = 0, renderHeight = 0;
	BumpWaterSceneImage shading, planarReflection, preWaterColor, preWaterDepth;
	BumpWaterSceneImage shadowDepth, info, shadowColor;
};
struct BumpWaterCompletion {
	uint64_t generation=0,normalSequence=0,coastSequence=0;
};
class PreparedBumpWater {
public:
	~PreparedBumpWater();
	// Original per-simulation-frame normal/coast recurrence. Call before the
	// water surface, once in the shared submission; only DidComplete retires it.
	void EncodeDependencies(id<MTL4CommandBuffer>);
	// Caller has ended/saved the pre-water world pass, produced the actual
	// planar reflection + color/depth copies, and resumed its loaded attachments.
	void Encode(id<MTL4RenderCommandEncoder>);
	BumpWaterCompletion DidComplete();
	id<MTLTexture> Normal() const;
	id<MTLTexture> Coast() const;
	uint32_t DependencyDrawCount() const;
private:
	struct Impl;std::unique_ptr<Impl> impl;
	explicit PreparedBumpWater(std::unique_ptr<Impl>);
	friend class BumpWaterGPU;
};
class BumpWaterGPU {
public:
	BumpWaterGPU();~BumpWaterGPU();
	std::unique_ptr<PreparedBumpWater> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeBumpWaterFrame>,const BumpWaterSceneImages&,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Only after the shared submission retires.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
