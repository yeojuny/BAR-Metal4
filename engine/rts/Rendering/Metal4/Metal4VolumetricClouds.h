/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeVolumetricCloudData.h"
#include <memory>
#include <span>

namespace rendering::metal4 {
// Actual shared immutable version allocations. The shared postprocess owner
// validates declarations/dependencies and remains responsible for publication
// after this family encoded and the common submission really retired.
struct VolumetricCloudImages {
	std::shared_ptr<const NativePostprocessVersion> depthVersion, outputVersion;
	__strong id<MTLTexture> depth = nil, output = nil;
};
class VolumetricCloudsGPU;
class PreparedVolumetricClouds {
public:
	~PreparedVolumetricClouds();
	void EncodeDependencies(id<MTL4CommandBuffer> command) const;
	std::size_t DrawCount() const;
	std::uint64_t SequenceAt(std::size_t index) const;
	std::shared_ptr<const NativePostprocessVersion> OutputAt(std::size_t index) const;
	id<MTLTexture> OriginalVolumeAt(std::size_t index) const;
	// The prior world encoder must be ended with real color/depth stored. This
	// creates only the original quarter-resolution fog FBO pass, no present.
	void EncodeDraw(std::size_t index, id<MTL4CommandBuffer> command) const;
	void DidComplete() const; // Only after actual shared feedback/event retirement.
private:
	PreparedVolumetricClouds();
	struct Impl;
	std::unique_ptr<Impl> impl;
	friend class VolumetricCloudsGPU;
};
class VolumetricCloudsGPU {
public:
	VolumetricCloudsGPU();
	~VolumetricCloudsGPU();
	void Initialize(RuntimeLease& lease, bool validation, double& compileMs);
	std::unique_ptr<PreparedVolumetricClouds> Prepare(RuntimeLease& lease,
		std::shared_ptr<const NativeVolumetricCloudFrame> frame,
		std::span<const VolumetricCloudImages> images, bool validation, double& compileMs);
	void ClearResources(); // Clear map/resource owners; preserve real initialized PSO.
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering::metal4
