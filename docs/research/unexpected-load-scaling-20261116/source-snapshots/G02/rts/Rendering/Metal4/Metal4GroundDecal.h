/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeGroundDecalData.h"
#include <functional>

namespace rendering::metal4 {
struct GroundDecalSceneImage { __strong id<MTLTexture> texture=nil;bool flipV=false; };
struct GroundDecalSceneImages {
	uint64_t sourceDrawFrame=0;
	// Actual terrain-only copy, not the later pre-water/full-scene depth.
	GroundDecalSceneImage height,normals,minimap,shadowDepth,shadowColor,terrainDepth;
	std::array<float,4> fragmentToLogical{};
};
using GroundDecalTextureProvider=std::function<id<MTLTexture>(RuntimeLease&,std::shared_ptr<const WorldTexture>)>;
class PreparedGroundDecal {
public:
	~PreparedGroundDecal();
	// Builds the original atlas once per generation in the enclosing shared
	// submission. Does not submit, wait, clear undefined padding or make mips.
	void EncodeDependencies(id<MTL4CommandBuffer>);
	// Terrain copy -> original DrawPreDecals -> engine decals -> ground flashes.
	void Encode(id<MTL4RenderCommandEncoder>);
	void DidComplete();
	id<MTLTexture> Atlas() const;
	uint32_t AtlasDrawCount() const;
private:
	struct Impl;std::unique_ptr<Impl> impl;
	explicit PreparedGroundDecal(std::unique_ptr<Impl>);
	friend class GroundDecalGPU;
};
class GroundDecalGPU {
public:
	GroundDecalGPU();~GroundDecalGPU();
	// Compile the actual atlas and surface programs before publishing the
	// private producer/consumer readiness. Does not encode or submit work.
	void Initialize(RuntimeLease&,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		uint32_t sampleCount,bool shaderValidation,double& shaderMilliseconds);
	std::unique_ptr<PreparedGroundDecal> Prepare(RuntimeLease&,std::shared_ptr<const NativeGroundDecalFrame>,
		const GroundDecalSceneImages&,MTLPixelFormat colorFormat,MTLPixelFormat depthFormat,MTLPixelFormat stencilFormat,
		bool shaderValidation,double& shaderMilliseconds,GroundDecalTextureProvider={});
	void ClearResources();
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
