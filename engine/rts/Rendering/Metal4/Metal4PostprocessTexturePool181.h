/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePostprocessData.h"
#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace rendering::metal4 {
// This pool serves only full fixed framebuffer copies. Every allocation has
// the original 2D/private/ShaderRead|RenderTarget descriptor, one level/sample/
// slice, and default remaining descriptor fields. No views or heap aliasing.
struct PostprocessTextureKey181 {
	MTLPixelFormat format=MTLPixelFormatInvalid;
	NSUInteger width=0,height=0;
	bool operator==(const PostprocessTextureKey181&) const = default;
};
struct PostprocessTextureStorage181 {
	__strong id<MTLTexture> texture=nil;
	// Exact immutable contents owner, not the reusable logical image identity.
	// Future CPU readers retain this version even before their GPU preparation.
	std::weak_ptr<const NativePostprocessVersion> version;
};
class PostprocessTexturePool181 {
public:
	static constexpr std::size_t ByteLimit=64u*1024u*1024u;
	static constexpr std::size_t EntryLimit=32;
	PostprocessTexturePool181()=default;
	PostprocessTexturePool181(const PostprocessTexturePool181&)=delete;
	PostprocessTexturePool181& operator=(const PostprocessTexturePool181&)=delete;
	// Runtime request serialization covers Acquire/Clear. Tokens have no weak
	// acquisition path. Every reading/writing request retains the shared token
	// directly in RuntimeFrameResources, through all feedback and poison owners.
	template<class Factory>
	std::shared_ptr<PostprocessTextureStorage181> Acquire(id<MTLDevice> device,
		const PostprocessTextureKey181& key,const std::shared_ptr<const NativePostprocessVersion>& version,Factory&& factory)
	{
		Require(device && version,"Postprocess texture reuse lacks its device/version owner");
		if(owner!=device) {Clear();owner=device;}
		++requests;
		for(auto& entry:entries) {
			if(entry.key!=key || entry.slot.use_count()!=1 || !entry.slot->version.expired()) continue;
			entry.slot->version=version;++reused;return entry.slot;
		}
		// A full/busy/oversized cache never restricts the original allocation.
		auto texture=factory();
		Require(texture!=nil,"Cannot allocate original DOF immutable attachment");
		++created;
		auto slot=std::make_shared<PostprocessTextureStorage181>();slot->texture=texture;slot->version=version;
		const bool supported=(key.format==MTLPixelFormatRGBA8Unorm || key.format==MTLPixelFormatDepth32Float) &&
			key.width && key.height && key.width<=std::numeric_limits<std::size_t>::max()/4/key.height;
		const auto logical=supported?std::size_t(key.width)*key.height*4:std::numeric_limits<std::size_t>::max();
		const auto footprint=std::max(logical,std::size_t(texture.allocatedSize));
		// The factory preserves every original descriptor field. Check the actual
		// allocation before admitting it so another family cannot enter this pool.
		const bool exact=supported && texture.device==device && texture.pixelFormat==key.format &&
			texture.width==key.width && texture.height==key.height && texture.textureType==MTLTextureType2D &&
			texture.depth==1 && texture.mipmapLevelCount==1 && texture.sampleCount==1 && texture.arrayLength==1 &&
			texture.storageMode==MTLStorageModePrivate && texture.usage==(MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget);
		if(exact && entries.size()<EntryLimit && footprint<=ByteLimit-bytes) {
			entries.push_back({key,slot,footprint});bytes+=footprint;
			peakBytes=std::max(peakBytes,bytes);peakEntries=std::max(peakEntries,entries.size());
		} else ++uncached;
		return slot;
	}
	void Clear() noexcept {entries.clear();bytes=0;owner=nil;}
	std::size_t CachedBytes() const noexcept {return bytes;}
	std::size_t CachedCount() const noexcept {return entries.size();}
	std::uint64_t requests=0,reused=0,created=0,uncached=0;
	std::size_t peakBytes=0,peakEntries=0;
private:
	struct Entry {PostprocessTextureKey181 key;std::shared_ptr<PostprocessTextureStorage181> slot;std::size_t bytes=0;};
	// Bounded linear lookup: arbitrary extents cannot create an unbounded key map.
	std::vector<Entry> entries;
	__strong id<MTLDevice> owner=nil;
	std::size_t bytes=0;
};
}
