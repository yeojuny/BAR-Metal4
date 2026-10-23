/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeTerrainImageSource.h"
#include "Rendering/Backend/NativeSMFTextureData.h"
#include <functional>
#include <stdexcept>
#include <tuple>

namespace rendering {
namespace {void Require(bool valid,const char* why){if(!valid)throw std::runtime_error(why);}}
bool NativeTerrainImageSource::Less::operator()(const NativeTerrainImageKey& a,const NativeTerrainImageKey& b) const
{
	if(a.originalOwner!=b.originalOwner)return std::less<const void*>{}(a.originalOwner,b.originalOwner);
	return std::tie(a.role,a.squareX,a.squareZ)<std::tie(b.role,b.squareX,b.squareZ);
}
NativeCUSTextureTarget NativeTerrainImageSource::Target(const NativeTerrainImageKey& key)
{
	Require(key.originalOwner,"Original terrain image source owner is missing");
	Require(key.role>=NativeTerrainImageRole::DiffuseSquare&&key.role<=NativeTerrainImageRole::GrassFar,
		"Original terrain image role is invalid");
	if(key.role==NativeTerrainImageRole::DiffuseSquare)Require(key.squareX>=0&&key.squareZ>=0,"Original terrain square identity is missing");
	else Require(key.squareX==-1&&key.squareZ==-1,"Non-square terrain image has invented square coordinates");
	return key.role==NativeTerrainImageRole::SkyReflection?NativeCUSTextureTarget::Cube:NativeCUSTextureTarget::Image2D;
}
void NativeTerrainImageSource::Check() const
{
	Require(registry&&history&&context&&registry->ContextGeneration()==context&&registry->MatchesHistory(*history),
		"Original terrain image source crossed its actual shared context lifetime");
}
unsigned NativeTerrainImageSource::ActiveUnit() const
{
	Check();const auto unit=history->ActiveUnit();Require(unit&&*unit<NativeCUSTextureHistory::Units,
		"Original terrain texture writer has no observed active unit");return *unit;
}
void NativeTerrainImageSource::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h)
{
	Require(!registry&&!history&&entries.empty()&&r.ContextGeneration()&&r.MatchesHistory(h),
		"Terrain image source must attach once to the existing shared registry/history");
	registry=&r;history=&h;context=r.ContextGeneration();
}
void NativeTerrainImageSource::Detach()
{
	Check();Require(entries.empty(),"Terrain image source cannot detach before actual owned/borrowed source lifetimes end");
	registry=nullptr;history=nullptr;context=0;
}
const NativeTerrainImageSource::Entry& NativeTerrainImageSource::Find(const NativeTerrainImageKey& key) const
{
	Check();Target(key);const auto it=entries.find(key);
	Require(it!=entries.end(),"Original terrain image has no observed creation/borrow owner");return it->second;
}
std::shared_ptr<const NativeCUSTextureObject> NativeTerrainImageSource::Lookup(const NativeTerrainImageKey& key) const
{
	const auto& entry=Find(key);return registry->Resolve(entry.original);
}
std::shared_ptr<const NativeCUSTextureObject> NativeTerrainImageSource::Create(const NativeTerrainImageKey& key,std::string source)
{
	Check();const auto target=Target(key);Require(!entries.contains(key),"Original terrain image source was created twice");
	auto object=registry->Create(target,std::move(source)); // Registry allocates the logical name; no GLuint conversion.
	entries.emplace(key,Entry{object,false});return object;
}
void NativeTerrainImageSource::Borrow(const NativeTerrainImageKey& key,std::shared_ptr<const NativeCUSTextureObject> actual)
{
	Check();const auto target=Target(key);Require(!entries.contains(key),"Original terrain borrowed image must be released before replacement");
	auto object=registry->Resolve(std::move(actual));Require(object->target==target,"Original borrowed terrain image target differs");
	entries.emplace(key,Entry{std::move(object),true}); // Existing logical identity, parameters and storage are retained.
}
void NativeTerrainImageSource::ForgetBorrow(const NativeTerrainImageKey& key)
{
	Require(Find(key).borrowed,"Owned terrain image cannot be forgotten without its real deletion");entries.erase(key);
}
void NativeTerrainImageSource::Delete(const NativeTerrainImageKey& key)
{
	Require(!Find(key).borrowed,"Terrain must not delete another source owner's borrowed texture");
	const auto object=Lookup(key);Require(registry->Delete(object->name),"Original terrain deletion did not delete its actual logical object");entries.erase(key);
}
void NativeTerrainImageSource::Bind(const NativeTerrainImageKey& key)
{
	const auto object=Lookup(key);registry->Bind(ActiveUnit(),object->name,object->target);
}
void NativeTerrainImageSource::BindDefault(NativeCUSTextureTarget target)
{
	Check();Require(target<NativeCUSTextureTarget::Count,"Original terrain default binding target is invalid");
	registry->Bind(ActiveUnit(),0,target);
}
void NativeTerrainImageSource::SetBoundSampling(NativeCUSTextureTarget target,const NativeTextureSampling& sampling)
{
	Check();ValidateNativeTerrainSampling(sampling);registry->SetBoundSampling(ActiveUnit(),target,sampling);
}
void NativeTerrainImageSource::RequireBound(const NativeTerrainImageKey& key) const
{
	const auto object=Lookup(key);const auto binding=history->Binding(ActiveUnit(),object->target);
	Require(binding.knowledge==NativeCUSBindingKnowledge::BoundObject&&binding.object&&binding.object->identity==object->identity,
		"Original terrain storage writer does not target its currently bound actual image");
}
void NativeTerrainImageSource::Allocate(const NativeTerrainImageKey& key,const NativeTextureAllocation& allocation)
{
	Require(!Find(key).borrowed,"Terrain storage allocation belongs to the actual borrowed publisher");RequireBound(key);
	registry->Allocate(Lookup(key)->name,allocation); // Preserve legal zero-size/incomplete storage.
}
void NativeTerrainImageSource::Publish(const NativeTerrainImageKey& key,std::shared_ptr<const WorldTexture> image,
	std::shared_ptr<const NativeTextureBacking> backing)
{
	Require(!Find(key).borrowed,"Terrain content publication belongs to the actual borrowed publisher");
	RequireBound(key); // Original owned TexImage/CompressedTexImage writes the bound object.
	const auto object=Lookup(key);
	if(const auto square=std::dynamic_pointer_cast<const NativeTerrainSquareBacking>(backing)){
		ValidateNativeTerrainSquareBacking(*square);
		Require(key.role==NativeTerrainImageRole::DiffuseSquare&&uint32_t(key.squareX)==square->squareX&&uint32_t(key.squareZ)==square->squareZ&&
			square->storageGeneration==object->identity&&!image,"Original terrain square publication owner/coordinates differ");
		const auto extent=NATIVE_SMF_SQUARE_TEXELS>>square->firstSourceMip;
		Require(object->allocation.width==extent&&object->allocation.height==extent&&object->allocation.depth==1&&
			object->allocation.samples==1&&object->allocation.levels==square->sourceMipCount,
			"Original terrain square mip interval differs from its actual source allocation");
	}
	registry->PublishContent(object->name,std::move(image),std::move(backing));
}
} // namespace rendering
