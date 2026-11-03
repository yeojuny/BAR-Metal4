/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4CUSSource.h"
#include "Metal4DeferredColorNative.h"
#include "Metal4ModelPhaseExecution165.h"
#include "Rendering/Backend/NativeCUSDeferredData.h"
#include <vector>

namespace rendering::metal4 {

// Engine-selected non-CUS objects and legacy Lua material bins retain their
// original locations on either side of DrawOpaque*Lua. The source-stage
// adapter must explicitly observe both lists, including genuinely empty
// lists; absence of an implementation is never an empty list.
struct CUSDeferredOtherDraws {
	bool observed=false;
	std::function<void(id<MTL4CommandBuffer>)> dependencies;
	std::function<void(id<MTL4RenderCommandEncoder>)> beforeLua,afterLua;
	// When selected, merge actual default model draws with CUS submissions at
	// their original per-object sequence. Grouped callbacks are then forbidden.
	bool sourceOrder=false;
	std::vector<uint64_t> drawSequences;
	std::function<void(size_t,id<MTL4RenderCommandEncoder>)> drawAt;
	// Only the proved first Map clear may become an attachment load. Its
	// drawAt callback still consumes the original ordinal and encoder state.
	std::function<bool(MTL4RenderPassDescriptor*)> initialAttachmentClear182;
	std::function<void()> didEncodeSourceDraws;
	std::function<void()> didComplete;
	std::shared_ptr<const void> lifetime;
	// Explicit caller proof, never inferred from an empty drawSequences vector:
	// with no ordered draws, dependencies/finish/retirement only advance the
	// empty consumer lifecycle and cannot modify attachment texels or encode
	// GPU commands. Unknown and Map callers retain the default false value.
	bool emptySourceCallbacksHaveNoCommands=false;
};

class PreparedCUSDeferred {
public:
	~PreparedCUSDeferred();
	const std::array<DeferredColorImage,5>& Colors()const;
	const ShieldDepthImage& Depth()const;
	// Pure query of the already-prepared 164 decision; does not test callbacks
	// or replace Encode's actual same-submission prior-readability checks.
	ModelPhaseExecution165 InspectExecution165(uint64_t actualDrawFrame,uint64_t actualSubmission)const;
	void Encode(id<MTL4CommandBuffer>); // Actual source phase, in original order.
	void DidComplete(); // After the common successful submission only.
private:
	PreparedCUSDeferred();struct Impl;std::unique_ptr<Impl> impl;
	friend std::unique_ptr<PreparedCUSDeferred> PrepareCUSDeferred(RuntimeLease&,CUSSourceGPU&,
		std::shared_ptr<const NativeCUSDeferredPhase>,const CUSSourceGPU::TextureProvider&,
		CUSDeferredOtherDraws,uint64_t,bool,double&,const NativeWorldAttachmentClear*);
};

// Provides five real RGBA8 targets plus the SAME D32F target for all source
// geometry. A proven empty feature phase retains the prior immutable storage.
// Existing logical allocation-only resources are retained by source versions;
// other no-clear phases load their real previous phase through ordered COW copies.
std::unique_ptr<PreparedCUSDeferred> PrepareCUSDeferred(RuntimeLease&,CUSSourceGPU&,
	std::shared_ptr<const NativeCUSDeferredPhase>,const CUSSourceGPU::TextureProvider&,
	CUSDeferredOtherDraws,uint64_t actualSubmission,bool validation,double& compileMilliseconds,
 const NativeWorldAttachmentClear* initialClear=nullptr);
} // namespace rendering::metal4
