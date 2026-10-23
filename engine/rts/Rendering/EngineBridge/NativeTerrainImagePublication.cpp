/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTerrainImagePublication.h"
#include "Rendering/Backend/NativeSMFTextureData.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeMiniMapData.h"
#include "Rendering/Backend/NativeWorldInfoData.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/TextureFormat.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileSystem.h"
#include <algorithm>
#include <bit>
#include <tuple>
#include <stdexcept>
#include <limits>

namespace rendering {
namespace {
constexpr auto target=NativeCUSTextureTarget::Image2D;
void Require(bool valid,const char* reason){if(!valid)throw std::runtime_error(reason);}
bool External(NativeTerrainImageRole role){return role==NativeTerrainImageRole::SkyReflection || role==NativeTerrainImageRole::ShadowDepth || role==NativeTerrainImageRole::ShadowColor || role==NativeTerrainImageRole::Info;}
bool RequiredRaw(NativeTerrainImageRole role){return role>=NativeTerrainImageRole::MiniMap&&role<=NativeTerrainImageRole::GrassFar;}
}
bool NativeTerrainImagePublication::Less::operator()(const Key& a,const Key& b) const
{
	if(a.owner!=b.owner)return std::less<const void*>{}(a.owner,b.owner);
	return std::tie(a.role,a.x,a.z)<std::tie(b.role,b.x,b.z);
}
void NativeTerrainImagePublication::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h,
	ui::NativeUIRecorder& recorderValue,NativeTerrainImagePublicationServices actual)
{
	Require(!registry&&!history&&!recorder&&slots.empty()&&infoOwners.empty(),"Terrain publication attached twice");
	images.Attach(r,h);registry=&r;history=&h;recorder=&recorderValue;context=r.ContextGeneration();services=std::move(actual);
}
void NativeTerrainImagePublication::Check() const
{
	Require(registry&&history&&recorder&&context&&registry->ContextGeneration()==context&&registry->MatchesHistory(*history),"Terrain publication lacks the actual Session source owners");
}
void NativeTerrainImagePublication::Detach()
{
	Check();Require(infoOwners.empty(),"Terrain publication detached before actual info handler destruction");
	for(const auto& [key,slot]:slots)Require(slot.borrowed,"Terrain publication detached before original owned lifetimes ended");
	while(!slots.empty())Release(slots.begin()->first); // Alias lifetime ends with this source context; external owners are never deleted.
	images.Detach();registry=nullptr;history=nullptr;recorder=nullptr;context=0;services={};
}
void NativeTerrainImagePublication::Mirror()
{
	const auto unit=history->ActiveUnit();Require(unit.has_value(),"Terrain writer lacks actual active unit");
	if(*unit<2)recorder->SyncObservedTextureBinding(*unit);
}
void NativeTerrainImagePublication::RestoreBinding(const NativeCUSTextureBinding& prior)
{
	const auto unit=history->ActiveUnit();Require(unit.has_value(),"Original bitmap restore lacks active unit");
	if(prior.knowledge==NativeCUSBindingKnowledge::DefaultObject)registry->Bind(*unit,0,target);
	else if(prior.knowledge==NativeCUSBindingKnowledge::BoundObject&&prior.object)registry->Bind(*unit,registry->Resolve(prior.object)->name,target);
	else throw std::runtime_error("Original bitmap temporary bind has an unobserved prior binding");
	Mirror();
}
void NativeTerrainImagePublication::Release(const Key& key)
{
	Check();const auto found=slots.find(key);if(found==slots.end())return;
	if(found->second.borrowed){images.ForgetBorrow(found->second.source);slots.erase(found);return;}
	const std::array before{history->Binding(0,target),history->Binding(1,target)};
	images.Delete(found->second.source);
	slots.erase(found);
	if(recorder->IsCompiling())return; // Immediate deletion must not append a deferred list mirror.
	for(unsigned unit=0;unit<before.size();++unit){
		const auto after=history->Binding(unit,target);
		if(before[unit].knowledge!=after.knowledge||bool(before[unit].object)!=bool(after.object)||
			(before[unit].object&&after.object&&before[unit].object->identity!=after.object->identity))
			recorder->SyncObservedTextureBinding(unit);
	}
}
void NativeTerrainImagePublication::ReleaseOwner(const void* owner)
{
	Check();for(auto it=slots.begin();it!=slots.end();){if(it->first.owner!=owner){++it;continue;}const auto key=it++->first;Release(key);}
}
void NativeTerrainImagePublication::BorrowMapAllocation(const void* map,const void* originalTexture,
	NativeTerrainImageRole role,std::shared_ptr<const NativeCUSTextureObject> actual)
{
	Check();const Key key{map,role};Require(!slots.contains(key),"Terrain map allocation borrowed twice");
	NativeTerrainImageKey source{originalTexture,role};images.Borrow(source,std::move(actual));slots.emplace(key,Slot{source,true});
	Mirror();
}
NativeTerrainImageKey NativeTerrainImagePublication::AllocateOwned(const void* owner,const void* originalTexture,
	NativeTerrainImageRole role,const NativeTextureAllocation& allocation,const NativeTextureSampling& sampling,const std::string& sourceName)
{
	Check();Require(role==NativeTerrainImageRole::MiniMap||role==NativeTerrainImageRole::GrassFar,
		"Original owned allocation hook belongs to MiniMap/GrassFar only");
	const Key key{owner,role};auto found=slots.find(key);
	if(found==slots.end()){
		const NativeTerrainImageKey source{originalTexture,role};images.Create(source,sourceName);
		found=slots.emplace(key,Slot{source,false}).first;
	}
	Require(!found->second.borrowed&&found->second.source.originalOwner==originalTexture,
		"Original image reallocation changed its actual owned source lifetime");
	const auto source=found->second.source;images.Bind(source);images.SetBoundSampling(target,sampling);
	images.Allocate(source,allocation);return source;
}
void NativeTerrainImagePublication::OwnedImageAllocated(const void* owner,const void* originalTexture,
	NativeTerrainImageRole role,const NativeTextureAllocation& allocation,const NativeTextureSampling& sampling,const std::string& sourceName)
{
	AllocateOwned(owner,originalTexture,role,allocation,sampling,sourceName);Mirror();
}
void NativeTerrainImagePublication::MiniMapUploaded(const void* map,const void* originalTexture,
	std::shared_ptr<const WorldTexture> payload,const NativeTextureSampling& sampling)
{
	Check();Require(payload&&payload->format==WorldTextureFormat::BC1RGBA&&payload->originalInternalFormat==0x83F1&&
		payload->mips.size()==9&&!payload->generateMipmaps&&!payload->flipV,
		"Embedded SMF minimap lacks its actual original BC1 mip chain");
	for(uint32_t level=0;level<payload->mips.size();++level){
		const auto& mip=payload->mips[level];const uint32_t side=1024u>>level;
		const size_t pitch=size_t((side+3)/4)*8;
		Require(mip.width==side&&mip.height==side&&mip.bytesPerRow==pitch&&mip.bytes.size()==pitch*((side+3)/4),
			"Embedded SMF minimap mip allocation differs from its original upload");
	}
	const auto source=AllocateOwned(map,originalTexture,NativeTerrainImageRole::MiniMap,
		{1024,1024,1,9,1,0x83F1},sampling,payload->sourcePath);
	images.Publish(source,std::move(payload),{});Mirror(); // Original embedded upload leaves this object bound.
}
void NativeTerrainImagePublication::AliasGrassToMiniMap(const void* map,const void* originalGrassTexture)
{
	Check();const auto original=slots.find(Key{map,NativeTerrainImageRole::MiniMap});
	Require(original!=slots.end()&&!original->second.borrowed,"Original grass raw alias lacks its actual owned MiniMap allocation");
	const Key key{map,NativeTerrainImageRole::GrassShading};Require(!slots.contains(key),"Original grass raw alias assigned twice");
	const NativeTerrainImageKey source{originalGrassTexture,key.role};
	images.Borrow(source,images.Lookup(original->second.source));
	slots.emplace(key,Slot{source,true,0,original->second.source}); // Assignment alone does not bind or mirror.
}
void NativeTerrainImagePublication::MapLuaTexture(const void* map,NativeTerrainImageRole role,uint32_t originalName)
{
	Check();const Key key{map,role,-2,-2};Release(key);if(!originalName)return;
	Require(bool(services.resolveBorrowed),"Original Lua map image lacks its genuine publisher resolver");
	NativeTerrainImageKey source{map,role};images.Borrow(source,services.resolveBorrowed(map,role,originalName));slots.emplace(key,Slot{source,true});
}
void NativeTerrainImagePublication::BindMap(const void* map,NativeTerrainImageRole role,uint32_t originalName,bool lua)
{
	Check();const Key key{map,role,lua?-2:-1,lua?-2:-1};auto found=slots.find(key);
	if(!lua&&role==NativeTerrainImageRole::Info&&found!=slots.end()&&!found->second.borrowed){
		// This handler's real source publisher owns the object. Do not delete its
		// lifetime while trying to turn the same logical image into an alias.
		InfoImage(map);images.Bind(found->second.source);Mirror();return;
	}
	if(!lua&&External(role)){
		Require(bool(services.resolveBorrowed),"Original external terrain image lacks its genuine publisher resolver");
		const auto actual=services.resolveBorrowed(map,role,originalName);
		Require(actual||originalName==0,"Original external nonzero terrain image has no genuine publisher object");
		const NativeTerrainImageKey source=found!=slots.end()?found->second.source:NativeTerrainImageKey{map,role};
		Release(key); // Original external publisher may replace its logical object; forget only the alias.
		if(!actual){
			images.BindDefault(role==NativeTerrainImageRole::SkyReflection?NativeCUSTextureTarget::Cube:target);
			if(role!=NativeTerrainImageRole::SkyReflection)Mirror();
			return; // The actual publisher, rather than provenance0, proved this default bind.
		}
		images.Borrow(source,actual);
		found=slots.emplace(key,Slot{source,true}).first;
	}
	if(found==slots.end()){
		Require(!lua&&!RequiredRaw(role)&&originalName==0,"Original terrain image has no actual creation or borrowed owner");
		images.BindDefault(role==NativeTerrainImageRole::SkyReflection?NativeCUSTextureTarget::Cube:target);
	}else images.Bind(found->second.source);
	if(role!=NativeTerrainImageRole::SkyReflection)Mirror();
}
uint64_t NativeTerrainImagePublication::OriginalName(const void* map,NativeTerrainImageRole role,bool lua) const
{
	Check();const auto found=slots.find(Key{map,role,lua?-2:-1,lua?-2:-1});
	if(found==slots.end()){Require(!lua&&!RequiredRaw(role),"Selected original terrain image has no observed raw/Lua lifetime");return 0;}
	return images.Lookup(found->second.source)->name;
}
std::shared_ptr<const NativeCUSTextureObject> NativeTerrainImagePublication::MapObject(const void* map,NativeTerrainImageRole role,bool lua) const
{
	Check();const auto found=slots.find(Key{map,role,lua?-2:-1,lua?-2:-1});
	Require(found!=slots.end(),"Selected original terrain image has no observed raw/Lua object");
	return images.Lookup(found->second.source);
}
std::shared_ptr<const WorldTexture> NativeTerrainImagePublication::RawMapImage(const void* map,NativeTerrainImageRole role) const
{
	Check();const auto found=slots.find(Key{map,role});
	if(found==slots.end()){Require(!RequiredRaw(role),"Required raw terrain image has no observed allocation");return {};}
	return images.Lookup(found->second.source)->image;
}
void NativeTerrainImagePublication::ShadingAllocated(const void* map,const void* originalTexture,uint32_t width,uint32_t height,const NativeTextureSampling& sampling)
{
	Check();const Key key{map,NativeTerrainImageRole::Shading};
	Require(width&&height&&!slots.contains(key),"Original terrain shading allocation lacks its actual extent or was created twice");
	const NativeTerrainImageKey source{originalTexture,key.role};images.Create(source,"CSMFReadMap::CreateShadingTex#shadingTex");
	slots.emplace(key,Slot{source,false});images.Bind(source);images.SetBoundSampling(target,sampling);
	images.Allocate(source,{width,height,1,1,1,0x8058});Mirror(); // Actual RGBA8 undefined FBO attachment, no fabricated pixels.
}
void NativeTerrainImagePublication::ShadingWritten(const void* map,const Shader::IProgramObject* program,std::array<int32_t,4> rect,uint64_t frame)
{
	Check();const auto found=slots.find(Key{map,NativeTerrainImageRole::Shading});
	Require(found!=slots.end()&&!found->second.borrowed&&program&&bool(services.captureShading),"Actual SMF shading write lacks its original allocation/program/source service");
	const auto object=images.Lookup(found->second.source);
	Require(rect[0]>=0&&rect[1]>=0&&rect[0]<=rect[2]&&rect[1]<=rect[3]&&uint32_t(rect[2])<=object->allocation.width&&uint32_t(rect[3])<=object->allocation.height,
		"Original SMF shading write rectangle differs from its actual target allocation");
	Require(found->second.writeRevision<std::numeric_limits<uint64_t>::max(),"Original SMF shading content revision overflow");
	const auto revision=found->second.writeRevision+1;
	const auto source=services.captureShading(map,revision);
	Require(source&&source->heights&&source->heights->width==object->allocation.width&&source->heights->height==object->allocation.height,
		"Original SMF shading write lacks its same actual retained height/input owner");
	auto backing=std::make_shared<NativeTerrainShadingBacking>();backing->source=source;backing->originalReadMap=map;
	backing->originalProgram=program;backing->originalWriteRevision=revision;backing->writtenRect=rect;backing->sourceDrawFrame=frame;
	backing->prior=std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(object->backing);
	backing->storageGeneration=object->identity;backing->contentVersion=object->version+1;backing->content=NativeTextureContent::ProducerVersion;
	// This is an actual FBO attachment write. It never binds the output as a sampler.
	registry->PublishContent(object->name,{},std::move(backing));found->second.writeRevision=revision;
}
std::shared_ptr<const NativeMiniMapBackground> NativeTerrainImagePublication::ShadingSource(const void* map) const
{
	Check();const auto found=slots.find(Key{map,NativeTerrainImageRole::Shading});if(found==slots.end())return {};
	const auto backing=std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(images.Lookup(found->second.source)->backing);
	return backing?backing->source:nullptr;
}
std::shared_ptr<const WorldTexture> NativeTerrainImagePublication::BitmapUploaded(const void* map,const void* originalTexture,NativeTerrainImageRole role,
	const std::string& path,bool loaded,const CBitmap& bitmap,const GL::TextureCreationParams& tcp,bool reload)
{
	Check();const Key key{map,role};auto found=slots.find(key);
	Require(found!=slots.end() || !reload || tcp.texID==0,"Original terrain reload lacks its actual texture lifetime");
	const bool dds=loaded&&FileSystem::GetExtensionLowerCase(path)=="dds";
	std::shared_ptr<const WorldTexture> payload;
	uint32_t format=0,levels=0;
	if(dds){
		Require(bool(services.resolveActualImage),"Original DDS terrain upload lacks the existing image loader/cache");
		payload=services.resolveActualImage(path,reload);Require(payload&&!payload->mips.empty()&&payload->originalInternalFormat,"Original DDS metadata/payload is unavailable; HEADLESS dummy is not a source");
		format=payload->originalInternalFormat;
		levels=uint32_t(payload->mips.size());if(payload->generateMipmaps)levels=std::bit_width(std::max(payload->mips.front().width,payload->mips.front().height));
	}else{
		Require(!bitmap.compressed&&bitmap.channels==4&&bitmap.dataType==0x1401&&bitmap.xsize>0&&bitmap.ysize>0&&bitmap.GetRawMem(),"Actual decoded terrain bitmap has no supported retained payload");
		auto image=std::make_shared<WorldTexture>();image->sourcePath=path;image->mips.push_back({uint32_t(bitmap.xsize),uint32_t(bitmap.ysize),size_t(bitmap.xsize)*4,{bitmap.GetRawMem(),bitmap.GetRawMem()+bitmap.GetMemSize()}});
		levels=tcp.reqNumLevels>0?uint32_t(tcp.reqNumLevels):uint32_t(bitmap.GetReqNumLevels());
		// CBitmap::GetIntFmt's actual unsigned-byte/four-channel table is RGBA8;
		// its HEADLESS stub returns0 and is not storage metadata. DDS never uses this table.
		image->generateMipmaps=levels>1;image->mipmappedSampling=levels>1;image->originalInternalFormat=GL::GetCompressedInternalFormat(0x8058);format=image->originalInternalFormat;payload=std::move(image);
	}
	const auto unit=history->ActiveUnit();Require(unit.has_value(),"Original bitmap allocation lacks active unit");
	const auto prior=history->Binding(*unit,target);
	if(dds)history->PushAttributes(true,true); // Actual CreateDDSTexture GL_TEXTURE_BIT includes target enables.
	if(found!=slots.end()&&found->second.rawWriteThrough&&!reload){
		// Successful CreateGrassTex bitmap upload creates a NEW raw allocation;
		// ReloadTextures instead reuses its original texID, including the alias.
		Release(key);found=slots.end();
	}
	if(found==slots.end()){
		NativeTerrainImageKey source{originalTexture,role};images.Create(source,path.empty()?"CSMFReadMap original bitmap fallback":path);
		found=slots.emplace(key,Slot{source,false}).first;
	}
	Require(!found->second.borrowed||found->second.rawWriteThrough.has_value(),
		"Original bitmap upload cannot overwrite another publisher's map allocation");
	const auto source=found->second.rawWriteThrough.value_or(found->second.source);
	if(found->second.rawWriteThrough){
		const auto original=slots.find(Key{map,NativeTerrainImageRole::MiniMap});
		Require(reload&&role==NativeTerrainImageRole::GrassShading&&original!=slots.end()&&!original->second.borrowed&&
			original->second.source==source&&images.Lookup(found->second.source)->identity==images.Lookup(source)->identity,
			"Original grass reload lost its exact raw MiniMap alias owner");
	}
	images.Bind(source);auto sampling=images.Lookup(source)->sampling;
	if(tcp.aniso>0)sampling.anisotropy=tcp.aniso;if(tcp.lodBias!=0)sampling.lodBias=tcp.lodBias;
	if(dds){
		history->Enable(*unit,target,true);sampling.minFilter=tcp.GetMinFilter(int32_t(payload->mips.size()-1));
	}else{
		sampling.wrapS=sampling.wrapT=0x2901;sampling.baseLevel=0;sampling.maxLevel=int(levels)-1;
		if(globalRendering->amdHacks){history->Enable(*unit,target,true);history->Enable(*unit,target,false);}
		sampling.minFilter=tcp.GetMinFilter(int(levels));sampling.magFilter=tcp.GetMagFilter();
	}
	images.SetBoundSampling(target,sampling);
	images.Allocate(source,{payload->mips.front().width,payload->mips.front().height,1,levels,1,format});
	images.Publish(source,payload,{});
	if(dds){history->PopAttributes();Mirror();}else RestoreBinding(prior);
	return payload;
}
void NativeTerrainImagePublication::ReleaseSquare(const void* owner,uint32_t x,uint32_t z)
{
	Release(Key{owner,NativeTerrainImageRole::DiffuseSquare,int32_t(x),int32_t(z)});
}
void NativeTerrainImagePublication::SquareLuaTexture(const void* owner,uint32_t x,uint32_t z,uint32_t originalName)
{
	ReleaseSquare(owner,x,z);if(!originalName)return;
	Require(bool(services.resolveBorrowed),"Original Lua terrain square lacks its genuine publisher resolver");
	NativeTerrainImageKey source{owner,NativeTerrainImageRole::DiffuseSquare,int32_t(x),int32_t(z)};
	images.Borrow(source,services.resolveBorrowed(owner,source.role,originalName));
	slots.emplace(Key{owner,source.role,int32_t(x),int32_t(z)},Slot{source,true});
}
void NativeTerrainImagePublication::SquareUploaded(const void* owner,uint32_t x,uint32_t z,
	std::shared_ptr<const WorldTerrainTiles> tiles,uint32_t mip,uint32_t count,uint32_t format,const NativeTextureSampling& sampling)
{
	Check();const Key key{owner,NativeTerrainImageRole::DiffuseSquare,int32_t(x),int32_t(z)};
	Require(mip<4&&count>0&&count<=4-mip,"Original square mip interval is invalid");
	Require(format==0x83F1,"Original non-BC1 terrain square needs its actual producer payload");
	Require(!slots.contains(key),"Original square upload did not release its prior lifetime");
	NativeTerrainImageKey source{owner,key.role,key.x,key.z};const auto object=images.Create(source,"CSMFGroundTextures original tile square");
	slots.emplace(key,Slot{source,false});images.Bind(source);images.SetBoundSampling(target,sampling);
	const auto extent=NATIVE_SMF_SQUARE_TEXELS>>mip;images.Allocate(source,{extent,extent,1,count,1,format});
	auto backing=std::make_shared<NativeTerrainSquareBacking>();backing->tiles=std::move(tiles);backing->squareX=x;backing->squareZ=z;backing->firstSourceMip=mip;backing->sourceMipCount=count;
	backing->storageGeneration=object->identity;backing->contentVersion=images.Lookup(source)->version+1;backing->content=NativeTextureContent::ProducerVersion;
	images.Publish(source,{},std::move(backing));Mirror();
}
void NativeTerrainImagePublication::BindSquare(const void* owner,uint32_t x,uint32_t z)
{
	Check();const auto found=slots.find(Key{owner,NativeTerrainImageRole::DiffuseSquare,int32_t(x),int32_t(z)});
	Require(found!=slots.end(),"Original square bind has no actual raw/Lua source lifetime");images.Bind(found->second.source);Mirror();
}
std::shared_ptr<const NativeCUSTextureObject> NativeTerrainImagePublication::BindBorrowedSquareDestination(const void* owner,uint32_t raw)
{
	Check();Require(owner&&bool(services.resolveBorrowed),"Original reverse square copy lacks its genuine destination publisher");
	const auto actual=services.resolveBorrowed(owner,NativeTerrainImageRole::DiffuseSquare,raw);
	Require(actual||raw==0,"Original nonzero reverse square destination has no genuine source object");
	if(!actual){BindDefault(target);return {};}
	const auto current=registry->Resolve(actual);
	Require(current->target==target,"Original reverse square destination is not the actual 2D image target");
	recorder->RecordOperation([this,current](ui::NativeUIRecorder& replay){
		if(replay.IsCompiling())return;
		Check();const auto destination=registry->ResolveRecordedName(current);
		const auto unit=history->ActiveUnit();Require(unit.has_value(),"Original reverse square bind lacks its actual active unit");
		registry->Bind(*unit,destination->name,target);Mirror();
	});
	return current;
}
void NativeTerrainImagePublication::SourceBorrowedSquareUploaded(std::shared_ptr<const NativeCUSTextureObject> actual,
	std::shared_ptr<const WorldTerrainTiles> tiles,uint32_t x,uint32_t z,uint32_t mip,uint32_t level,uint32_t format,uint32_t width,uint32_t height,uint64_t frame)
{
	Check();Require(actual&&bool(services.publishBorrowedSquare),"Original reverse square write requires its genuine Lua/UI cell and mip-preserving storage publisher");
	const auto recorded=registry->Resolve(actual);
	Require(recorded->target==target,"Original reverse square destination is not the actual 2D image target");
	Require(tiles&&tiles->gridWidth&&tiles->gridHeight&&tiles->gridWidth%NATIVE_SMF_TILES_PER_SQUARE==0&&tiles->gridHeight%NATIVE_SMF_TILES_PER_SQUARE==0&&
		x<tiles->gridWidth/NATIVE_SMF_TILES_PER_SQUARE&&z<tiles->gridHeight/NATIVE_SMF_TILES_PER_SQUARE&&mip<4,
		"Original reverse square write differs from its retained source tile grid/mip");
	Require(level==0&&format==0x83F1&&width==(NATIVE_SMF_SQUARE_TEXELS>>mip)&&height==width,
		"Original reverse square write differs from its actual compressed TexImage2D destination");
	// OpenGL2.1 section5.4: both binds and actual-target TexImage are listable;
	// client/PBO input data is copied at compilation, not looked up at replay.
	// Reuse the existing exact ExtractSquareTiles-equivalent source assembly.
	auto square=AssembleNativeSMFSquare(*tiles,x,z);
	NativeTerrainBorrowedSquareWrite write;write.originalTarget=recorded;write.tiles=std::move(tiles);
	write.sourcePayload=std::make_shared<const WorldTextureMip>(std::move(square[mip]));
	write.contextGeneration=context;write.originalObjectGeneration=recorded->identity;write.sourceDrawFrame=frame;
	write.squareX=x;write.squareZ=z;write.sourceMip=mip;write.originalDestLevel=level;write.originalInternalFormat=format;write.width=width;write.height=height;
	write.recordedCommand=recorder->IsCompiling();
	recorder->RecordOperation([this,write=std::move(write)](ui::NativeUIRecorder& replay){
		if(replay.IsCompiling())return;
		Check();Require(write.contextGeneration==context,"Original reverse square list crossed context lifetime");
		const auto unit=history->ActiveUnit();Require(unit.has_value(),"Original reverse square write lacks its actual active unit");
		const auto bound=history->Binding(*unit,target);
		Require(bound.knowledge==NativeCUSBindingKnowledge::BoundObject&&bound.object&&bound.object->target==target,
			"Original reverse square write no longer targets its actual bound destination");
		// BindTexture resolves its recorded name. TexImage instead acts on the
		// current binding at execution, with no old-object lookup or rebind.
		const auto before=registry->Resolve(bound.object);
		auto executed=write;executed.originalTarget=before;executed.originalObjectGeneration=before->identity;
		if(write.recordedCommand)executed.sourceDrawFrame=globalRendering->drawFrame;
		services.publishBorrowedSquare(executed); // The genuine owner updates its live cell and shared storage at execution.
		const auto after=registry->Resolve(before);
		Require(after->version>before->version&&after->storage==NativeTextureStorageKnowledge::Published&&
			after->allocation.width==write.width&&after->allocation.height==write.height&&after->allocation.depth==1&&after->allocation.samples==1&&
			after->allocation.originalInternalFormat==write.originalInternalFormat&&after->allocation.levels==std::max(before->allocation.levels,1u)&&after->sampling==before->sampling,
			"Original reverse square publisher did not preserve its actual name/other mip allocations and publish the real write");
		const auto afterBound=history->Binding(*unit,target);
		Require(history->ActiveUnit()==unit&&afterBound.knowledge==NativeCUSBindingKnowledge::BoundObject&&afterBound.object&&afterBound.object->identity==before->identity,
			"Original reverse square publisher changed its caller-owned active unit/destination binding");
		Mirror();
	});
}
void NativeTerrainImagePublication::BindDefault(NativeCUSTextureTarget t)
{
	Check();recorder->RecordOperation([this,t,recordedContext=context](ui::NativeUIRecorder& replay){
		if(replay.IsCompiling())return;
		Check();Require(recordedContext==context,"Original default texture bind crossed context lifetime");
		images.BindDefault(t);if(t==target)Mirror();
	});
}
void NativeTerrainImagePublication::InfoOwnerCreated(const void* owner)
{
	Check();Require(owner&&infoOwners.emplace(owner,InfoLineage{}).second,"Original info handler was registered twice");
	// CNativeInfoTextureHandler has no allocation or clear in its constructor.
	// Only a later successful SelectMode/Update event supplies an actual frame.
}
void NativeTerrainImagePublication::InvalidateInfoLineage()
{
	if(infoOwners.empty())return; // No original handler has been constructed in this context.
	Check();for(auto& [owner,lineage]:infoOwners){if(lineage.currentMap)lineage.invalidatedMap=lineage.currentMap;lineage.currentMap=0;}
	// Do not delete, clear or overwrite retained image versions at a CPU reset.
}
void NativeTerrainImagePublication::InfoDestroyed(const void* owner)
{
	Check();const auto found=infoOwners.find(owner);Require(found!=infoOwners.end(),"Original info destruction lacks its actual constructor");
	Release(Key{owner,NativeTerrainImageRole::Info});infoOwners.erase(found);
}
void NativeTerrainImagePublication::ObserveInfo(const void* owner,std::shared_ptr<const NativeWorldInfoFrame> frame)
{
	Check();const auto lineage=infoOwners.find(owner);
	Require(lineage!=infoOwners.end()&&frame&&frame->mapIdentity&&frame->modeGeneration&&
		frame->mapIdentity!=lineage->second.invalidatedMap,"Original info event has no actual handler/frame or belongs to its reset source lineage");
	Require((frame->mode==NativeWorldInfoMode::Normal||frame->mode==NativeWorldInfoMode::Los||frame->mode==NativeWorldInfoMode::Metal)&&
		frame->squaresX&&frame->squaresZ&&frame->paddedSquaresX>=frame->squaresX&&frame->paddedSquaresZ>=frame->squaresZ&&
		frame->paddedSquaresX<=16384&&frame->paddedSquaresZ<=16384&&std::has_single_bit(frame->paddedSquaresX)&&std::has_single_bit(frame->paddedSquaresZ),
		"Original info event differs from the existing producer's actual padded output extent");
	// CaptureNativeWorldInfo already validates its source events/coverage. Do not
	// rescan payloads here. SameNativeWorldInfoVersion excludes CPU draw cadence.
	const Key key{owner,NativeTerrainImageRole::Info};auto found=slots.find(key);
	if(found!=slots.end()){
		Require(!found->second.borrowed,"Original info source object was replaced with another publisher's alias");
		const auto old=std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(images.Lookup(found->second.source)->backing);
		if(old&&old->source&&SameNativeWorldInfoVersion(*old->source,*frame)){lineage->second.currentMap=frame->mapIdentity;return;}
	}else{
		const NativeTerrainImageKey source{owner,key.role};images.Create(source,"CNativeInfoTextureHandler actual WorldInfoGPU output source");
		found=slots.emplace(key,Slot{source,false}).first;
	}
	auto object=images.Lookup(found->second.source);
	const uint32_t levels=std::bit_width(std::max(frame->paddedSquaresX,frame->paddedSquaresZ));
	const NativeTextureAllocation allocation{frame->paddedSquaresX,frame->paddedSquaresZ,1,levels,1,0x8059}; // Actual RGB10_A2/full chain.
	if(object->storage==NativeTextureStorageKnowledge::Empty){
		NativeTextureSampling sampling;sampling.minFilter=levels==1?0x2601:0x2701;sampling.magFilter=0x2601;
		sampling.wrapS=sampling.wrapT=sampling.wrapR=0x812F;sampling.baseLevel=0;sampling.maxLevel=int(levels)-1;
		registry->SetSampling(object->name,sampling);
	}else Require(object->allocation==allocation,"Original info handler changed allocation without a real owner lifetime change");
	if(object->storage==NativeTextureStorageKnowledge::Empty)registry->Allocate(object->name,allocation);
	object=images.Lookup(found->second.source);
	auto backing=std::make_shared<NativeTerrainInfoBacking>();backing->source=std::move(frame);backing->originalHandler=owner;
	backing->storageGeneration=object->identity;backing->contentVersion=object->version+1;backing->sourceDrawFrame=backing->source->sourceDrawFrame;
	backing->content=NativeTextureContent::ProducerVersion;
	// Existing GPU producer output metadata: no sampler bind, fake pixels, GPU
	// allocation, replayed update or completion/readiness declaration occurs here.
	registry->PublishContent(object->name,{},backing);lineage->second.currentMap=backing->source->mapIdentity;
}
std::shared_ptr<const NativeCUSTextureObject> NativeTerrainImagePublication::InfoImage(const void* owner) const
{
	Check();const auto lineage=infoOwners.find(owner);
	Require(lineage!=infoOwners.end()&&lineage->second.currentMap,"Original info image is unobserved before its first actual event or after source reset");
	const auto found=slots.find(Key{owner,NativeTerrainImageRole::Info});Require(found!=slots.end()&&!found->second.borrowed,"Original info image lacks its actual source lifetime");
	const auto object=images.Lookup(found->second.source);const auto backing=std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(object->backing);
	Require(backing&&backing->originalHandler==owner&&backing->source&&backing->source->mapIdentity==lineage->second.currentMap,
		"Original info lookup refers to an invalidated retained image version");return object;
}
std::shared_ptr<const NativeWorldInfoFrame> NativeTerrainImagePublication::InfoSource(const void* owner) const
{
	return std::static_pointer_cast<const NativeTerrainInfoBacking>(InfoImage(owner)->backing)->source;
}
NativeTerrainImagePublication& OriginalNativeTerrainImagePublication(){static NativeTerrainImagePublication publication;return publication;}
} // namespace rendering
