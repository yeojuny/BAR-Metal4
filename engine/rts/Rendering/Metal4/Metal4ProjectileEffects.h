/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeProjectileEffectData.h"
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering::metal4 {
class ProjectileEffectsGPU;
class PreparedProjectileEffects {
public:
	~PreparedProjectileEffects();
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	void EncodeDraw(std::size_t index,id<MTL4RenderCommandEncoder> encoder) const;
	void DidComplete() const;
private:
	PreparedProjectileEffects();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class ProjectileEffectsGPU;
};
class ProjectileEffectsGPU {
public:
	ProjectileEffectsGPU();~ProjectileEffectsGPU();
	// Eagerly compile every actual supported family PSO before original gadgets.
	void Initialize(RuntimeLease& lease,MTLPixelFormat colorFormat,bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	std::unique_ptr<PreparedProjectileEffects> Prepare(RuntimeLease& lease,
		const NativeProjectileEffectFrame& frame,const WorldCamera& viewCamera,
		MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,
		bool shaderValidation,double& shaderMilliseconds,unsigned samples=1);
	void ClearResources(); // Drop map images only after real shared retirement.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
