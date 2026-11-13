/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4OpaqueModelMRT.h"
#include "Metal4MRTFrontier209.h"
#include "Metal4EngineModelForward.h"
#include "Metal4AlphaPass166Policy.h"
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
	// Bound actual typed owner; an absent query is deliberately Unknown.
	std::function<AlphaCallbackExecution166(const NativeOpaqueModelFrame*,size_t,uint64_t,uint64_t)> inspectExecution166;
};
// One prepared owner for the original opaque pass vector. It never acquires a
// device, lease, submission or scene attachment of its own. The common caller
// preserves every recorded source boundary; a proven CPU-only phase may keep
// its actual loaded scene encoder while still executing that phase once.
class PreparedOpaqueModelSource {
public:
	~PreparedOpaqueModelSource();
	size_t PhaseCount() const;
	uint64_t SequenceAt(size_t) const;
	uint64_t PostDeferredSequenceAt(size_t) const;
	bool IsDeferredPhase(size_t) const;
	// Pure inspection of the exact next phase in this invocation. Alpha and
	// forward bodies remain Unknown; no lifecycle or source ordinal is consumed.
	ModelPhaseExecution165 InspectPhaseExecution165(size_t,const NativeOpaqueModelFrame* invocation,
		uint64_t actualDrawFrame,uint64_t actualSubmission)const;
	bool CanScopeFirstPair220()const;
 void EncodeFirstMRTScoped220(id<MTL4CommandBuffer>);
	void EncodeMRTPhase(size_t,id<MTL4CommandBuffer>);
	// End the scene encoder before dependencies; resume the same scene targets
	// with Load before EncodeForwardPhase. No source callback is replayed.
	void AttachForwardCallbacks(size_t,OpaqueModelForwardCallbacks);
	void EncodeForwardDependencies(size_t,id<MTL4CommandBuffer>);
 // No GPU commands. A successful result consumes only this phase's CPU
 // dependency states; otherwise the existing EncodeForwardDependencies path
 // remains untouched. All alpha callbacks are conservatively Unknown.
	ModelPassDependencyReport160 TryKeepForwardPass160(size_t,MTL4RenderPassDescriptor*);
	AlphaKeepReport166 TryKeepAlphaPass166(size_t,id<MTL4CommandBuffer>,MTL4RenderPassDescriptor*,
		const NativeOpaqueModelFrame* invocation,uint64_t actualDrawFrame,uint64_t actualSubmission);
	void EncodeForwardPhase(size_t,id<MTL4RenderCommandEncoder>);
	void EncodeForwardPhase(size_t,id<MTL4CommandBuffer>,id<MTL4RenderCommandEncoder> __strong&,MTL4RenderPassDescriptor* loadedScene);
	void DidComplete(); // Only after the existing common successful retirement.
private:
	PreparedOpaqueModelSource();
	void PreparePassInspection160();
	struct Impl;std::unique_ptr<Impl> impl;
	friend std::unique_ptr<PreparedOpaqueModelSource> PrepareOpaqueModelSource(RuntimeLease&,
		CUSSourceGPU&,EngineModelDeferredGPU&,EngineModelForwardGPU&,
		std::shared_ptr<const NativeOpaqueModelFrame>,const CUSSourceTarget&,
		const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,
		uint64_t,uint64_t,bool,double&,const NativeTextureRegistry*);
};

// Prepare all actual MRT phases first, so every consumer sees its six fulfilled
// immutable backings. Encode retains the original mixed vector order instead.
std::unique_ptr<PreparedOpaqueModelSource> PrepareOpaqueModelSource(RuntimeLease&,
	CUSSourceGPU&,EngineModelDeferredGPU&,EngineModelForwardGPU&,
	std::shared_ptr<const NativeOpaqueModelFrame>,const CUSSourceTarget& actualForwardTarget,
	const EngineDeferredGeometryProvider&,const CUSSourceImageProvider&,
	uint64_t actualDrawFrame,uint64_t actualSubmission,bool validation,double& milliseconds,
 const MRTFrontier209* sealedMainRegistry209=nullptr);
} // namespace rendering::metal4
