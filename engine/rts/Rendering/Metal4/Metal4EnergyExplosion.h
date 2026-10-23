/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeEnergyExplosionData.h"
#include "Rendering/Backend/WorldFrameData.h"
namespace rendering::metal4 {
class EnergyExplosionGPU;
class PreparedEnergyExplosion {
public:
	~PreparedEnergyExplosion();
	void EncodeDependencies(id<MTL4CommandBuffer>) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t) const;
	void EncodeDraw(std::size_t,id<MTL4RenderCommandEncoder>) const;
private:
	PreparedEnergyExplosion();
	struct Impl;std::unique_ptr<Impl> impl;
	friend class EnergyExplosionGPU;
};
class EnergyExplosionGPU {
public:
	EnergyExplosionGPU();~EnergyExplosionGPU();
	void Initialize(RuntimeLease&,MTLPixelFormat,bool shaderValidation,double& milliseconds,unsigned samples=1);
	std::unique_ptr<PreparedEnergyExplosion> Prepare(RuntimeLease&,const NativeEnergyExplosionFrame&,const WorldCamera&,MTLPixelFormat color,MTLPixelFormat depth,bool shaderValidation,double& milliseconds,unsigned samples=1);
	void ClearResources(); // No per-map images; preserve actually compiled family availability.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
