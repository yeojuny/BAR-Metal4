/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeTextureBacking.h"
#include <array>
#include <cstdint>
#include <memory>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace rendering {
struct WorldTexture;
enum class NativeCUSTextureTarget : uint8_t { Image2D, Image3D, Cube, Image2DMultisample, Image2DArray, Count };
enum class NativeCUSBindingKnowledge : uint8_t { Unobserved, DefaultObject, BoundObject };
enum class NativeTextureStorageKnowledge : uint8_t { Unobserved, Empty, AllocatedUndefined, Published };
struct NativeTextureAllocation {
 uint32_t width=0,height=0,depth=0,levels=0,samples=0,originalInternalFormat=0;
 bool operator==(const NativeTextureAllocation&)const=default;
};
struct NativeCUSTextureObject {
 uint64_t identity=0; // Actual context object generation, not a path hash.
 uint64_t contextGeneration=0,name=0,version=0;
 NativeCUSTextureTarget target=NativeCUSTextureTarget::Image2D;
 std::string source;
 std::shared_ptr<const WorldTexture> image; // Nil for a real external engine image token.
 std::shared_ptr<const NativeTextureBacking> backing;
 NativeTextureSampling sampling;
 NativeTextureStorageKnowledge storage=NativeTextureStorageKnowledge::Unobserved;
 NativeTextureAllocation allocation;
};
struct NativeCUSTextureBinding {
 NativeCUSBindingKnowledge knowledge=NativeCUSBindingKnowledge::Unobserved;
 std::shared_ptr<const NativeCUSTextureObject> object;
 bool enabled=false,enableObserved=false;
 // Object zero alone is insufficient: storage writes can make it complete.
 bool defaultStorageKnownEmpty=false;
 NativeTextureSampling defaultSampling; // Exact object0 parameters at this draw.
};
enum class NativeCUSObjectKind : uint8_t { Unit, Feature, UnitDefinition };
struct NativeCUSSubmittedObject {NativeCUSObjectKind kind;int32_t id;};
struct NativeCUSDrawTextureSnapshot {
 uint64_t contextGeneration=0,eventSequence=0,sourceDrawFrame=0;
 unsigned originalPass=0;
 std::string originalShader,originalUniformBin;
 std::vector<NativeCUSSubmittedObject> originalSubmissionOrder;
 std::array<NativeCUSTextureBinding,12> samplers; // Actual source slots0..11, target-aware, including envLUT.
 std::array<NativeCUSTextureBinding,3> wreck; // Original sampler2D slots3,4,5.
};
// Inactive until actual engine-stage and original Lua-family event producers
// are connected. Never guesses a prior image, treats false as missing, or
// turns a default/unobserved object into a fabricated black/blank texture.
class NativeCUSTextureHistory {
public:
 static constexpr unsigned Units=32; // CGlobalRendering::MAX_TEXTURE_UNITS.
 void ResetForNewContext(); // Only a real context creation establishes defaults.
 void CloseContext() noexcept; // Drops observations, without inventing another context.
 using ObjectResolver=std::function<std::shared_ptr<const NativeCUSTextureObject>(std::shared_ptr<const NativeCUSTextureObject>)>;
 // Shared registry resolves current contents only when a draw/state query is
 // captured. Already returned snapshots keep their exact immutable version.
 void SetObjectResolvers(ObjectResolver current,ObjectResolver restoreSaved);
 void ObjectStateChanged();
 void DeletedObject(uint64_t identity); // Current bindings only; prior draws survive.
 void ActiveTexture(unsigned unit);
 std::optional<unsigned> ActiveUnit()const noexcept{return activeUnit;}
 void Bind(unsigned unit,std::shared_ptr<const NativeCUSTextureObject>);
 void BindDefault(unsigned unit,NativeCUSTextureTarget);
 // Call when original image/storage operations modify the default object.
 // No storage at a real new context is the only implicit empty observation.
 void ObserveDefaultStorage(NativeCUSTextureTarget,bool knownEmpty);
 void ObserveDefaultSampling(NativeCUSTextureTarget,const NativeTextureSampling&);
 void Enable(unsigned unit,NativeCUSTextureTarget,bool enabled);
 void LuaEnable2D(unsigned unit,bool enabled); // gl.Texture(unit,true/false).
 void FailedImageLookup(unsigned unit); // Original ParseTextureImage=false: no state change.
 // The first Lua argument is zero when omitted. Original Texture(0, ...) does
 // not change the active unit; nonzero requests restore unit zero afterwards.
 bool LuaTexture(unsigned requestedUnit,bool enabled);
 // A nil parsed image means the actual ParseTextureImage returned false.
 // It never means texture object zero or permission to supply a dummy image.
 bool LuaTexture(unsigned requestedUnit,std::shared_ptr<const NativeCUSTextureObject> parsedImage);
 // ParseTextureImage may succeed while LuaMatTexture::GetTextureID is zero.
 // Its Bind then does nothing and gl.Texture returns true, not BindDefault.
 bool LuaTextureLookup(unsigned requestedUnit,bool parsed,std::shared_ptr<const NativeCUSTextureObject> image,
  const std::function<void(unsigned actualUnit)>& afterOriginalBind = {});
 using LuaImageParse=std::function<std::pair<bool,std::shared_ptr<const NativeCUSTextureObject>>() >;
 bool LuaTextureLookup(unsigned requestedUnit,const LuaImageParse&,
  const std::function<void(unsigned actualUnit)>& afterOriginalBind = {});
 void PushAttributes(bool textureBindings,bool enables);
 void PopAttributes();
 NativeCUSTextureBinding Binding(unsigned unit,NativeCUSTextureTarget) const;
 NativeCUSDrawTextureSnapshot Submit(uint64_t drawFrame,unsigned originalPass,
  std::string shader,std::string uniformBin,std::span<const NativeCUSSubmittedObject>);
 uint64_t Sequence() const noexcept{return sequence;}
private:
 using TargetState=std::array<NativeCUSTextureBinding,size_t(NativeCUSTextureTarget::Count)>;
 using State=std::array<TargetState,Units>;
 struct Saved {State state;std::optional<unsigned>active;bool bindings,enables;
  std::array<NativeTextureSampling,size_t(NativeCUSTextureTarget::Count)> defaultSampling;};
 State state{};
 std::array<bool,size_t(NativeCUSTextureTarget::Count)> defaultStorageKnownEmpty{};
 std::array<NativeTextureSampling,size_t(NativeCUSTextureTarget::Count)> defaultSampling{};
 std::optional<unsigned>activeUnit;std::vector<Saved> stack;uint64_t context=0,sequence=0;
 ObjectResolver resolveObject,restoreObject;
 void Advance();
 unsigned BeginLuaTexture(unsigned requestedUnit);
};
} // namespace rendering
