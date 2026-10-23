/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeNamedVolumeSource.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace rendering {
namespace {void Check(bool v,const char* why){if(!v)throw std::logic_error(why);}}
void NativeNamedVolumeSource::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h,std::function<uint64_t()> frame)
{
	Check(!registry&&r.ContextGeneration()&&r.MatchesHistory(h)&&frame,"Original named volume requires its actual context");
	registry=&r;history=&h;sourceFrame=std::move(frame);context=r.ContextGeneration();
}
void NativeNamedVolumeSource::Detach()
{
	Check(names.empty(),"Original named volumes must be killed before context detach");
	registry=nullptr;history=nullptr;sourceFrame={};context=0;
}
bool NativeNamedVolumeSource::Supports(const std::string& path)const noexcept
{
	std::string_view original=path;
	if(!original.empty()&&original.front()==':'){
		const auto end=original.find(':',1);
		if(end==std::string_view::npos)return false;
		original.remove_prefix(end+1);
	}
	return Attached()&&original=="LuaUI/images/noisetextures/noise64_cube_3.dds";
}
bool NativeNamedVolumeSource::Owns(uint32_t name)const{return names.contains(name);}
uint32_t NativeNamedVolumeSource::Upload(const std::string& source,uint32_t name,
	std::shared_ptr<const NativeCUSNoiseVolume> pixels,unsigned filter,float bias)
{
	Check(registry&&registry->ContextGeneration()==context&&pixels,"Original DDS3D upload lost its real source/context");
	Check(pixels->width==64&&pixels->height==64&&pixels->depth==64&&pixels->bytesPerRow==256&&
		pixels->bytesPerImage==16384&&pixels->bgraBytes.size()==1048576,"Original DDS3D decoded storage differs");
	Check((filter==0x2600||filter==0x2601||filter==0x2700||filter==0x2701||filter==0x2702||filter==0x2703)&&std::isfinite(bias),"Original DDS3D sampler is invalid");
	Check(!name||Owns(name),"Original named reload refers to another source object");
	const auto active=history->ActiveUnit();Check(bool(active),"Original DDS3D upload has no observed active texture unit");
	const bool created=!name;
	if(created){const auto object=registry->Create(NativeCUSTextureTarget::Image3D,source);Check(object->name<=std::numeric_limits<uint32_t>::max(),"Original named GLuint namespace overflow");name=uint32_t(object->name);}
	// CreateDDSTexture wraps the upload in GL_TEXTURE_BIT. NamedTextures adds
	// no 2D qualifier binds to compressed/volume files after this restoration.
	history->PushAttributes(true,true);
	try{
		history->Enable(*active,NativeCUSTextureTarget::Image3D,true);registry->Bind(*active,name,NativeCUSTextureTarget::Image3D);
		auto sampling=registry->Find(name)->sampling;sampling.minFilter=filter;
		if(bias!=0)sampling.lodBias=bias; // Original zero does not reset a reload's prior bias.
		registry->SetSampling(name,sampling);
		const unsigned levels=(filter==0x2600||filter==0x2601)?1:7;
		registry->Allocate(name,{64,64,64,levels,1,4}); // nv_dds uses components=4 as original internal format.
		const auto object=registry->Find(name);auto image=std::make_shared<NativeNamedVolumeBacking>();
		image->storageGeneration=object->identity;image->contentVersion=object->version+1;image->sourceDrawFrame=sourceFrame();
		image->content=NativeTextureContent::ProducerVersion;image->pixels=std::move(pixels);
		registry->PublishContent(name,{},image);names[name]=std::move(image);
	}catch(...){history->PopAttributes();if(created)registry->Delete(name);throw;}
	history->PopAttributes();return name;
}
void NativeNamedVolumeSource::Delete(uint32_t name)
{
	if(!Owns(name))return;
	Check(registry&&registry->ContextGeneration()==context,"Original named volume delete lost its context");
	registry->Delete(name);names.erase(name);
}
void NativeNamedVolumeSource::ForgetNames()noexcept{names.clear();}
NativeNamedVolumeSource& OriginalNativeNamedVolumes(){static NativeNamedVolumeSource owner;return owner;}
} // namespace rendering
