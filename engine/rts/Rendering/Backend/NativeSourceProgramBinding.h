/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>

namespace rendering {
class NativeTerrainProgramConsumer;
struct NativeTerrainProgramPublication;
// Functional current-program source state. The engine and private CUS adapter
// share this binding, exactly as their original glUseProgram writes did. It is
// not a capability/readiness flag and never compiles or substitutes a shader.
class NativeSourceProgramBinding {
public:
 enum class Kind { Unbound, EngineModel, CUS, UnsupportedGL, Terrain, UnsupportedTerrain, DeferredLights, UnitStencil, Outline, Distortion, UnitGlass };
 struct Selection {
  Kind kind=Kind::Unbound;
  std::shared_ptr<const void> owner;
  std::uint32_t originalGLName=0;
  std::shared_ptr<const NativeTerrainProgramConsumer> terrain;
  std::shared_ptr<const NativeTerrainProgramPublication> terrainSource;
 };
 void BindEngine(std::shared_ptr<const void> compiledOriginal){Bind(Kind::EngineModel,std::move(compiledOriginal));}
 void BindCUS(std::shared_ptr<const void> actualProgram){Bind(Kind::CUS,std::move(actualProgram));}
 void BindDistortion(std::shared_ptr<const void> actualProgram){Bind(Kind::Distortion,std::move(actualProgram));}
 void BindDeferredLights(std::shared_ptr<const void> actualProgram){Bind(Kind::DeferredLights,std::move(actualProgram));}
 void BindUnitStencil(std::shared_ptr<const void> actualProgram){Bind(Kind::UnitStencil,std::move(actualProgram));}
 void BindUnitGlass(std::shared_ptr<const void> actualProgram){Bind(Kind::UnitGlass,std::move(actualProgram));}
 void BindOutline(std::shared_ptr<const void> actualProgram){Bind(Kind::Outline,std::move(actualProgram));}
 void BindTerrain(std::shared_ptr<const NativeTerrainProgramConsumer> actualProgram){
  if(!actualProgram)throw std::logic_error("Original terrain source binding has no actual compiled terrain owner");
  Unbind();current.kind=Kind::Terrain;current.terrain=std::move(actualProgram);
 }
 void BindTerrainSource(std::shared_ptr<const NativeTerrainProgramPublication>);
 void BindOriginalGL(std::uint32_t name){Unbind();if(name){current.kind=Kind::UnsupportedGL;current.originalGLName=name;}}
 void Unbind()noexcept{pending={};current={};}
 // Immediate inspection only: never retain this reference across a bind,
 // restore, callback, or source mutation. It is not a saved source snapshot.
 const Selection& InspectCurrent()const noexcept{return current;}
 Kind CurrentKind()const noexcept{return current.kind;}
 // Explicit state-preservation boundary for nested ActiveShader callbacks.
 // Current remains the owning observation API for existing draw consumers.
 // InspectCurrent's terrain publication is provenance only while pending;
 // immutable uniform contents are observed exclusively through Current/Save.
 void MarkTerrainPending(void* writer,std::weak_ptr<const void> lifetime,void (*observe)(void*),
  std::shared_ptr<const NativeTerrainProgramConsumer> consumer) noexcept {
  current.kind=Kind::Terrain;current.owner.reset();current.originalGLName=0;current.terrain=std::move(consumer);
  pending={writer,std::move(lifetime),observe};
 }
 bool HasPendingTerrain(const void* writer)const noexcept{return pending.writer==writer&&pending.observe;}
 void SealTerrainSource(const void* writer,std::shared_ptr<const NativeTerrainProgramPublication> source){
  if(!HasPendingTerrain(writer)||!source)throw std::runtime_error("Terrain seal lost its pending source selection");
  current.terrainSource=std::move(source);pending={};
 }
 Selection Current()const{
  if(pending.observe){
   const auto lifetime=pending.lifetime.lock();
   if(!lifetime)throw std::runtime_error("Pending terrain source writer no longer exists");
   pending.observe(pending.writer);
   if(pending.observe)throw std::runtime_error("Terrain observation did not seal its pending writer");
  }
  return current;
 }
 Selection SaveCurrent()const{return Current();}
 void Restore(Selection saved){pending={};current=std::move(saved);}
 bool IsCUS(const std::shared_ptr<const void>& actualProgram)const noexcept{
  return current.kind==Kind::CUS&&actualProgram&&current.owner.get()==actualProgram.get();
 }
 bool IsDistortion(const std::shared_ptr<const void>& actualProgram)const noexcept{return current.kind==Kind::Distortion&&actualProgram&&current.owner.get()==actualProgram.get();}
 bool IsDeferredLights(const std::shared_ptr<const void>& actualProgram)const noexcept{
  return current.kind==Kind::DeferredLights&&actualProgram&&current.owner.get()==actualProgram.get();
 }
 bool IsUnitStencil(const std::shared_ptr<const void>& actualProgram)const noexcept{
  return current.kind==Kind::UnitStencil&&actualProgram&&current.owner.get()==actualProgram.get();
 }
 bool IsOutline(const std::shared_ptr<const void>& actualProgram)const noexcept{
  return current.kind==Kind::Outline&&actualProgram&&current.owner.get()==actualProgram.get();
 }
 std::shared_ptr<const void> EngineOwner()const{
  if(current.kind!=Kind::EngineModel||!current.owner)
   throw std::runtime_error("Original default model draw has an unbound or unsupported actual source program");
  return current.owner;
 }
 std::shared_ptr<const NativeTerrainProgramConsumer> TerrainOwner()const{
  if(current.kind!=Kind::Terrain||!current.terrain)
   throw std::runtime_error("Original terrain draw has an unbound or unsupported actual source program");
  return current.terrain;
 }
private:
 void Bind(Kind kind,std::shared_ptr<const void> owner){
  if(!owner)throw std::logic_error("Original source program binding has no actual compiled owner");
  pending={};current={kind,std::move(owner),0};
 }
 struct Pending {void* writer=nullptr;std::weak_ptr<const void> lifetime;void (*observe)(void*)=nullptr;};
 Pending pending;
 Selection current;
};
// There is one actual native GL-source context, owned by SceneBridge Session.
inline NativeSourceProgramBinding& OriginalNativeSourceProgramBinding(){
 static NativeSourceProgramBinding binding;return binding;
}
} // namespace rendering
