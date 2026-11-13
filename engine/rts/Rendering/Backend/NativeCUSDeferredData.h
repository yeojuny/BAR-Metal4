/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSSourceData.h"
#include <type_traits>
#include <utility>
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

// Allocate the durable receipt BEFORE Begin publishes registry contents. Begin
// rolls back its own failures; after it succeeds there is no throwing ownership
// gap before the caller receives the receipt. Do not wrap Begin in make_shared.
template<class Begin> std::shared_ptr<NativeCUSDeferredPhase> OwnNativeCUSDeferredPhase(Begin&& begin) {
 static_assert(std::is_nothrow_move_assignable_v<NativeCUSDeferredPhase>);
 auto owner=std::make_shared<NativeCUSDeferredPhase>();
 *owner=std::forward<Begin>(begin)();
 return owner;
}

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
bool CanCancelNativeCUSDeferredPhase(const NativeCUSDeferredPhase&) noexcept;
void CancelNativeCUSDeferredPhase(NativeTextureRegistry&,const NativeCUSDeferredPhase&) noexcept;
void ValidateNativeCUSDeferredPhase(const NativeCUSDeferredPhase&);
} // namespace rendering
