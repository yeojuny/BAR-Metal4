/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4ShieldNative.h"
#include "Rendering/Backend/NativeModelDepthPlan.h"
namespace rendering::metal4 {
// Original deferred *depth* write contract; not a claim to restore its five
// color attachments or pending full CUS vertex/material families.
using ModelDepthAlpha=NativeModelDepthAlpha;
struct ModelDepthDraw {
	id<MTLBuffer> vertices=nil, indices=nil, uniforms=nil, scene=nil;
	std::size_t uniformOffset=0, indexCount=0;
	id<MTLTexture> other=nil;
	ModelDepthAlpha alpha=ModelDepthAlpha::CUSOpaque;
	bool feature=false, projectile=false, opaque=true;
	// Actual WorldUniform and SceneUniform shared with this frame's main model
	// draw. UnitVertexMain is taken from that exact compiled world library.
};
struct ModelDepthPhase {
	std::optional<bool> feature;
	std::size_t firstDraw=0,drawCount=0;
	bool clearBefore=true;
};
struct ModelDepthSource {
	std::span<const ModelDepthPhase> phases;
	bool namedDepthStorageExists=false;
	std::uint32_t storageWidth=0,storageHeight=0; // Original WantedSize/Create, never guessed backing scale.
	const ShieldDepthImage* prior=nullptr;
};
class ModelDepthGPU;
class PreparedModelDepth {
public:
	~PreparedModelDepth();
	const ShieldDepthImage& Image() const;
	std::size_t PieceCount() const;
	bool RequiresEncode() const;
	void Encode(id<MTL4CommandBuffer>);
	void DidComplete(); // Only after common successful GPU feedback/retirement.
private:
	PreparedModelDepth();struct Impl;std::unique_ptr<Impl> impl;friend class ModelDepthGPU;
};
class ModelDepthGPU {
public:
	ModelDepthGPU();~ModelDepthGPU();
	void Initialize(RuntimeLease&,id<MTLLibrary> actualWorldLibrary,bool validation,double& shaderMilliseconds);
	std::unique_ptr<PreparedModelDepth> Prepare(RuntimeLease&,std::uint64_t sourceDrawFrame,const WorldCamera&,
		std::uint32_t renderWidth,std::uint32_t renderHeight,std::uint32_t logicalWidth,std::uint32_t logicalHeight,
		std::span<const ModelDepthDraw>,id<MTLLibrary> actualWorldLibrary,bool validation,double& shaderMilliseconds,
		const ModelDepthSource* actualSource = nullptr,std::uint64_t submissionIdentity=0);
	void ClearResources(); // After retirement; no global family readiness flag.
private:
	struct Impl;std::unique_ptr<Impl> impl;
};
}
