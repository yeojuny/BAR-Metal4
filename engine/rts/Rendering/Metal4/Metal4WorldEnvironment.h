/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "../Backend/NativeWorldEnvironmentData.h"
#include <memory>
namespace rendering::metal4 {
class PreparedWorldEnvironment {
public:
	~PreparedWorldEnvironment();
	void EncodeDependencies(id<MTL4CommandBuffer> command);
	// One original reflection face per retired world request. The caller appends
	// the actual terrain to this pass after EncodeSky(reflection=true).
	MTL4RenderPassDescriptor* ReflectionPass() const;
	const WorldCamera& ReflectionCamera() const;
	bool ReflectionSkyOnly() const;
	void EncodeSky(id<MTL4RenderCommandEncoder> encoder, bool reflection);
	// Reuse the original sky resource/PSO with the actual planar-water camera.
	// No cube face is advanced, and this image is rendered top-down normally.
	uint32_t PrepareWaterSky(RuntimeLease&, const WorldCamera&);
	void EncodeWaterSky(id<MTL4RenderCommandEncoder>, uint32_t index);
	void FinishReflection(id<MTL4CommandBuffer> command);
	void DidComplete();
	bool ReflectionReady() const;
	id<MTLTexture> Reflection() const;
	id<MTLTexture> SkyReflection() const;
	id<MTLTexture> Specular() const;
	id<MTLTexture> UnitBRDF() const;
	uint32_t Face() const;
private:
	struct Impl;std::unique_ptr<Impl> impl;
	explicit PreparedWorldEnvironment(std::unique_ptr<Impl> value);
	friend class WorldEnvironmentGPU;
};
class WorldEnvironmentGPU {
public:
	WorldEnvironmentGPU();~WorldEnvironmentGPU();
	std::unique_ptr<PreparedWorldEnvironment> Prepare(RuntimeLease& lease,const NativeWorldEnvironmentFrame& frame,
		const WorldCamera& mainCamera,MTLPixelFormat mainFormat,bool validation,double& shaderMilliseconds, uint64_t sourceSubmission = 0,unsigned samples=1);
	void ClearResources(); // Only after retirement; immutable shader objects survive.
private:
	struct Impl;std::unique_ptr<Impl> impl;
	friend struct PreparedWorldEnvironment::Impl;
};
} // namespace rendering::metal4
