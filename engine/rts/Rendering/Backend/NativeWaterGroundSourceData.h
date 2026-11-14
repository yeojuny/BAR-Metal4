/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <stdexcept>
#include "NativeTerrainSourceData.h"
#include "NativeTerrainMeshSnapshot.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace rendering {
struct NativeCUSDeferredPhase;

// Written at the real original uniform locations, including link defaults and
// original Lua writes. An absent entry remains unobserved. Matrices retain the
// source shape and transpose argument; consumers must apply those semantics.
struct NativeTerrainUniformValue {
	uint32_t rows=1, columns=1, count=1;
	bool transpose=false;
	std::variant<std::vector<float>,std::vector<int32_t>,std::vector<uint32_t>> values;
};
// Fixed original terrain ABI slots. Names are resolved at source publication,
// not repeatedly in each GPU draw. Unknown/inactive source names remain in values.
enum class NativeTerrainUniformSlot : size_t {
 blendNormalsTex, borderMinHeight, cameraPos, colorTex, depthTex, detailTex, detailsTex, diffuseTex, groundAmbientColor, groundDiffuseColor, groundShadowDensity, groundSpecularColor, groundSpecularExponent, heightMapTex, infoTex, infoTexGen, infoTexIntensityMul, lightDir, lightEmissionTex, mapHeights, mapSize, normalTexGen, normalsTex, parallaxHeightTex, shadingTex, shadowColorTex, shadowMat, shadowTex, skyReflectModTex, skyReflectTex, specularTex, specularTexGen, splatDetailNormalTex1, splatDetailNormalTex2, splatDetailNormalTex3, splatDetailNormalTex4, splatDetailTex, splatDistrTex, splatTexMults, splatTexScales, texSquare, waterAbsorbColor, waterBaseColor, waterMinColor, Count
};
inline constexpr std::array<const char*,size_t(NativeTerrainUniformSlot::Count)> NativeTerrainUniformNames={
 "blendNormalsTex", "borderMinHeight", "cameraPos", "colorTex", "depthTex", "detailTex", "detailsTex", "diffuseTex", "groundAmbientColor", "groundDiffuseColor", "groundShadowDensity", "groundSpecularColor", "groundSpecularExponent", "heightMapTex", "infoTex", "infoTexGen", "infoTexIntensityMul", "lightDir", "lightEmissionTex", "mapHeights", "mapSize", "normalTexGen", "normalsTex", "parallaxHeightTex", "shadingTex", "shadowColorTex", "shadowMat", "shadowTex", "skyReflectModTex", "skyReflectTex", "specularTex", "specularTexGen", "splatDetailNormalTex1", "splatDetailNormalTex2", "splatDetailNormalTex3", "splatDetailNormalTex4", "splatDetailTex", "splatDistrTex", "splatTexMults", "splatTexScales", "texSquare", "waterAbsorbColor", "waterBaseColor", "waterMinColor"
};
// Only successful source conversions are cached. Malformed values retain the
// original reader path and its original consumption-time diagnostics.
struct NativeTerrainTypedVector {
 union { std::array<float,4> floats; std::array<int32_t,4> integers; };
 uint8_t kind=0, components=0;
 NativeTerrainTypedVector():floats{}{}
};
struct NativeTerrainUniformSnapshot {
 uint64_t contextGeneration=0, originalProgramGeneration=0, revision=0;
 using Table=std::map<std::string,NativeTerrainUniformValue>;
 // Materialized writer/import storage. Consumers use Find/FindName; an overlay
 // deliberately leaves this empty. ValuesCopy is for serialization/debug only.
 Table values;
 NativeTerrainUniformSnapshot()=default;
 // Pointer slots must never point into a copied-from snapshot.
 NativeTerrainUniformSnapshot(const NativeTerrainUniformSnapshot& other):
  contextGeneration(other.contextGeneration),originalProgramGeneration(other.originalProgramGeneration),
  revision(other.revision),values(other.ValuesCopy()){}
 NativeTerrainUniformSnapshot& operator=(const NativeTerrainUniformSnapshot& other){
  if(this==&other)return *this;
  auto copied=other.ValuesCopy(); // Strong exception safety before invalidating current state.
  contextGeneration=other.contextGeneration;originalProgramGeneration=other.originalProgramGeneration;
  revision=other.revision;values=std::move(copied);commonBase.reset();squareOverride.reset();InvalidateSlots();return *this;
 }
 // A single immutable common root, never a chain of square snapshots.
 static std::shared_ptr<NativeTerrainUniformSnapshot> WithTexSquare(
  const std::shared_ptr<const NativeTerrainUniformSnapshot>& previous,NativeTerrainUniformValue value){
  auto next=std::make_shared<NativeTerrainUniformSnapshot>();
  next->contextGeneration=previous->contextGeneration;next->originalProgramGeneration=previous->originalProgramGeneration;
  next->revision=previous->revision;
  next->commonBase=previous->commonBase?previous->commonBase:previous;
  next->squareOverride=std::move(value);
  next->ReuseTypedBaseAfterSingleWrite(*previous,"texSquare");
  return next;
 }
 bool HasTexSquareOverride()const noexcept{return bool(commonBase);}
 void RewriteTexSquare(NativeTerrainUniformValue value){
  // Writer-only: this snapshot has not escaped to an observed publication.
  squareOverride=std::move(value);InvalidateWrittenSlot("texSquare");
 }
 const NativeTerrainUniformValue* FindName(const std::string& name)const{
  if(commonBase){if(name=="texSquare")return &*squareOverride;return commonBase->FindName(name);}
  const auto it=values.find(name);return it==values.end()?nullptr:&it->second;
 }
 template<class Visitor> void ForEachValue(Visitor&& visit)const{
  if(!commonBase){for(const auto& [name,value]:values)visit(name,value);return;}
  bool emitted=false;
  const std::string_view squareName="texSquare";
  for(const auto& [name,value]:commonBase->values){
   if(!emitted&&name>=squareName){visit(squareName,*squareOverride);emitted=true;}
   if(name!=squareName)visit(name,value);
  }
  if(!emitted)visit(squareName,*squareOverride);
 }
 Table ValuesCopy()const{
  if(!commonBase)return values;
  auto result=commonBase->values;result.insert_or_assign("texSquare",*squareOverride);return result;
 }
 bool MaterializeForWrite(){
  if(!commonBase)return false;
  auto copied=ValuesCopy(); // No observable mutation if allocation fails.
  values=std::move(copied);commonBase.reset();squareOverride.reset();slotsReady=false;return true;
 }
 void InvalidateSlots()noexcept{slotsReady=false;typedClassified.fill(false);shadowMatrixReady=false;}
 // Writer-only transport after an exact map copy and one named write. Unlike
 // pointer slots, typed values own their bytes and can retain unchanged fields.
 void ReuseTypedBaseAfterSingleWrite(const NativeTerrainUniformSnapshot& previous,const std::string& changed)noexcept{
  typedVectors=previous.typedVectors;typedClassified=previous.typedClassified;
  shadowMatrix=previous.shadowMatrix;shadowMatrixReady=previous.shadowMatrixReady;
  InvalidateWrittenSlot(changed);
 }
 void InvalidateWrittenSlot(const std::string& changed)noexcept{
  slotsReady=false;
  for(size_t i=0;i<NativeTerrainUniformNames.size();++i)if(changed==NativeTerrainUniformNames[i]){
   typedClassified[i]=false;
   if(i==size_t(NativeTerrainUniformSlot::shadowMat))shadowMatrixReady=false;
   return;
  }
 }
 void SealSlots()noexcept{
  if(slotsReady)return;
  slots.fill(nullptr);
  const auto& common=commonBase?commonBase->values:values;
  auto it=common.begin();
  for(size_t i=0;i<slots.size();++i){
   while(it!=common.end()&&it->first<NativeTerrainUniformNames[i])++it;
   if(it!=common.end()&&it->first==NativeTerrainUniformNames[i])slots[i]=&it->second;
  }
  if(commonBase)slots[size_t(NativeTerrainUniformSlot::texSquare)]=&*squareOverride;
  // Classification cannot throw or report an error at publication. All values
  // not represented here still undergo the original reader validation later.
  for(size_t i=0;i<slots.size();++i){
   if(typedClassified[i])continue;
   typedClassified[i]=true;
   auto& packed=typedVectors[i];packed.kind=0;packed.components=0;
   const auto* value=slots[i];
   if(i==size_t(NativeTerrainUniformSlot::shadowMat)){
    shadowMatrixReady=false;
    if(value&&value->rows==4&&value->columns==4&&value->count==1){
     if(const auto* floats=std::get_if<std::vector<float>>(&value->values);floats&&floats->size()==16&&
        std::all_of(floats->begin(),floats->end(),[](float x){return (std::bit_cast<uint32_t>(x)&0x7f800000u)!=0x7f800000u;})){
      for(size_t c=0;c<4;++c)for(size_t r=0;r<4;++r)shadowMatrix[c*4+r]=(*floats)[value->transpose?r*4+c:c*4+r];
      shadowMatrixReady=true;
     }
    }
   }
   if(!value||value->count!=1||value->columns!=1||value->rows<1||value->rows>4||value->transpose)continue;
   const auto n=value->rows;
   if(const auto* floats=std::get_if<std::vector<float>>(&value->values);floats&&floats->size()==n){
    if(!std::all_of(floats->begin(),floats->end(),[](float x){return (std::bit_cast<uint32_t>(x)&0x7f800000u)!=0x7f800000u;}))continue;
    packed.floats={};for(size_t j=0;j<n;++j)packed.floats[j]=(*floats)[j];packed.kind=1;
   }else if(const auto* ints=std::get_if<std::vector<int32_t>>(&value->values);ints&&ints->size()==n){
    packed.integers={};for(size_t j=0;j<n;++j)packed.integers[j]=(*ints)[j];packed.kind=2;
   }
   if(packed.kind)packed.components=uint8_t(n);
  }
  slotsReady=true;
 }
 const NativeTerrainUniformValue* Find(NativeTerrainUniformSlot slot)const{
  const auto index=size_t(slot);
  if(index>=slots.size())return nullptr;
  if(slotsReady)return slots[index];
  return FindName(NativeTerrainUniformNames[index]);
 }
 const std::array<float,4>* PackedFloat(NativeTerrainUniformSlot slot,unsigned components)const noexcept{
  const size_t i=size_t(slot);if(!slotsReady||i>=typedVectors.size())return nullptr;
  const auto& v=typedVectors[i];return v.kind==1&&v.components==components?&v.floats:nullptr;
 }
 const std::array<int32_t,4>* PackedInt(NativeTerrainUniformSlot slot,unsigned components)const noexcept{
  const size_t i=size_t(slot);if(!slotsReady||i>=typedVectors.size())return nullptr;
  const auto& v=typedVectors[i];return v.kind==2&&v.components==components?&v.integers:nullptr;
 }
 const std::array<float,16>* PackedMatrix(NativeTerrainUniformSlot slot)const noexcept{
  return slotsReady&&shadowMatrixReady&&slot==NativeTerrainUniformSlot::shadowMat?&shadowMatrix:nullptr;
 }
private:
 std::shared_ptr<const NativeTerrainUniformSnapshot> commonBase;
 std::optional<NativeTerrainUniformValue> squareOverride;
 std::array<NativeTerrainTypedVector,size_t(NativeTerrainUniformSlot::Count)> typedVectors{};
 std::array<bool,size_t(NativeTerrainUniformSlot::Count)> typedClassified{};
 std::array<float,16> shadowMatrix{};
 bool shadowMatrixReady=false;
 std::array<const NativeTerrainUniformValue*,size_t(NativeTerrainUniformSlot::Count)> slots{};
 bool slotsReady=false;
};
// A draw retains the exact immutable source definition instead of copying its
// strings/compile defines. This is not a validation bypass or a readiness token.
struct NativeTerrainProgramSnapshot {
	std::shared_ptr<const NativeTerrainProgramDefinition> definition;
	const NativeTerrainProgramIdentity& Identity() const {
		if(!definition)throw std::runtime_error("Original terrain snapshot has no source definition");
		return definition->identity;
	}
	const std::shared_ptr<const NativeTerrainProgramConsumer>& Consumer() const {
		if(!definition)throw std::runtime_error("Original terrain snapshot has no source definition");
		return definition->consumer;
	}
	std::shared_ptr<const NativeTerrainUniformSnapshot> uniforms;
};

