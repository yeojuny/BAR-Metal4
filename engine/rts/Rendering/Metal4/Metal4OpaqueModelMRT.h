/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4CUSDeferred.h"
#include "Metal4EngineModelDeferred.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"

namespace rendering::metal4 {
// Prepare one actual selected deferred entry from NativeOpaqueModelFrame.
// The shared caller retains the original passes vector order, including its
// forward and PostDeferred entries. This function never regroups phases.
// Existing engine/legacy guards remain authoritative; this is no readiness
// provider and cannot replace missing material/per-object consumers.
std::unique_ptr<PreparedCUSDeferred> PrepareOpaqueModelMRTPhase(RuntimeLease&,
	CUSSourceGPU&,EngineModelDeferredGPU&,const NativeOpaqueModelPass&,
	const EngineDeferredGeometryProvider&,const CUSSourceGPU::TextureProvider&,
	uint64_t actualDrawFrame,uint64_t actualSubmission,bool shaderValidation,
	double& shaderMilliseconds);
} // namespace rendering::metal4
