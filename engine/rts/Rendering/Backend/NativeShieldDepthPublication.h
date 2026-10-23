/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
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
	WorldCamera camera;
	NativeMapDepthSelection mapSelection;
	NativeModelDepthPlan modelPlan;
	NativeShieldDepthVersion map, models;
	bool HasWriters() const noexcept { return map.Changed() || models.Changed(); }
};
NativeShieldDepthVersion PublishNativeShieldDepth(NativeTextureRegistry&,const NativeGeometryTextureNames&,
	NativeTextureBackingKind,uint64_t sourceDrawFrame,const WorldCamera&,bool actualWriter);
void CancelNativeShieldDepth(NativeTextureRegistry&,const NativeShieldDepthFrame&) noexcept;
}
