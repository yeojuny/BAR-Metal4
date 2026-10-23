/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4OpaqueModelMRT.h"
#include "Metal4EngineModelForward.h"
#include <functional>
#include <utility>
#include <vector>

namespace rendering::metal4 {
// Source-owned generic/typed alpha callbacks, interleaved with engine and CUS
// by the one original world ordinal. The lifetime is retained by this owner.
struct OpaqueModelForwardCallbacks {
	std::vector<std::pair<uint64_t,std::function<void(id<MTL4RenderCommandEncoder>)>>> draws;
	std::vector<std::pair<uint64_t,std::function<void(id<MTL4CommandBuffer>)>>> commands;
	std::shared_ptr<const void> lifetime;
	std::function<void(id<MTL4CommandBuffer>)> dependencies;
	std::function<void()> didComplete;
};
// One prepared owner for the original opaque pass vector. It never acquires a
// device, lease, submission or scene attachment of its own. The common caller
// closes/resumes its actual scene encoder at each recorded source boundary.
class PreparedOpaqueModelSource {
public:
	~PreparedOpaqueModelSource();
	size_t PhaseCount() const;
	uint64_t SequenceAt(size_t) const;
	uint64_t PostDeferredSequenceAt(size_t) const;
	bool IsDeferredPhase(size_t) const;
	void EncodeMRTPhase(size_t,id<MTL4CommandBuffer>);
	// End the scene encoder before dependencies; resume the same scene targets
	// with Load before EncodeForwardPhase. No source callback is replayed.
	void AttachForwardCallbacks(size_t,OpaqueModelForwardCallbacks);
	void EncodeForwardDependencies(size_t,id<MTL4CommandBuffer>);
	void EncodeForwardPhase(size_t,id<MTL4RenderCommandEncoder>);
	void EncodeForwardPhase(size_t,id<MTL4CommandBuffer>,id<MTL4RenderCommandEncoder> __strong&,MTL4RenderPassDescriptor* loadedScene);
	void DidComplete(); // Only after the existing common successful retirement.
private:
	PreparedOpaqueModelSource();
	struct Impl;std::unique_ptr<Impl> impl;
	friend std::unique_ptr<PreparedOpaqueModelSource> PrepareOpaqueModelSource(RuntimeLease&,
		CUSSourceGPU&,EngineModelDeferredGPU&,EngineModelForwardGPU&,
		std::shared_ptr<const NativeOpaqueModelFrame>,const CUSSourceTarget&,
		const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,
		uint64_t,uint64_t,bool,double&);
};

// Prepare all actual MRT phases first, so every consumer sees its six fulfilled
// immutable backings. Encode retains the original mixed vector order instead.
std::unique_ptr<PreparedOpaqueModelSource> PrepareOpaqueModelSource(RuntimeLease&,
	CUSSourceGPU&,EngineModelDeferredGPU&,EngineModelForwardGPU&,
	std::shared_ptr<const NativeOpaqueModelFrame>,const CUSSourceTarget& actualForwardTarget,
	const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,
	uint64_t actualDrawFrame,uint64_t actualSubmission,bool validation,double& milliseconds);
} // namespace rendering::metal4
