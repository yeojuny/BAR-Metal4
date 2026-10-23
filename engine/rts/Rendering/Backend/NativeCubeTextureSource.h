/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include <atomic>
namespace rendering {
enum class NativeCubeTextureKind {Specular,Reflection,SkyReflection};
struct NativeCubeTextureResource {virtual ~NativeCubeTextureResource()=default;};
struct NativeCubeTextureBacking final:NativeTextureBacking {
	NativeCubeTextureKind kind=NativeCubeTextureKind::Reflection;
	uint32_t size=0,levels=1;
	std::shared_ptr<const NativeCubeTextureResource> resource;
	std::array<std::shared_ptr<const std::vector<uint8_t>>,6> specularFaces;
	// Retained only through the real copy/retirement dependency. Releasing it
	// at completion prevents a persistent cube's versions retaining all frames.
	mutable std::shared_ptr<const NativeCubeTextureBacking> prior;
	uint32_t sourceFace=0;
	bool initialMipmaps=false,regenerateMipmaps=false,requiresFaceDraw=false;
	mutable std::atomic<uint64_t> dependenciesSubmission{0},encodedSubmission{0};
	mutable std::atomic<bool> completed{false};
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
struct NativeCubeTextureServices {
	// Must allocate real native cube and original reflection depth storage.
	std::function<std::shared_ptr<const NativeCubeTextureResource>(NativeCubeTextureKind,uint32_t,uint32_t)> allocate;
	std::function<uint64_t()> sourceDrawFrame;
};
struct NativeCubeReflectionUpdate {
	std::shared_ptr<const NativeCUSTextureObject> previous,current;
	std::shared_ptr<const NativeCubeTextureBacking> backing;
	std::shared_ptr<const NativeCUSTextureObject> mipPrevious,mipCurrent;
	std::shared_ptr<const NativeCubeTextureBacking> mipBacking;
	uint32_t face=0;
	bool skyOnly=false;
};
class NativeCubeTextureSource {
public:
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&,NativeCubeTextureServices);
	void Detach(); // After CubeMapHandler::Free and source/GPU retirement.
	bool Attached()const noexcept{return registry!=nullptr;}
	uint32_t Create(NativeCubeTextureKind,uint32_t size,bool mipmaps);
	void Delete(uint32_t);
	void Bind(uint32_t);
	void GenerateInitialMipmaps(uint32_t);
	void SpecularWrite(uint32_t,unsigned face,unsigned y,unsigned rows,std::span<const uint8_t>);
	NativeCubeReflectionUpdate AdvanceReflection(uint32_t reflection,uint32_t skyReflection,unsigned originalFace,bool regenerateMipmaps);
	const NativeCubeReflectionUpdate& CurrentReflectionUpdate()const{return update;}
	std::shared_ptr<const NativeCubeTextureBacking> Image(uint32_t)const;
	void CancelReflection()noexcept;
private:
	NativeTextureRegistry* registry=nullptr;NativeCUSTextureHistory* history=nullptr;
	NativeCubeTextureServices services;
	NativeCubeReflectionUpdate update;
	std::shared_ptr<NativeCubeTextureBacking> Next(uint32_t,NativeTextureContent);
};
NativeCubeTextureSource& OriginalNativeCubeTextures();
} // namespace rendering
