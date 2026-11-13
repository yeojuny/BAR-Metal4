/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <stdexcept>
#include "NativeTerrainSourceData.h"
#include "NativeTerrainMeshSnapshot.h"
#include "NativeWorldDrawData.h"
#include "WorldFrameData.h"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
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
struct NativeTerrainUniformSnapshot {
 uint64_t contextGeneration=0, originalProgramGeneration=0, revision=0;
 std::map<std::string,NativeTerrainUniformValue> values;
 NativeTerrainUniformSnapshot()=default;
 // Pointer slots must never point into a copied-from snapshot.
 NativeTerrainUniformSnapshot(const NativeTerrainUniformSnapshot& other):
  contextGeneration(other.contextGeneration),originalProgramGeneration(other.originalProgramGeneration),
  revision(other.revision),values(other.values){}
 NativeTerrainUniformSnapshot& operator=(const NativeTerrainUniformSnapshot& other){
  if(this==&other)return *this;
  auto copied=other.values; // Strong exception safety before invalidating current state.
  contextGeneration=other.contextGeneration;originalProgramGeneration=other.originalProgramGeneration;
  revision=other.revision;values=std::move(copied);InvalidateSlots();return *this;
 }
 void InvalidateSlots()noexcept{slotsReady=false;}
 void SealSlots()noexcept{
  if(slotsReady)return;
  slots.fill(nullptr);
  auto it=values.begin();
  for(size_t i=0;i<slots.size();++i){
   while(it!=values.end()&&it->first<NativeTerrainUniformNames[i])++it;
   if(it!=values.end()&&it->first==NativeTerrainUniformNames[i])slots[i]=&it->second;
  }
  slotsReady=true;
 }
 const NativeTerrainUniformValue* Find(NativeTerrainUniformSlot slot)const{
  const auto index=size_t(slot);
  if(index>=slots.size())return nullptr;
  if(slotsReady)return slots[index];
  const auto it=values.find(NativeTerrainUniformNames[index]);
  return it==values.end()?nullptr:&it->second;
 }
private:
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
