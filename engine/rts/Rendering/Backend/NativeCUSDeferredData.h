/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include "NativeDeferredColorData.h"
#include "NativeShieldDepthPublication.h"

namespace rendering {

// A phase owns the original Map or model five-output deferred target. The
// model CUS unit callback draws BOTH its unit and feature bins; feature does
// not divide that callback's instance stream. Map keeps its ordered clear.
struct NativeCUSDeferredPhase {
	uint64_t sourceDrawFrame=0;
	WorldCamera camera;
	bool feature=false,clearBefore=true,clearOnly=false;
 NativeGeometryTextureRole role=NativeGeometryTextureRole::Models;
	std::array<NativeDeferredColorVersion,5> colors;
	NativeDeferredColorVersion combinedColor; // Optional sixth color; depth remains separate.
	NativeShieldDepthVersion depth;
	std::shared_ptr<const NativeCUSSourceFrame> lua;
};

// Call only for an actual selected source phase, before its original Lua
// callbacks. Six names keep one logical lifetime; each captured phase keeps
// its own immutable contents so PostDeferred consumers cannot observe a
// later feature clear. A skipped phase never calls this publisher.
NativeCUSDeferredPhase BeginNativeCUSDeferredPhase(NativeTextureRegistry&,
	const NativeGeometryTextureNames&,uint64_t sourceDrawFrame,const WorldCamera&,
	bool feature,bool clearBefore);
NativeCUSDeferredPhase BeginNativeMapDeferredPhase(NativeTextureRegistry&,
 const NativeGeometryTextureNames&,uint64_t,const WorldCamera&);
void FinishNativeMapDeferredPhase(NativeCUSDeferredPhase&,std::shared_ptr<const NativeCUSSourceFrame>);
void FinishNativeCUSDeferredPhase(NativeCUSDeferredPhase&,
	std::shared_ptr<const NativeCUSSourceFrame> actualCallback);
void CancelNativeCUSDeferredPhase(NativeTextureRegistry&,const NativeCUSDeferredPhase&) noexcept;
void ValidateNativeCUSDeferredPhase(const NativeCUSDeferredPhase&);
} // namespace rendering
