/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMapTexturePublication.h"
#include "WorldFrameData.h"
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {void Require(bool value,const char* why){if(!value)throw std::logic_error(why);}constexpr auto target=NativeCUSTextureTarget::Image2D;}
void NativeMapTextureBacking::Fulfill(std::shared_ptr<const NativeMapTextureResource> actual)const
{
	Require(actual && !resource,"Original map texture backing must receive its actual GPU resource once");resource=std::move(actual);
}
void NativeMapTexturePublication::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h)
{
	Require(!registry && !owner && r.ContextGeneration() && r.MatchesHistory(h),"Original map textures need one actual context owner");registry=&r;history=&h;context=r.ContextGeneration();
}
void NativeMapTexturePublication::Detach()
{
	Require(!owner && !names[0] && !names[1],"Original map texture context cannot detach before map destruction");registry=nullptr;history=nullptr;context=0;
}
void NativeMapTexturePublication::Check(const void* original)const
{
	Require(registry && history && context==registry->ContextGeneration() && owner && owner==original,"Original map texture source owner/context differs");
}
void NativeMapTexturePublication::Create(const void* original,NativeMapTextureRole role,uint32_t width,uint32_t height)
{
	Require(registry && history && registry->ContextGeneration()==context && original && width && height,"Original map texture allocation lacks actual context/map extent");
	Require(!owner || owner==original,"Two original maps share one texture lifetime");
	const auto index=size_t(role);Require(index<names.size() && !names[index],"Original map texture allocated twice");
	const auto unit=history->ActiveUnit();Require(bool(unit),"Original map texture allocation lacks active texture unit");
	const bool normals=role==NativeMapTextureRole::Normals;
	auto object=registry->Create(target,normals?"CSMFReadMap::CreateNormalTex#$normals":"CSMFReadMap::CreateHeightMapTex#$heightmap");
	owner=original;names[index]=object->name;
	registry->Bind(*unit,object->name,target);
	NativeTextureSampling sampling;sampling.minFilter=0x2600;sampling.magFilter=0x2601;sampling.wrapS=sampling.wrapT=0x812F;
	sampling.swizzle=normals?std::array<unsigned,4>{0x1903,0x1904,0x1904,0x1904}:std::array<unsigned,4>{0x1903,0x1903,0x1903,0x1903};
	if(!normals)sampling.baseLevel=sampling.maxLevel=0;
	registry->SetSampling(object->name,sampling);registry->Allocate(object->name,{width,height,1,1,1,normals?0x822Fu:0x822Eu});
	if(!normals)registry->Bind(*unit,0,target); // Exact CreateHeightMapTex final writer.
}
void NativeMapTexturePublication::HeightUploaded(const void* original)
{
	Check(original);Require(names[1] && revisions[1]!=std::numeric_limits<uint64_t>::max(),"Original height upload lifetime/revision differs");
	const auto unit=history->ActiveUnit();Require(bool(unit),"Original height upload lacks current unit");registry->Bind(*unit,names[1],target);++revisions[1];registry->Bind(*unit,0,target);
}
void NativeMapTexturePublication::NormalsRendered(const void* original)
{
	Check(original);Require(names[0] && names[1] && revisions[0]!=std::numeric_limits<uint64_t>::max(),"Original normal writer lifetime/revision differs");
	// UpdateVisNormalsAndShadingTexture explicitly sets unit0, samples the
	// actual height texture, then unbinds it. It never binds the normal output.
	history->ActiveTexture(0);registry->Bind(0,names[1],target);++revisions[0];registry->Bind(0,0,target);
}
void NativeMapTexturePublication::NormalOverride(const void* original,bool actual)
{
	Check(original);normalOverride=actual;
}
void NativeMapTexturePublication::Destroy(const void* original)
{
	Check(original);for(auto name:names)if(name)registry->Delete(name);names={};revisions={};owner=nullptr;normalOverride=false;
}
NativeTextureLookup NativeMapTexturePublication::Lookup(std::string_view name)const
{
	if(name!="$normals" && name!="$heightmap")return {};
	Check(owner);Require(name!="$normals" || !normalOverride,"Original Lua-overridden map normals require their actual replacement texture owner");
	const auto object=registry->Find(names[name=="$heightmap"]);Require(bool(object),"Original map texture name is not allocated");return {true,object,false};
}
std::shared_ptr<const NativeMapTextureFrame> NativeMapTexturePublication::Publish(std::shared_ptr<const WorldTerrainHeights> heights,uint64_t frame)
{
	Check(owner);Require(heights && HasOriginalAllocation(),"Original map texture publication lacks source heights or allocations");
	auto output=std::make_shared<NativeMapTextureFrame>();output->sourceDrawFrame=frame;
	for(size_t i=0;i<names.size();++i){
		auto prior=registry->Find(names[i]);Require(prior && prior->allocation.width==heights->width && prior->allocation.height==heights->height,"Original map texture dimensions differ from the retained existing height cache");
		auto previous=std::dynamic_pointer_cast<const NativeMapTextureBacking>(prior->backing);
		if(previous && previous->originalRevision==revisions[i] && previous->heights==heights){output->images[i]=std::move(prior);continue;}
		Require(revisions[i]>0,"Original map texture has not yet had its actual source writer");
		auto backing=std::make_shared<NativeMapTextureBacking>();backing->role=NativeMapTextureRole(i);backing->originalRevision=revisions[i];backing->heights=heights;
		backing->storageGeneration=prior->identity;backing->contentVersion=prior->version+1;backing->sourceDrawFrame=frame;backing->content=NativeTextureContent::ProducerVersion;
		registry->PublishContent(prior->name,{},backing);output->images[i]=registry->Find(prior->name);
	}
	return output;
}
NativeMapTexturePublication& OriginalNativeMapTextures(){static NativeMapTexturePublication owner;return owner;}
}
