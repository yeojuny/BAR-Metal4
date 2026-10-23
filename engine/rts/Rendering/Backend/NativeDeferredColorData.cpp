/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDeferredColorData.h"
#include <stdexcept>
namespace rendering {
namespace {void Check(bool v,const char* s){if(!v)throw std::runtime_error(s);}}
NativeDeferredColorBacking::NativeDeferredColorBacking(uint64_t storage,uint64_t version,uint64_t frame,NativeTextureContent kind,const NativeDeferredColorMetadata& m):metadata(m)
{
	Check(storage&&version&&(m.attachment<5||(m.role==NativeGeometryTextureRole::Map&&m.attachment==NativeGeometryTextureNames::CombinedColorAttachment))&&m.view.width&&m.view.height,"Original deferred color metadata missing");
	storageGeneration=storage;contentVersion=version;sourceDrawFrame=frame;content=kind;
}
void NativeDeferredColorBacking::Fulfill(std::shared_ptr<const NativeDeferredColorResource> value)const{Check(value&&!resource,"Original deferred color storage missing or fulfilled twice");resource=std::move(value);}
NativeDeferredColorVersion PublishNativeDeferredColor(NativeTextureRegistry& registry,const NativeGeometryTextureNames& names,NativeGeometryTextureRole role,unsigned attachment,uint64_t frame,const NativeShieldDepthMetadata& view,bool writes)
{
	Check(attachment<5||(role==NativeGeometryTextureRole::Map&&attachment==NativeGeometryTextureNames::CombinedColorAttachment&&names.HasCombinedColor()),"Original deferred color index out of range");NativeDeferredColorVersion result;result.previous=result.current=registry.Find(names.Name(attachment));
	if(!writes)return result;
	Check(result.previous&&result.previous->target==NativeCUSTextureTarget::Image2D,"Original deferred color has no selected sampler2D allocation");
	const auto& a=result.previous->allocation;Check(a.width==view.width&&a.height==view.height&&a.depth==1&&a.levels==1&&a.samples==1&&(a.originalInternalFormat==0x1908||a.originalInternalFormat==0x8058),"Original deferred color storage format/extent differs");
	result.backing=std::make_shared<NativeDeferredColorBacking>(result.previous->identity,result.previous->version+1,frame,NativeTextureContent::ProducerVersion,NativeDeferredColorMetadata{role,attachment,view});
	registry.PublishContent(result.previous->name,{},result.backing);result.current=registry.Find(result.previous->name);return result;
}
void CancelNativeDeferredColor(NativeTextureRegistry& registry,const NativeDeferredColorVersion& v)noexcept{if(v.Changed())registry.CancelContentPublication(v.current,v.previous);}
}
