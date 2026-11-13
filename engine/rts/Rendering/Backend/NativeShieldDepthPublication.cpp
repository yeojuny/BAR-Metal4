/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShieldDepthPublication.h"
#include <stdexcept>
#include <type_traits>
#include "NativeTextureContext.h"
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
namespace rendering {

namespace {void CaptureRequire(bool value,const char* message){if(!value)throw std::logic_error(message);}}
std::shared_ptr<const NativeShieldDepthFrame> NativeShieldCaptureJournal::Begin(NativeTextureContext& context,uint64_t frame,const WorldCamera& camera,const NativeMapDepthSelection& selection,uint64_t submissionIdentity) {
 CaptureRequire(!pending,"Original map GBuffer publication was not consumed or aborted");
 auto next=std::make_shared<NativeShieldDepthFrame>();next->sourceDrawFrame=frame;
 CaptureRequire(submissionIdentity!=0,"Shield capture requires an actual submission identity");
 next->submissionIdentity=submissionIdentity;next->camera=camera;next->mapSelection=selection;
 const auto object=context.Registry().Find(context.Geometry(NativeGeometryTextureRole::Map).Name(NativeGeometryTextureNames::DepthAttachment));
 const auto backing=object?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object->backing):nullptr;
 CaptureRequire(backing&&backing->Kind()==NativeTextureBackingKind::MapDepth,"Retained original map depth has no actual named backing");
 next->map={object,object,backing};registry=&context.Registry();generation=registry->ContextGeneration();pending=std::move(next);return pending;
}
void NativeShieldCaptureJournal::ValidateModelStage(const WorldFrame& frame) const {
 CaptureRequire(pending&&!pending->sealed&&pending->sourceDrawFrame==frame.sourceDrawFrame,"Model depth requires this draw's unsealed map capture");
 CaptureRequire(pending->camera.view==frame.viewCamera.view&&pending->camera.projection==frame.viewCamera.projection&&pending->camera.viewport==frame.viewCamera.viewport,"Main camera changed between original map and model GBuffer writers");
}
std::shared_ptr<const NativeShieldDepthFrame> NativeShieldCaptureJournal::PublishModels(NativeTextureContext& context,const WorldFrame& frame,const NativeModelDepthSelection& selection) {
 ValidateModelStage(frame);CaptureRequire(!frame.opaqueModelFrame,"Standalone model depth cannot replace actual MRT model publications");
 CaptureRequire(registry==&context.Registry()&&generation==registry->ContextGeneration(),"Model capture changed its source registry epoch");
 auto plan=BuildNativeModelDepthPlan(frame,selection);
 auto models=PublishNativeShieldDepth(*registry,context.Geometry(NativeGeometryTextureRole::Models),NativeTextureBackingKind::ModelDepth,frame.sourceDrawFrame,frame.viewCamera,!plan.phases.empty());
 static_assert(std::is_nothrow_move_assignable_v<NativeModelDepthPlan>);
 static_assert(std::is_nothrow_move_assignable_v<NativeShieldDepthVersion>);
 pending->modelPlan=std::move(plan);pending->models=std::move(models);pending->sealed=true;return pending;
}
std::shared_ptr<const NativeShieldDepthFrame> NativeShieldCaptureJournal::BorrowModels(NativeTextureContext& context,const WorldFrame& frame) {
 ValidateModelStage(frame);CaptureRequire(registry==&context.Registry()&&generation==registry->ContextGeneration(),"Model capture changed its source registry epoch");
 CaptureRequire(frame.opaqueModelFrame&&frame.opaqueModelFrame->sourceDrawFrame==frame.sourceDrawFrame,"Actual MRT depth borrowing requires this source frame");
 const auto name=context.Geometry(NativeGeometryTextureRole::Models).Name(NativeGeometryTextureNames::DepthAttachment);
 CaptureRequire(name!=0,"Actual original model depth5 name is absent");
 const auto object=registry->Find(name);
 const auto backing=object?std::dynamic_pointer_cast<const NativeShieldDepthBacking>(object->backing):nullptr;
 CaptureRequire(backing&&backing->Kind()==NativeTextureBackingKind::ModelDepth,"Actual model depth5 has no retained backing");
 pending->models={object,object,backing};pending->sealed=true;return pending;
}
bool NativeShieldCaptureJournal::CanCancel() const noexcept {return !pending||(registry&&generation==registry->ContextGeneration()&&CanCancelNativeShieldDepth(*pending));}
bool NativeShieldCaptureJournal::Cancel() noexcept {
 if(!CanCancel())return false;
 if(pending&&!CancelNativeShieldDepth(*registry,*pending))return false;
 Reset();return true;
}

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
