/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <utility>
#include "Rendering/Backend/NativeShieldDepthBacking.h"
#include "Rendering/Backend/NativeTextureRegistry.h"
#include "Rendering/Backend/NativeMapDepthPlan.h"
#include "Rendering/Backend/NativeModelDepthPlan.h"
namespace rendering {
struct NativeShieldDepthVersion {
	std::shared_ptr<const NativeCUSTextureObject> previous, published;
	std::shared_ptr<const NativeShieldDepthBacking> backing;
	bool Changed() const noexcept { return previous != published; }
};
struct NativeShieldDepthFrame {
	uint64_t sourceDrawFrame=0, submissionIdentity=0;
	bool sealed=false;
	WorldCamera camera;
	NativeMapDepthSelection mapSelection;
	NativeModelDepthPlan modelPlan;
	NativeShieldDepthVersion map, models;
	bool HasWriters() const noexcept { return map.Changed() || models.Changed(); }
};
class NativeTextureContext;
// Main/source-owned mutable builder. Render receives only Receipt() after seal.
class NativeShieldCaptureJournal {
public:
 NativeShieldCaptureJournal()=default;
 NativeShieldCaptureJournal(const NativeShieldCaptureJournal&)=delete;
 NativeShieldCaptureJournal& operator=(const NativeShieldCaptureJournal&)=delete;
 std::shared_ptr<const NativeShieldDepthFrame> Begin(NativeTextureContext&,uint64_t,const WorldCamera&,const NativeMapDepthSelection&,uint64_t submissionIdentity);
 std::shared_ptr<const NativeShieldDepthFrame> PublishModels(NativeTextureContext&,const WorldFrame&,const NativeModelDepthSelection&);
 std::shared_ptr<const NativeShieldDepthFrame> BorrowModels(NativeTextureContext&,const WorldFrame&);
 std::shared_ptr<const NativeShieldDepthFrame> Receipt() const noexcept {return pending;}
 void Swap(NativeShieldCaptureJournal& other) noexcept {
  using std::swap;swap(generation,other.generation);swap(registry,other.registry);pending.swap(other.pending);
 }
 bool CanCancel() const noexcept;
 bool Cancel() noexcept;
 void Reset() noexcept {pending.reset();registry=nullptr;generation=0;} // Accepted/retired by caller.
private:
 void ValidateModelStage(const WorldFrame&) const;
 uint64_t generation=0;
 NativeTextureRegistry* registry=nullptr;
 std::shared_ptr<NativeShieldDepthFrame> pending;
};
NativeShieldDepthVersion PublishNativeShieldDepth(NativeTextureRegistry&,const NativeGeometryTextureNames&,
	NativeTextureBackingKind,uint64_t sourceDrawFrame,const WorldCamera&,bool actualWriter);
bool CanCancelNativeShieldDepth(const NativeShieldDepthFrame&) noexcept;
bool CancelNativeShieldDepth(NativeTextureRegistry&,const NativeShieldDepthFrame&) noexcept;
// Serial capture rollback requires the exact retained pending owner.
bool CancelPendingNativeShieldDepth(NativeTextureRegistry&,std::shared_ptr<NativeShieldDepthFrame>& pending,
 const std::shared_ptr<const NativeShieldDepthFrame>& expected) noexcept;
}
