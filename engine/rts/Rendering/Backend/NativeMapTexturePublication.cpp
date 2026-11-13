/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMapTexturePublication.h"
#include "WorldFrameData.h"
#include "NativeTerrainHeightSnapshot.h"
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {void Require(bool value,const char* why){if(!value)throw std::logic_error(why);}constexpr auto target=NativeCUSTextureTarget::Image2D;}
namespace {
	NativeTerrainHeightSnapshotCache heightSnapshots;
	uint64_t cpuHeightCopyRevision=0;
}
std::shared_ptr<const WorldTerrainHeights> CaptureNativeTerrainHeightSnapshot(
	const CReadMap* original,const float* values,uint32_t width,uint32_t height)
{
	return heightSnapshots.Capture(original,values,width,height,
		OriginalNativeMapTextures().HeightSnapshotVersion(original));
}
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
	heightSnapshots.Reset();
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
void NativeMapTexturePublication::InvalidateCPUHeightSnapshot(const void* original)
{
	// Before attachment/allocation the snapshot has no authoritative version
	// and uses the original full comparison instead.
	if (!registry || owner!=original || !names[1]) return;
	Check(original);
	Require(cpuHeightCopyRevision!=std::numeric_limits<uint64_t>::max(),"Original CPU height copy revision exhausted");
	++cpuHeightCopyRevision;
}
std::optional<std::array<uint64_t,4>> NativeMapTexturePublication::HeightSnapshotVersion(const void* original)const
{
	if (!registry || owner!=original || !names[1] || !revisions[1]) return {};
	Check(original);
	return std::array<uint64_t,4>{context,names[1],revisions[1],cpuHeightCopyRevision};
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
	heightSnapshots.Reset();
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
 std::array<std::shared_ptr<const NativeCUSTextureObject>,2> priorWrites{};
 std::array<std::shared_ptr<const NativeMapTextureBacking>,2> staged{};
 try {
	for(size_t i=0;i<names.size();++i){
		auto prior=registry->Find(names[i]);Require(prior && prior->allocation.width==heights->width && prior->allocation.height==heights->height,"Original map texture dimensions differ from the retained existing height cache");
		auto previous=std::dynamic_pointer_cast<const NativeMapTextureBacking>(prior->backing);
		if(previous && previous->originalRevision==revisions[i] && previous->heights==heights){output->images[i]=std::move(prior);continue;}
		Require(revisions[i]>0,"Original map texture has not yet had its actual source writer");
  Require(prior->version<std::numeric_limits<uint64_t>::max()-1,"Map publication lacks rollback version capacity");
		auto backing=std::make_shared<NativeMapTextureBacking>();backing->role=NativeMapTextureRole(i);backing->originalRevision=revisions[i];backing->heights=heights;
		backing->storageGeneration=prior->identity;backing->contentVersion=prior->version+1;backing->sourceDrawFrame=frame;backing->content=NativeTextureContent::ProducerVersion;
  priorWrites[i]=prior;staged[i]=std::move(backing);
	}
  // Finish all source validation/backing allocation before the first writer.
  for(size_t i=0;i<names.size();++i)if(staged[i]){
   registry->PublishContent(priorWrites[i]->name,{},staged[i]);
   // Successful Replace clears deferred restoration, so Find only retains the
   // committed object here; it cannot allocate after publication.
   output->images[i]=registry->Find(priorWrites[i]->name);
  }
 }catch(...){
  // Only writes from this attempt are reversed; reused complete images are not
  // producers of this transaction. Source capture is still strictly serial.
  for(size_t i=names.size();i-->0;)if(priorWrites[i]&&output->images[i])
   registry->CancelContentPublication(output->images[i],priorWrites[i]);
  throw;
 }
	return output;
}
NativeMapTexturePublication& OriginalNativeMapTextures(){static NativeMapTexturePublication owner;return owner;}
}
