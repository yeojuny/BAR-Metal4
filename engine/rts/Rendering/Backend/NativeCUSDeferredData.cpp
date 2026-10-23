/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSDeferredData.h"
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
static NativeCUSDeferredPhase BeginPhase(NativeTextureRegistry& registry,
	const NativeGeometryTextureNames& names,uint64_t frame,const WorldCamera& camera,
	bool feature,bool clear,NativeGeometryTextureRole role)
{
	Check(names.Width()&&names.Height(),"Original selected model MRT has no actual allocated extent");
	NativeCUSDeferredPhase out;out.sourceDrawFrame=frame;out.camera=camera;out.feature=feature;out.clearBefore=clear;out.role=role;
	const NativeShieldDepthMetadata view{camera.view,camera.projection,camera.viewport,names.Width(),names.Height()};
	try {
		for(unsigned attachment=0;attachment<out.colors.size();++attachment)
			out.colors[attachment]=PublishNativeDeferredColor(registry,names,role,attachment,frame,view,true);
		if(role==NativeGeometryTextureRole::Map&&names.HasCombinedColor())
			out.combinedColor=PublishNativeDeferredColor(registry,names,role,NativeGeometryTextureNames::CombinedColorAttachment,frame,view,true);
		out.depth=PublishNativeShieldDepth(registry,names,role==NativeGeometryTextureRole::Map?NativeTextureBackingKind::MapDepth:NativeTextureBackingKind::ModelDepth,frame,camera,true);
		Check(out.depth.Changed(),"Original selected MRT depth did not publish alongside its five colors");
	} catch(...) {CancelNativeCUSDeferredPhase(registry,out);throw;}
	return out;
}
NativeCUSDeferredPhase BeginNativeCUSDeferredPhase(NativeTextureRegistry& r,const NativeGeometryTextureNames& n,uint64_t f,const WorldCamera& c,bool feature,bool clear)
{return BeginPhase(r,n,f,c,feature,clear,NativeGeometryTextureRole::Models);}
NativeCUSDeferredPhase BeginNativeMapDeferredPhase(NativeTextureRegistry& r,const NativeGeometryTextureNames& n,uint64_t f,const WorldCamera& c)
{
 // The actual original clear is an ordered event, including masks/scissor.
 // Load/COW prior contents first; do not replace it with an unconditional clear.
 return BeginPhase(r,n,f,c,false,false,NativeGeometryTextureRole::Map);
}
void FinishNativeMapDeferredPhase(NativeCUSDeferredPhase& phase,std::shared_ptr<const NativeCUSSourceFrame> callback)
{
 Check(phase.role==NativeGeometryTextureRole::Map,"Map MRT callback belongs to a model phase");
 Check(!phase.lua&&callback&&callback->sourceDrawFrame==phase.sourceDrawFrame,"Original map MRT callback is absent, repeated or from another draw");
 phase.lua=std::move(callback);ValidateNativeCUSDeferredPhase(phase);
}
void FinishNativeCUSDeferredPhase(NativeCUSDeferredPhase& phase,std::shared_ptr<const NativeCUSSourceFrame> callback)
{
	Check(phase.role==NativeGeometryTextureRole::Models,"Model MRT callback belongs to a map phase");
 Check(!phase.lua&&callback&&callback->sourceDrawFrame==phase.sourceDrawFrame,"Original model MRT callback was omitted, repeated or captured in another frame");
	for(const auto& draw:callback->draws)
		Check(draw.program&&(draw.program->originalPass==0||HasNoNativeCUSAttachmentEffects(draw)),"Original model MRT received a forward/reflection/shadow program");
	phase.lua=std::move(callback);ValidateNativeCUSDeferredPhase(phase);
}
void CancelNativeCUSDeferredPhase(NativeTextureRegistry& registry,const NativeCUSDeferredPhase& phase) noexcept
{
	if(phase.depth.Changed())registry.CancelContentPublication(phase.depth.published,phase.depth.previous);
	CancelNativeDeferredColor(registry,phase.combinedColor);
	for(auto it=phase.colors.rbegin();it!=phase.colors.rend();++it)CancelNativeDeferredColor(registry,*it);
}
void ValidateNativeCUSDeferredPhase(const NativeCUSDeferredPhase& phase)
{
	Check(phase.role==NativeGeometryTextureRole::Models||phase.role==NativeGeometryTextureRole::Map,"Original MRT role is invalid");
 Check(phase.depth.backing&&phase.depth.backing->Kind()==(phase.role==NativeGeometryTextureRole::Map?NativeTextureBackingKind::MapDepth:NativeTextureBackingKind::ModelDepth),"Original MRT role and named depth differ");
 Check(phase.lua&&phase.lua->sourceDrawFrame==phase.sourceDrawFrame,"Original model MRT lacks its captured callback");
	Check(phase.depth.Changed()&&phase.depth.backing&&phase.depth.backing->sourceDrawFrame==phase.sourceDrawFrame,
		"Original model MRT lacks its corresponding depth version");
	const auto& depth=phase.depth.backing->Metadata();
	Check(depth.view==phase.camera.view&&depth.projection==phase.camera.projection&&depth.viewport==phase.camera.viewport,
		"Original model MRT depth/callback camera differs");
	for(unsigned n=0;n<phase.colors.size();++n){
		const auto& color=phase.colors[n];Check(color.Changed()&&color.backing&&color.backing->sourceDrawFrame==phase.sourceDrawFrame,"Original model MRT color version is absent");
		const auto& m=color.backing->Metadata();
		Check(m.role==phase.role&&m.attachment==n&&m.view.width==depth.width&&m.view.height==depth.height&&
			m.view.view==depth.view&&m.view.projection==depth.projection&&m.view.viewport==depth.viewport,"Original five-MRT outputs do not share their actual source view");
	}
	if(phase.combinedColor.Changed()){
		const auto& color=phase.combinedColor;Check(phase.role==NativeGeometryTextureRole::Map&&color.backing&&color.backing->sourceDrawFrame==phase.sourceDrawFrame,"Combined color lacks its actual Map phase");
		const auto& m=color.backing->Metadata();Check(m.role==phase.role&&m.attachment==NativeGeometryTextureNames::CombinedColorAttachment&&m.view.width==depth.width&&m.view.height==depth.height&&m.view.view==depth.view&&m.view.projection==depth.projection&&m.view.viewport==depth.viewport,"Combined color and named depth have different source owners");
	}
	for(const auto& draw:phase.lua->draws){ValidateNativeCUSSourceDraw(draw);Check(draw.program&&(draw.program->originalPass==0||HasNoNativeCUSAttachmentEffects(draw)),"Original deferred phase contains another writing source program pass");}
}
} // namespace rendering
