/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShieldDepthPublication.h"
#include <stdexcept>
namespace rendering {
NativeShieldDepthVersion PublishNativeShieldDepth(NativeTextureRegistry& registry,const NativeGeometryTextureNames& names,
	NativeTextureBackingKind kind,uint64_t frame,const WorldCamera& camera,bool writer)
{
	NativeShieldDepthVersion result;
	const auto name=names.Name(NativeGeometryTextureNames::DepthAttachment);
	if(!name)return result;
	result.previous=result.published=registry.Find(name);
	if(!result.previous)throw std::logic_error("Original named GBuffer object disappeared before its writer");
	const auto& allocation=result.previous->allocation;
	if(!allocation.width||!allocation.height){if(writer)throw std::logic_error("Original empty GBuffer cannot have a depth writer");return result;}
	result.backing=std::dynamic_pointer_cast<const NativeShieldDepthBacking>(result.previous->backing);
	if(!writer){if(!result.backing)throw std::logic_error("Original GBuffer allocation provider did not retain its actual depth backing");return result;}
	if(allocation.originalInternalFormat!=0x8CAC||allocation.samples!=1||allocation.levels!=1||allocation.depth!=1)
		throw std::logic_error("Original Shield writer needs actual single-sample D32F GBuffer storage");
	NativeShieldDepthMetadata metadata{camera.view,camera.projection,camera.viewport,allocation.width,allocation.height};
	auto next=std::make_shared<NativeShieldDepthBacking>(kind,result.previous->identity,
		result.previous->version+1,frame,NativeTextureContent::ProducerVersion,metadata);
	registry.PublishContent(name,{},next);
	result.published=registry.Find(name);result.backing=std::move(next);return result;
}
bool CanCancelNativeShieldDepth(const NativeShieldDepthFrame& frame) noexcept
{
 for(const auto* version:{&frame.models,&frame.map})if(version->Changed()&&(!version->backing||!version->backing->CanCancelUnsubmitted()))return false;
 return true;
}
bool CancelNativeShieldDepth(NativeTextureRegistry& registry,const NativeShieldDepthFrame& frame) noexcept
{
 if(!CanCancelNativeShieldDepth(frame))return false;
	for(const auto* version:{&frame.models,&frame.map})if(version->Changed())
		registry.CancelContentPublication(version->published,version->previous);
 return true;
}
bool CancelPendingNativeShieldDepth(NativeTextureRegistry& registry,std::shared_ptr<NativeShieldDepthFrame>& pending,
 const std::shared_ptr<const NativeShieldDepthFrame>& expected) noexcept
{
 if(!expected || pending!=expected)return true;
 if(!CancelNativeShieldDepth(registry,*expected))return false;
 pending.reset();return true;
}

}
