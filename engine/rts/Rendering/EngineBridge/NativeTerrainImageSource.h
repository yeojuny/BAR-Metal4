/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeTerrainSourceData.h"
#include <map>

namespace rendering {
enum class NativeTerrainImageRole {
	DiffuseSquare,Height,Detail,Shading,ShadowDepth,Normals,Specular,SplatDetail,SplatDistribution,
	SkyReflection,SkyReflectionModulation,BlendNormals,Emission,ParallaxHeight,
	SplatNormal0,SplatNormal1,SplatNormal2,SplatNormal3,Info,ShadowColor,
	MiniMap,GrassShading,GrassBlade,GrassFar // Append only; existing source role ordinals are unchanged.
};
struct NativeTerrainImageKey {
	const void* originalOwner=nullptr;
	NativeTerrainImageRole role=NativeTerrainImageRole::DiffuseSquare;
	int32_t squareX=-1,squareZ=-1; // Only DiffuseSquare has coordinates.
	bool operator==(const NativeTerrainImageKey&) const = default;
};

// Logical source-lifetime routing into the Session's EXISTING registry/history.
// No decoder, image cache, GPU object, device, upload or execution lease exists
// here. Original creation sites call Create/Bind/SetBoundSampling/Allocate/
// Publish in their original order. Existing map/shadow/cube/Lua/named publishers
// supply Borrow instead of allocating a second logical object for their image.
//
// Attachment sites: SMFReadMap's actual terrain texture creation/replacement;
// SMFGroundTextures::{LoadSquareTexture,LoadSquareTexturePersistent,
// SetSquareLuaTexture,destructor}; selected SMFRenderState Enable/Disable and
// BindSquareTexture. Bind NEVER selects unit0, enables texturing, or performs
// LuaTexture semantics. The source writer subsequently mirrors UI-visible2D
// units0/1 via SyncObservedTextureBinding, with no second bind/history event.
class NativeTerrainImageSource {
public:
	NativeTerrainImageSource()=default;
	NativeTerrainImageSource(const NativeTerrainImageSource&)=delete;
	NativeTerrainImageSource& operator=(const NativeTerrainImageSource&)=delete;
	NativeTerrainImageSource(NativeTerrainImageSource&&)=delete;
	NativeTerrainImageSource& operator=(NativeTerrainImageSource&&)=delete;
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&);
	void Detach(); // Requires real source lifetimes already released.
	std::shared_ptr<const NativeCUSTextureObject> Create(const NativeTerrainImageKey&,std::string originalSource);
	void Borrow(const NativeTerrainImageKey&,std::shared_ptr<const NativeCUSTextureObject>);
	void ForgetBorrow(const NativeTerrainImageKey&); // Does not delete another owner's texture.
	void Delete(const NativeTerrainImageKey&); // Actual owned source glDelete only.
	void Bind(const NativeTerrainImageKey&);
	void BindDefault(NativeCUSTextureTarget);
	void SetBoundSampling(NativeCUSTextureTarget,const NativeTextureSampling&);
	void Allocate(const NativeTerrainImageKey&,const NativeTextureAllocation&);
	void Publish(const NativeTerrainImageKey&,std::shared_ptr<const WorldTexture>,std::shared_ptr<const NativeTextureBacking>);
	std::shared_ptr<const NativeCUSTextureObject> Lookup(const NativeTerrainImageKey&) const;
private:
	struct Less {bool operator()(const NativeTerrainImageKey&,const NativeTerrainImageKey&) const;};
	struct Entry {std::shared_ptr<const NativeCUSTextureObject> original;bool borrowed=false;};
	std::map<NativeTerrainImageKey,Entry,Less> entries;
	NativeTextureRegistry* registry=nullptr;
	NativeCUSTextureHistory* history=nullptr;
	uint64_t context=0;
	void Check() const;
	static NativeCUSTextureTarget Target(const NativeTerrainImageKey&);
	unsigned ActiveUnit() const;
	const Entry& Find(const NativeTerrainImageKey&) const;
	void RequireBound(const NativeTerrainImageKey&) const;
};
} // namespace rendering