enum class NativeTerrainMeshSource : uint8_t { Basic, ROAM, Composite };
enum class NativeGroundCallback : uint8_t { PreForward, PostForward, PreDeferred, Deferred, PostDeferred };
struct NativeGroundSourceSpan {
	uint64_t begin=0, end=0; // Exclusive bounds in the existing common draw order.
	bool Contains(uint64_t sequence) const noexcept {return begin<sequence && sequence<end;}
};

// Copied at the existing Basic/ROAM actual submit, like the MAP shadow
// producer. This is the original selected mesh, not a generated SMT tile grid.
// Local coordinates and exact triangle order survive to the same frame/lease.
// Geometry storage is shared with the original ROAM shadow producer.

struct NativeTerrainSquareDraw {
	uint64_t sequence=0;
	uint32_t squareX=0, squareZ=0;
	NativeTerrainMeshSource mesh=NativeTerrainMeshSource::Basic;
	uint64_t originalIndexCount=0; // Provenance of the actual nonempty submission.
	bool border=false,deferred=false,composite=false;
	NativeTerrainMeshGeometry geometry; // Basic, border and composite fallback.
 std::shared_ptr<const NativeTerrainMeshSnapshot> meshSnapshot;
 const NativeTerrainMeshGeometry& Geometry()const noexcept{return meshSnapshot?meshSnapshot->Geometry():geometry;}
	NativeTerrainProgramSnapshot program;
	NativeTerrainImageSnapshot images;
	NativeWorldDrawState world;
	ScreenDrawState surface;
	std::array<float,16> modelView{}, projection{};
	std::array<bool,3> clipEnabled{};
	std::array<std::array<float,4>,3> clipEyeEquations{};
	bool fogEnabled=false;
	std::array<float,4> fogColor{}, fogParameters{};
};

