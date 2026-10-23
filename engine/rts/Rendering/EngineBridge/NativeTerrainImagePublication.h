/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTerrainImageSource.h"
#include "Rendering/Textures/TextureCreationParams.hpp"
#include <functional>
#include <map>
#include <optional>

class CBitmap;
namespace rendering {
namespace ui {class NativeUIRecorder;}
struct NativeMiniMapBackground;
struct NativeWorldInfoFrame;
struct WorldTextureMip;
// One real GPU output/plan retained by its immutable source version, using
// the existing serialized common runtime. These are not another image cache.
struct NativeTerrainInfoResource {virtual ~NativeTerrainInfoResource()=default;};
struct NativeTerrainShadingResource {virtual ~NativeTerrainShadingResource()=default;};
struct NativeTerrainInfoBacking final:NativeTextureBacking {
	std::shared_ptr<const NativeWorldInfoFrame> source;
	const void* originalHandler=nullptr;
	mutable std::shared_ptr<const NativeTerrainInfoResource> resource;
	// Logical descriptor of the existing WorldInfoGPU/MetalInfoGPU output.
	// Source publication does not allocate a GPU image or certify completion.
	NativeTextureBackingKind Kind() const noexcept override {return NativeTextureBackingKind::Other;}
};
struct NativeTerrainBorrowedSquareWrite {
	// This is the existing genuine destination, not a newly owned terrain name.
	// Its prior immutable content/allocation must survive through actual retirement.
	std::shared_ptr<const NativeCUSTextureObject> originalTarget;
	std::shared_ptr<const WorldTerrainTiles> tiles;
	// TexImage copies client/PBO bytes when the command is issued, including
	// list compilation. The retained command applies these bytes on replay.
	std::shared_ptr<const WorldTextureMip> sourcePayload;
	uint64_t contextGeneration=0,originalObjectGeneration=0,sourceDrawFrame=0;
	uint32_t squareX=0,squareZ=0,sourceMip=0,originalDestLevel=0;
	uint32_t originalInternalFormat=0,width=0,height=0;
	bool recordedCommand=false; // Genuine Lua destination was validated when the list was compiled.
	// TexImage redefines ONLY originalDestLevel. The actual Lua/UI owner must
	// preserve every other allocated mip and the existing name/lifetime, update
	// its current/postprocess cell AND shared registry, and retain prior content.
	// It must preserve sampling/current unit/binding: the original caller owns
	// the one destination bind and final default bind, with no LuaTexture reset.
	// Registry-only publication would let the old cell resurrect stale contents.
	// If that Lua cell was actually deleted before replay, the recorded bind
	// resolves the current name and only its registry storage survives; never
	// revive the deleted Lua cell or its postprocess binding.
};
struct NativeTerrainShadingBacking final:NativeTextureBacking {
	mutable std::shared_ptr<const NativeTerrainShadingResource> resource;
	std::shared_ptr<const NativeMiniMapBackground> source;
	const void* originalReadMap=nullptr;
	const Shader::IProgramObject* originalProgram=nullptr; // Provenance only; never dereference after source capture.
	uint64_t originalWriteRevision=0;
	std::array<int32_t,4> writtenRect{}; // Actual half-open FBO output rectangle, not a full-map replacement.
	// Actual partial-update/copy dependency only. The common GPU retirement owner
	// clears this after successful completion, like NativeCubeTextureBacking::prior.
	mutable std::shared_ptr<const NativeTerrainShadingBacking> prior;
	// Actual common completion owner only, after the partial producer and all
	// dependent consumers have retired successfully. Source capture never calls it.
	void RetirePrior() const noexcept {prior.reset();}
	NativeTextureBackingKind Kind() const noexcept override {return NativeTextureBackingKind::Other;}
};
struct NativeTerrainImagePublicationServices {
	// Existing WorldFrameCapture image loader/cache only; never another decoder.
	std::function<std::shared_ptr<const WorldTexture>(const std::string&,bool)> resolveActualImage;
	// Original GL name is provenance for the genuine source publisher, NOT a registry name.
	// Null is allowed ONLY when that actual publisher proves an object0 bind.
	// Missing service/Unobserved must throw; raw provenance0 alone is no proof.
	std::function<std::shared_ptr<const NativeCUSTextureObject>(const void*,NativeTerrainImageRole,uint32_t)> resolveBorrowed;
	std::function<std::shared_ptr<const NativeMiniMapBackground>(const void*,uint64_t)> captureShading;
	// Actual destination publisher only. No decoder, cache or terrain-owned
	// replacement is permitted. Missing owner/cell/mip implementation must throw.
	std::function<void(const NativeTerrainBorrowedSquareWrite&)> publishBorrowedSquare;
};

class NativeTerrainImagePublication {
public:
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&,ui::NativeUIRecorder&,NativeTerrainImagePublicationServices);
	void Detach(); // After original map/square lifetimes have ended.
	std::shared_ptr<const WorldTexture> BitmapUploaded(const void* map,const void* originalMapTexture,NativeTerrainImageRole,
		const std::string& actualPath,bool actualLoadSucceeded,const CBitmap&,const GL::TextureCreationParams&,bool reload=false);
	// Actual original allocation/upload sites only. Undefined storage is never
	// reported as pixels or as a completed native producer.
	void OwnedImageAllocated(const void* owner,const void* originalTexture,NativeTerrainImageRole,
		const NativeTextureAllocation&,const NativeTextureSampling&,const std::string& originalSource);
	void MiniMapUploaded(const void* map,const void* originalMapTexture,std::shared_ptr<const WorldTexture> actualEmbeddedMips,
		const NativeTextureSampling&);
	// CreateGrassTex assigns the SAME raw MiniMap object, independently of Lua
	// overrides. Later original texID reuse writes through that exact owner.
	void AliasGrassToMiniMap(const void* map,const void* originalGrassTexture);
	void BorrowMapAllocation(const void* map,const void* originalMapTexture,NativeTerrainImageRole,
		std::shared_ptr<const NativeCUSTextureObject> actual);
	void BindMap(const void* map,NativeTerrainImageRole,uint32_t originalName,bool actualLuaOverride=false);
	void MapLuaTexture(const void* map,NativeTerrainImageRole,uint32_t originalLuaName);
	uint64_t OriginalName(const void* map,NativeTerrainImageRole,bool actualLuaOverride=false) const;
	std::shared_ptr<const NativeCUSTextureObject> MapObject(const void* map,NativeTerrainImageRole,bool actualLuaOverride=false) const;
	std::shared_ptr<const WorldTexture> RawMapImage(const void* map,NativeTerrainImageRole) const;
	void ShadingAllocated(const void* map,const void* originalMapTexture,uint32_t width,uint32_t height,const NativeTextureSampling&);
	void ShadingWritten(const void* map,const Shader::IProgramObject* actualProgram,std::array<int32_t,4> actualRect,uint64_t actualSourceDrawFrame);
	std::shared_ptr<const NativeMiniMapBackground> ShadingSource(const void* map) const;
	void SquareUploaded(const void* originalGroundTextures,uint32_t x,uint32_t z,
		std::shared_ptr<const WorldTerrainTiles>,uint32_t sourceMip,uint32_t mipCount,uint32_t actualInternalFormat,
		const NativeTextureSampling& actualSampling);
	void ReleaseSquare(const void* originalGroundTextures,uint32_t x,uint32_t z);
	void SquareLuaTexture(const void* originalGroundTextures,uint32_t x,uint32_t z,uint32_t originalLuaName);
	void BindSquare(const void* originalGroundTextures,uint32_t x,uint32_t z);
	std::shared_ptr<const NativeCUSTextureObject> BindBorrowedSquareDestination(const void* originalGroundTextures,uint32_t rawProvenance);
	void SourceBorrowedSquareUploaded(std::shared_ptr<const NativeCUSTextureObject> actualGenuineObject,
		std::shared_ptr<const WorldTerrainTiles> actualTiles,uint32_t squareX,uint32_t squareZ,uint32_t sourceMip,
		uint32_t originalDestLevel,uint32_t actualInternalFormat,uint32_t width,uint32_t height,uint64_t actualSourceDrawFrame);
	void BindDefault(NativeCUSTextureTarget);
	void ReleaseOwner(const void* originalOwner);
	void InfoOwnerCreated(const void* originalHandler); // Actual ctor only; no invented initial clear/allocation.
	void ObserveInfo(const void* originalHandler,std::shared_ptr<const NativeWorldInfoFrame> actualEvent);
	void InvalidateInfoLineage(); // Reset invalidates current lookup, retaining immutable prior versions.
	void InfoDestroyed(const void* originalHandler); // Actual dtor only.
	std::shared_ptr<const NativeCUSTextureObject> InfoImage(const void* originalHandler) const;
	std::shared_ptr<const NativeWorldInfoFrame> InfoSource(const void* originalHandler) const;
private:
	struct Key {const void* owner;NativeTerrainImageRole role;int32_t x=-1,z=-1;};
	struct Less {bool operator()(const Key&,const Key&) const;};
	struct Slot {
		NativeTerrainImageKey source;bool borrowed=false;uint64_t writeRevision=0;
		// Only the original CreateGrassTex raw alias may write through. This is
		// never granted to borrowed Lua/UI/external publisher objects.
		std::optional<NativeTerrainImageKey> rawWriteThrough;
	};
	std::map<Key,Slot,Less> slots;
	struct InfoLineage {uint64_t currentMap=0,invalidatedMap=0;};
	std::map<const void*,InfoLineage,std::less<const void*>> infoOwners; // Actual handler instances, never a pixel cache.
	NativeTerrainImageSource images;
	NativeTextureRegistry* registry=nullptr;
	NativeCUSTextureHistory* history=nullptr;
	ui::NativeUIRecorder* recorder=nullptr;
	NativeTerrainImagePublicationServices services;
	uint64_t context=0;
	void Check() const;
	void Mirror();
	void RestoreBinding(const NativeCUSTextureBinding&);
	void Release(const Key&);
	NativeTerrainImageKey AllocateOwned(const void*,const void*,NativeTerrainImageRole,
		const NativeTextureAllocation&,const NativeTextureSampling&,const std::string&);
};
NativeTerrainImagePublication& OriginalNativeTerrainImagePublication();
} // namespace rendering
