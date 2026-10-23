/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include <array>
#include <memory>
namespace rendering {
struct WorldTerrainHeights;
enum class NativeMapTextureRole : uint8_t { Normals, Height };
struct NativeMapTextureResource {
	virtual ~NativeMapTextureResource()=default;
	virtual bool Completed()const noexcept=0;
};
struct NativeMapTextureBacking final:NativeTextureBacking {
	NativeMapTextureRole role;
	uint64_t originalRevision=0;
	std::shared_ptr<const WorldTerrainHeights> heights;
	NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::Other;}
	void Fulfill(std::shared_ptr<const NativeMapTextureResource>)const;
	const std::shared_ptr<const NativeMapTextureResource>& Resource()const{return resource;}
private:
	mutable std::shared_ptr<const NativeMapTextureResource> resource;
};
struct NativeMapTextureFrame {
	uint64_t sourceDrawFrame=0;
	std::array<std::shared_ptr<const NativeCUSTextureObject>,2> images;
};
// Follows the real CSMFReadMap allocation/update/destruction, not widget or
// consumer lifetime. It shares the native context's sole name/history owner.
class NativeMapTexturePublication {
public:
	void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&);
	void Detach();
	void Create(const void* originalOwner,NativeMapTextureRole,uint32_t width,uint32_t height);
	void HeightUploaded(const void* originalOwner);
	void NormalsRendered(const void* originalOwner);
	void NormalOverride(const void* originalOwner,bool actualLuaOverride);
	void Destroy(const void* originalOwner);
	NativeTextureLookup Lookup(std::string_view originalName)const;
	std::shared_ptr<const NativeMapTextureFrame> Publish(std::shared_ptr<const WorldTerrainHeights>,uint64_t sourceDrawFrame);
	bool HasOriginalAllocation()const noexcept{return registry && names[0] && names[1];}
private:
	NativeTextureRegistry* registry=nullptr;NativeCUSTextureHistory* history=nullptr;
	uint64_t context=0;const void* owner=nullptr;
	std::array<uint64_t,2> names{},revisions{};
	bool normalOverride=false;
	void Check(const void*)const;
};
NativeMapTexturePublication& OriginalNativeMapTextures();
}
