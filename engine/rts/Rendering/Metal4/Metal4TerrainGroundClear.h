/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4RuntimeNative.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include <memory>
#include <span>

namespace rendering::metal4 {
struct CUSSourceTarget;
// Usage: prepare before the common lease commits resources; merge SequenceAt
// into the actual ordered water-ground queue, then EncodeDraw once per index
// on that same water attachment encoder. Retain this plan through common
// retirement. No separate encoder, target, submission or completion is created.
// Source order includes mask-zero, masked-out and empty-scissor events.
// Actual draws following a clear must establish their own encoder state.
class PreparedTerrainGroundClear {
public:
	~PreparedTerrainGroundClear();
	PreparedTerrainGroundClear(const PreparedTerrainGroundClear&)=delete;
	PreparedTerrainGroundClear& operator=(const PreparedTerrainGroundClear&)=delete;
	size_t DrawCount() const;
	uint64_t SequenceAt(size_t index) const;
	void EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
	PreparedTerrainGroundClear();
	friend std::shared_ptr<PreparedTerrainGroundClear> PrepareTerrainGroundClears(
		RuntimeLease&,std::span<const NativeWorldAttachmentClear>,const CUSSourceTarget&,bool,double&);
};
// Exactly one reflection/refraction/terrain target, or the five RGBA8/D32
// model MRT owned by one NativeOpaqueModelPass. ModelDeferred is phase-local
// and accepts only the original color/depth clear; it has no stencil target.
// All are sample1. The caller proves encoder attachment identity; this scalar target
// contract supplies actual physical/logical extents, never the camera viewport.
// Existing runtime/compiler are reused. Without a new persistent/global cache,
// a nonempty physical-clear plan compiles one library and its distinct color
// mask PSOs per preparation. shaderMilliseconds includes those compiler calls.
// This source proposal has not been compiled, executed or visually accepted.
std::shared_ptr<PreparedTerrainGroundClear> PrepareTerrainGroundClears(RuntimeLease&,
	std::span<const NativeWorldAttachmentClear>,const CUSSourceTarget&,bool validation,double& shaderMilliseconds);
} // namespace rendering::metal4
