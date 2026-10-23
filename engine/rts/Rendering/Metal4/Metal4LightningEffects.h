/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeLightningEffectData.h"
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering::metal4 {
class LightningEffectsGPU;
class PreparedLightningEffects {
public:
	~PreparedLightningEffects();
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const;
	void DidComplete() const;
private:
	PreparedLightningEffects();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class LightningEffectsGPU;
};
class LightningEffectsGPU {
public:
	LightningEffectsGPU();~LightningEffectsGPU();
	// Eagerly compile every actual supported family PSO before original gadgets.
	void Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	std::unique_ptr<PreparedLightningEffects> Prepare(RuntimeLease& lease,
		const NativeLightningEffectFrame& frame,const WorldCamera& viewCamera,
		std::uint32_t width,std::uint32_t height,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Drop map images only after real shared retirement.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
