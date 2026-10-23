/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShieldDepthBacking.h"
#include <cmath>
#include <stdexcept>

namespace rendering {
NativeShieldDepthBacking::NativeShieldDepthBacking(NativeTextureBackingKind k,uint64_t storage,uint64_t version,
	uint64_t frame,NativeTextureContent c,const NativeShieldDepthMetadata& m):kind(k),metadata(m)
{
	if((k!=NativeTextureBackingKind::MapDepth&&k!=NativeTextureBackingKind::ModelDepth)||!storage||!version||!m.width||!m.height||m.viewport[2]<=0||m.viewport[3]<=0)
		throw std::invalid_argument("Original Shield depth backing needs its actual map/model storage/version/extent");
	for(const auto* matrix:{&m.view,&m.projection})for(float f:*matrix)if(!std::isfinite(f))
		throw std::invalid_argument("Original Shield depth backing camera is nonfinite");
	storageGeneration=storage;contentVersion=version;sourceDrawFrame=frame;content=c;
}
void NativeShieldDepthBacking::Fulfill(std::shared_ptr<const NativeShieldDepthResource> actual)const
{
	if(!actual||resource)throw std::logic_error("Original Shield depth backing must be fulfilled once with its real native resource");
	resource=std::move(actual);
}
bool IsNativeShieldDepthReadable(bool encoded,bool completed,bool allocationOnly,bool actualAllocationVerified,
	uint64_t producerSubmission,uint64_t consumerSubmission) noexcept
{
	// The zero consumer identity is reserved for the explicit controlled
	// producer fixture API. Actual shared-history draws require a nonzero ID.
	return completed||(allocationOnly&&actualAllocationVerified)||(encoded&&(!consumerSubmission||producerSubmission==consumerSubmission));
}
} // namespace rendering