// One original Draw in its existing unsafe main, IBL or water stage. A missing frame means
// the outer reflection-mode predicate omitted Draw. A present frame with
// drawEntered=false records an original drawGround/void-water early return.
// Source state remains live in the common context; there is no exit-state
// reconstruction or resetting state between callbacks and original models.
struct NativeWaterGroundSourceFrame {
	uint64_t contextGeneration=0, sourceDrawFrame=0;
	NativeWorldDrawStage stage=NativeWorldDrawStage::WaterReflection;
	NativeGroundSourceSpan source;
	bool drawEntered=false, forwardEntered=false, borderEntered=false, deferredEntered=false;
	std::optional<NativeGroundSourceSpan> preForward, postForward, deferred, preDeferred, deferredCallback, postDeferred;
 std::optional<WorldCamera> sourceView;
 std::shared_ptr<const NativeCUSDeferredPhase> mapMRT;
 bool deferredClearOnly=false,combined=false,compositeEntered=false;
	std::vector<NativeTerrainSquareDraw> squares;
	// Original DrawGroundDeferred submissions at their own ordinals. This is
	// independent of the later DrawWorldPreUnit map-edge packet and state update.
	std::vector<std::shared_ptr<const mapedge::DrawPacket>> mapEdges;
};
// The existing common ordinal stream owns generic geometry and clears. This
// packet retains one actual caller/camera and its same-invocation typed owners.
struct NativeWorldSkyResource;
struct NativeTerrainSceneSourceFrame {
 uint64_t sourceDrawFrame=0,initialClearSequence=0;
 NativeWorldDrawStage stage=NativeWorldDrawStage::Ground;
 WorldCamera viewCamera;
 uint32_t reflectionFace=0;
 bool skyOnly=false,groundInvoked=false;
 NativeWorldDrawState debugCubeState;
 std::shared_ptr<const NativeWorldSkyResource> debugCubeSky; // Actual debug branch; only reflectionFace is drawn.
 std::shared_ptr<const NativeWaterGroundSourceFrame> ground;
 std::shared_ptr<const NativeCUSSourceFrame> cus;
 std::shared_ptr<const NativeWaterLuaFrame> lua;
};
} // namespace rendering
