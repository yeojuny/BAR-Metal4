/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSNoiseVolume.h"
#include "NativeTextureRegistry.h"
#include <map>

namespace rendering {
struct NativeNamedVolumeResource {virtual ~NativeNamedVolumeResource()=default;};
struct NativeNamedVolumeBacking final:NativeTextureBacking {
	std::shared_ptr<const NativeCUSNoiseVolume> pixels;
	mutable std::shared_ptr<NativeNamedVolumeResource> resource;
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};

// CNamedTextures owns names, persistence and reload order. This leaf supplies
// the selected original CBitmap DDS3D upload, with the same registry/history
// used by ordinary named images. It does not maintain another path-name cache.
class NativeNamedVolumeSource {
public:
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&,std::function<uint64_t()> sourceFrame);
	void Detach(); // After original named-texture Kill and retained GPU retirement.
	bool Attached()const noexcept{return registry!=nullptr;}
	bool Supports(const std::string& originalPath)const noexcept;
	uint32_t Upload(const std::string& sourceName,uint32_t existingName,
		std::shared_ptr<const NativeCUSNoiseVolume>,unsigned originalMinFilter,float originalLodBias);
	bool Owns(uint32_t)const;
	void Delete(uint32_t);
	void ForgetNames()noexcept; // Original CNamedTextures::Init clears its cache without deletes.
private:
	NativeTextureRegistry* registry=nullptr;
	NativeCUSTextureHistory* history=nullptr;
	std::function<uint64_t()> sourceFrame;
	uint64_t context=0;
	std::map<uint32_t,std::shared_ptr<const NativeNamedVolumeBacking>> names;
};
NativeNamedVolumeSource& OriginalNativeNamedVolumes();
} // namespace rendering
