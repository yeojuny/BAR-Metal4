/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureRegistry.h"
#include "WorldFrameData.h"
#include <array>
#include <functional>
#include <map>
namespace rendering {
// Reuses the actual VFS-decoded image and mip bytes used by the native model
// cache. This is a source upload, not a texture chosen at a draw consumer.
struct NativeModelTextureBacking final:NativeTextureBacking {
 std::shared_ptr<const WorldTexture> pixels;
 NativeTextureBackingKind Kind()const noexcept override{return NativeTextureBackingKind::ColorImage;}
};
// The actual handler-selected immutable object pair at ONE CPU source event.
// Geometry stays in its existing WorldModel owner/cache across Reload.
struct NativeS3OTexturePair {
 uint64_t contextGeneration=0,sourceDrawFrame=0;
 unsigned originalTextureBin=0;
 std::array<std::shared_ptr<const NativeCUSTextureObject>,2> objects;
};
// Reads retained bytes only, never a current name/path/registry lookup.
std::shared_ptr<const WorldTexture> NativeS3OTexturePixels(const NativeS3OTexturePair&,unsigned slot);
class NativeS3OTextureSource {
public:
 using Loader=std::function<std::shared_ptr<const WorldTexture>(const std::string&,bool reload)>;
 struct Result {uint32_t name=0,width=0,height=0;bool invertAxis=false,invertAlpha=false,missingPrimary=false;};
 // The required mirror marks actual shared history in the same UI recorder;
 // it must not issue another history event or install a second observer.
 void Attach(NativeTextureRegistry&,NativeCUSTextureHistory&,Loader,std::function<uint64_t()> sourceFrame,
  std::function<void(unsigned)> syncObservedTextureBinding);
 void Detach();
 bool Attached()const noexcept{return registry!=nullptr;}
 Result Load(const std::string& originalTextureName,unsigned textureNumber,bool invertAxis,bool invertAlpha,bool preload);
 std::shared_ptr<const WorldTexture> Pixels(const std::string&)const;
 // Caller preserves original textureCache traversal; this is one real reload.
 void Reload(const std::string& originalTextureName);
 // Resolve one already-bound logical identity at the next CPU draw event.
 // Reload keeps identity; Delete/reuse or context loss must fail strictly.
 std::shared_ptr<const NativeCUSTextureObject> CurrentObject(uint64_t actualContext,uint64_t name,uint64_t identity)const;
 std::shared_ptr<const NativeS3OTexturePair> CapturePair(unsigned originalTextureBin,uint32_t primary,uint32_t secondary)const;
 // Called in the original material primary/secondary deletion order.
 void Delete(uint32_t originalName);
 void Kill();
private:
 struct Entry {uint64_t name=0;bool invertAxis=false,invertAlpha=false,dds=false,missingPrimary=false;std::shared_ptr<const WorldTexture> pixels;};
 std::map<std::string,Entry> entries;
 NativeTextureRegistry* registry=nullptr;NativeCUSTextureHistory* history=nullptr;
 Loader loader;std::function<uint64_t()> sourceFrame;std::function<void(unsigned)> syncObservedTextureBinding;uint64_t context=0;
 std::shared_ptr<const WorldTexture> LoadPixels(const std::string&,unsigned,bool,bool,bool dummy,bool reload)const;
 void Upload(const std::string&,Entry&);
 unsigned BoundUIUnits(uint64_t name)const;
 void SyncUIUnits(unsigned mask);
};
NativeS3OTextureSource& OriginalNativeS3OTextures();
}
