/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCubeTextureSource.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}constexpr auto Cube=NativeCUSTextureTarget::Cube;}
NativeCubeTextureSource& OriginalNativeCubeTextures(){static NativeCubeTextureSource source;return source;}
void NativeCubeTextureSource::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h,NativeCubeTextureServices s){Check(!Attached()&&r.MatchesHistory(h)&&r.ContextGeneration()&&s.allocate&&s.sourceDrawFrame,"Original cube source has no actual texture context/allocator");registry=&r;history=&h;services=std::move(s);}
void NativeCubeTextureSource::Detach(){registry=nullptr;history=nullptr;services={};update={};}
std::shared_ptr<const NativeCubeTextureBacking> NativeCubeTextureSource::Image(uint32_t name)const {
	Check(Attached(),"Original cube context is detached");auto object=registry->Find(name);Check(object&&object->target==Cube,"Original cube name is not live");auto image=std::dynamic_pointer_cast<const NativeCubeTextureBacking>(object->backing);Check(bool(image),"Original cube has no actual typed resource");return image;
}
void NativeCubeTextureSource::Bind(uint32_t name){Check(Attached()&&history->ActiveUnit().has_value(),"Original cube bind active unit was not observed");registry->Bind(*history->ActiveUnit(),name,Cube);}
uint32_t NativeCubeTextureSource::Create(NativeCubeTextureKind kind,uint32_t size,bool mipmaps) {
	Check(Attached()&&size,"Original cube source has no valid allocation");uint32_t levels=1;for(auto n=size;mipmaps&&n>1;n/=2)++levels;
	auto resource=services.allocate(kind,size,levels);Check(bool(resource),"Actual cube/depth framebuffer allocation failed");
	auto object=registry->Create(Cube,"original CubeMapHandler");Check(object->name<=std::numeric_limits<uint32_t>::max(),"Original cube name exceeds GLuint");
	auto image=std::make_shared<NativeCubeTextureBacking>();image->kind=kind;image->size=size;image->levels=levels;image->resource=std::move(resource);image->storageGeneration=object->identity;image->contentVersion=1;image->sourceDrawFrame=services.sourceDrawFrame();
	try {Bind(uint32_t(object->name));NativeTextureSampling sampling;sampling.minFilter=mipmaps?0x2703:0x2601;sampling.magFilter=0x2601;sampling.wrapS=sampling.wrapT=0x812F;registry->SetSampling(object->name,sampling);registry->Allocate(object->name,{size,size,6,levels,1,0x8058});registry->PublishContent(object->name,{},image);}
	catch(...){registry->Delete(object->name);throw;}
	return uint32_t(object->name);
}
void NativeCubeTextureSource::Delete(uint32_t name){if(!name)return;Check(Attached(),"Original cube delete has no context");registry->Delete(name);if(update.current&&update.current->name==name)update={};}
std::shared_ptr<NativeCubeTextureBacking> NativeCubeTextureSource::Next(uint32_t name,NativeTextureContent content) {
	const auto old=Image(name);const auto object=registry->Find(name);auto image=std::make_shared<NativeCubeTextureBacking>();image->kind=old->kind;image->size=old->size;image->levels=old->levels;image->specularFaces=old->specularFaces;
	image->resource=services.allocate(image->kind,image->size,image->levels);Check(bool(image->resource),"Original cube update has no actual native allocation");
	image->storageGeneration=object->identity;image->contentVersion=object->version+1;image->sourceDrawFrame=services.sourceDrawFrame();image->content=content;if(image->kind!=NativeCubeTextureKind::Specular)image->prior=old;return image;
}
void NativeCubeTextureSource::GenerateInitialMipmaps(uint32_t name){auto image=Next(name,NativeTextureContent::ProducerVersion);image->initialMipmaps=true;registry->PublishContent(name,{},image);}
void NativeCubeTextureSource::SpecularWrite(uint32_t name,unsigned face,unsigned y,unsigned rows,std::span<const uint8_t> bytes) {
	const auto old=Image(name);Check(old->kind==NativeCubeTextureKind::Specular&&face<6&&rows&&y<old->size&&rows<=old->size-y&&bytes.size()==size_t(rows)*old->size*4,"Original CPU specular write extent differs");
	auto image=Next(name,NativeTextureContent::ProducerVersion);auto pixels=std::make_shared<std::vector<uint8_t>>();
	if(old->specularFaces[face]) *pixels=*old->specularFaces[face];
	else {Check(y==0&&rows==old->size,"Original partial specular write precedes full source face initialization");pixels->resize(size_t(old->size)*old->size*4);}
	std::copy(bytes.begin(),bytes.end(),pixels->begin()+size_t(y)*old->size*4);image->specularFaces[face]=std::move(pixels);registry->PublishContent(name,{},image);
}
NativeCubeReflectionUpdate NativeCubeTextureSource::AdvanceReflection(uint32_t reflection,uint32_t sky,unsigned face,bool mipmaps) {
	Check(face<(sky?12u:6u),"Original reflection cursor differs");const bool skyOnly=face>=6;const uint32_t name=skyOnly?sky:reflection;
	NativeCubeReflectionUpdate next;next.previous=registry->Find(name);next.face=face%6;next.skyOnly=skyOnly;
	auto image=Next(name,NativeTextureContent::ProducerVersion);image->sourceFace=face%6;image->requiresFaceDraw=true;image->regenerateMipmaps=mipmaps&&!skyOnly;
	// A sky-only wrap regenerates the MAIN reflection cube, after the real
	// sky-face draw. Both publications form one recoverable source operation.
	try {
		registry->PublishContent(name,{},image);next.current=registry->Find(name);next.backing=image;
		if(mipmaps&&skyOnly){next.mipPrevious=registry->Find(reflection);auto mip=Next(reflection,NativeTextureContent::ProducerVersion);mip->regenerateMipmaps=true;registry->PublishContent(reflection,{},mip);next.mipCurrent=registry->Find(reflection);next.mipBacking=std::move(mip);}
	} catch (...) {
		if(next.mipCurrent)registry->CancelContentPublication(next.mipCurrent,next.mipPrevious);
		if(next.current)registry->CancelContentPublication(next.current,next.previous);
		throw;
	}
	update=std::move(next);return update;
}
void NativeCubeTextureSource::CancelReflection()noexcept{if(registry){if(update.mipCurrent)registry->CancelContentPublication(update.mipCurrent,update.mipPrevious);if(update.current)registry->CancelContentPublication(update.current,update.previous);}update={};}
} // namespace rendering
