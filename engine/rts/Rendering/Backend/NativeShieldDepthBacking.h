/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeTextureBacking.h"
#include <memory>

namespace rendering {
struct NativeShieldDepthMetadata {
	std::array<float,16> view{},projection{};
	std::array<int32_t,4> viewport{};
	uint32_t width=0,height=0;
};
struct NativeShieldDepthResource { virtual ~NativeShieldDepthResource()=default; };
// Logical object/version publication precedes Lua DrawWorld; the typed GPU
// resource is fulfilled once during Prepare, before the same-submit producer
// and consumer. Publication alone never certifies pixels or GPU completion.
class NativeShieldDepthBacking final:public NativeTextureBacking {
public:
	NativeShieldDepthBacking(NativeTextureBackingKind,uint64_t storageGeneration,uint64_t contentVersion,
		uint64_t sourceDrawFrame,NativeTextureContent,const NativeShieldDepthMetadata&);
	NativeTextureBackingKind Kind()const noexcept override{return kind;}
	const NativeShieldDepthMetadata& Metadata()const{return metadata;}
	const std::shared_ptr<const NativeShieldDepthResource>& Resource()const{return resource;}
	void Fulfill(std::shared_ptr<const NativeShieldDepthResource>)const;
private:
	NativeTextureBackingKind kind;
	NativeShieldDepthMetadata metadata;
	mutable std::shared_ptr<const NativeShieldDepthResource> resource;
};
bool IsNativeShieldDepthReadable(bool encoded,bool completed,bool allocationOnly,bool actualAllocationVerified,
	uint64_t producerSubmission,uint64_t consumerSubmission) noexcept;
} // namespace rendering
