/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUITextureObserver.h"
#include "NativeTextureRegistry.h"
#include <map>
namespace rendering {struct NativePostprocessPublication;struct NativeTerrainBorrowedSquareWrite;}
namespace rendering::ui {
// Portable immutable owner of an actual screen image/version. GPU consumers
// must resolve its retained RTT/copy producer before sampling; bytes are never
// replaced with constants when that producer has not run.
struct NativeScreenTextureBacking final:NativeTextureBacking {
 std::shared_ptr<const ScreenTexture> image;
 NativeTextureBackingKind Kind()const noexcept override;
};
class NativeUITextureHistory final:public NativeUITextureObserver {
public:
 NativeUITextureHistory(NativeCUSTextureHistory&,NativeTextureRegistry&,std::function<uint64_t()> actualProducerFrame);
 ~NativeUITextureHistory() override;
 void ActiveTexture(unsigned)override;
 unsigned LuaEnable(unsigned,bool)override;
 std::shared_ptr<const NativeCUSTextureObject> CaptureBind(const std::shared_ptr<NativeUITextureBinding>&,const std::shared_ptr<const ScreenTexture>&)override;
 unsigned LuaBind(unsigned,std::shared_ptr<const NativeCUSTextureObject>)override;
 unsigned LuaLookup(unsigned,const std::function<NativeTextureLookup()>&)override;
 ObservedImage BoundImage(unsigned)override;
 ObservedImage DescribeBinding(const NativeCUSTextureBinding&);
 static ScreenSampler DescribeSampler(const NativeCUSTextureBinding&);
 // Shared explicit shaders must use their existing incomplete-texture mask,
 // rather than sampling a truncated chain or a UI argument placeholder.
 static bool SamplingIncomplete(const NativeCUSTextureBinding&);
 bool Enabled(unsigned)const override;
 unsigned LuaNoBind(unsigned,bool)override;
 void Push(bool,bool)override;void Pop()override;void Reset2D()override;
 void Create(const std::shared_ptr<NativeUITextureBinding>&,const std::string&,unsigned)override;
 void Delete(const std::shared_ptr<NativeUITextureBinding>&)override;
 void Params(const std::shared_ptr<NativeUITextureBinding>&,ScreenSampler)override;
 NativeUITexturePublication Publish(const std::shared_ptr<NativeUITextureBinding>&)override;
 void CancelPublication(const NativeUITexturePublication&)noexcept override;
 void Named(const std::string&,const std::shared_ptr<const ScreenTexture>&,ScreenSampler,bool,bool flatDDS=false)override;
 void DeleteNamed(const std::string&)override;
 // The original CNamedTextures::Update site calls this after compiling lists.
 // Registration alone does not prematurely publish deferred upload contents.
 void UpdateNamed()override;
 void Reset(); // Actual context owner only, after all families detach.
 // Engine-owned UnitDef images register at their real original producer.
 // A source-proven GetTextureID()==0 registers zero: parsed ID0 preserves
 // the inherited binding, unlike an actual explicit glBindTexture(...,0).
 void RegisterExternal(const std::shared_ptr<NativeUITextureBinding>&,uint64_t actualName);
 // Resolves only a logical name registered by this actual UI/named owner.
 // An arbitrary raw GL name or an unobserved name is never accepted.
 std::shared_ptr<const NativeCUSTextureObject> ResolveObservedName(uint64_t actualName);
	// Executed compressed TexImage on the existing original Lua destination,
	// immediately outside a list or at replay using its captured input bytes.
	// This updates its actual ordinary/shared cell and the registry together.
	void PublishBorrowedSquare(const NativeTerrainBorrowedSquareWrite&);
 // Original map setters use CNamedTextures::GetInfo(forceLoad=false).
 // This queries the real registered named owner without loading a new image.
 NativeTextureLookup LookupObservedNamed(const std::string&) const;
 using ExternalImageDescriptorProvider=std::function<std::shared_ptr<const ScreenTexture>(const NativeCUSTextureBinding&)>;
 void SetExternalImageDescriptorProvider(ExternalImageDescriptorProvider provider){externalImages=std::move(provider);}
 // Original UnitDefImage's owned load/delete, including borrowed Lua names.
 uint64_t UploadBuildPicture(const std::shared_ptr<NativeUITextureBinding>&,const std::string&,bool flatDDS);
 void ReleaseBuildPicture(const std::shared_ptr<NativeUITextureBinding>&);
 // Private source families share the same logical names and immutable image
 // generations as public CopyToTexture/RTT, without a second binding writer.
 void CreatePostprocess(const std::shared_ptr<NativePostprocessBinding>&,const std::string& source);
 void DeletePostprocess(const std::shared_ptr<NativePostprocessBinding>&);
 unsigned BindPostprocess(unsigned,const std::shared_ptr<NativePostprocessBinding>&);
 std::shared_ptr<NativePostprocessPublication> PublishPostprocess(const std::shared_ptr<NativePostprocessBinding>&,const std::shared_ptr<const NativePostprocessVersion>&,uint64_t sourceDrawFrame);
private:
 struct Entry{uint64_t name=0;std::shared_ptr<NativeUITextureBinding> binding;std::shared_ptr<const ScreenTexture> image;ScreenSampler sampler;bool deferred=false;bool flatDDS=false;std::shared_ptr<const NativeCUSTextureObject> declared;bool ownsStorage=false;};
 NativeCUSTextureHistory& history;NativeTextureRegistry& registry;uint64_t context=0;
 std::function<uint64_t()> sourceFrame;
 ExternalImageDescriptorProvider externalImages;
 std::shared_ptr<const int> publicationLifetime=std::make_shared<const int>(0);
 std::map<const NativeUITextureBinding*,Entry> bindings;
 std::map<std::string,Entry> named;
 std::vector<std::string> pendingNames;
 void CheckContext()const;
 Entry& Find(const std::shared_ptr<NativeUITextureBinding>&);
 Entry* FindPostprocess(const std::shared_ptr<NativePostprocessBinding>&);
 void PublishImage(Entry&,std::shared_ptr<const ScreenTexture>,ScreenSampler);
 void UploadNamed(Entry&);
 static NativeTextureSampling Sampling(ScreenSampler);
 static NativeTextureAllocation Allocation(const ScreenTexture&,unsigned format=0);
};
} // namespace rendering::ui
