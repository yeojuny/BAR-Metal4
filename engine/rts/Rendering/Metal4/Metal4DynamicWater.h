/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4SourceImage.h"
#include "Rendering/Backend/NativeDynamicWaterData.h"
namespace rendering::metal4 {
struct DynamicWaterSceneImages {
	__strong id<MTLTexture> reflection = nil, refraction = nil;
	bool reflectionFlipV = true, refractionFlipV = true;
};
struct DynamicWaterCompletion { uint64_t generation = 0, sequence = 0, sourceDrawFrame = 0; };
class PreparedDynamicWater {
public:
	~PreparedDynamicWater();
	void EncodeUpdates(id<MTL4CommandBuffer>);
	void EncodeDerived(id<MTL4CommandBuffer>);
	void EncodeSurface(id<MTL4RenderCommandEncoder>);
	DynamicWaterCompletion DidComplete(); // Only successful enclosing submission retirement.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	explicit PreparedDynamicWater(std::unique_ptr<Impl>);
	friend class DynamicWaterGPU;
};
class DynamicWaterGPU {
public:
	DynamicWaterGPU();
	~DynamicWaterGPU();
	std::unique_ptr<PreparedDynamicWater> Prepare(RuntimeLease&,
		std::shared_ptr<const NativeDynamicWaterFrame>, const DynamicWaterSceneImages&,
		const std::function<CUSSourceImage(const NativeCUSTextureBinding&)>&,
		MTLPixelFormat color, unsigned samples, bool validation, double& compileMs);
	void ClearResources(); // After common submission retirement; rejects live prepared work.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
