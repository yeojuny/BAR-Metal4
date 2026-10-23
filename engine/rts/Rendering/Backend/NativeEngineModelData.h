/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "WorldFrameData.h"
#include "NativeCUSTextureHistory.h"
#include "ScreenDrawList.h"
#include "NativeWorldDrawData.h"
#include "NativeCUSPublishedData.h"
#include "NativeCUSUniformData.h"
#include "NativeModelLightData.h"
#include <array>
#include <memory>
#include <optional>
#include <vector>
#include <stdexcept>

namespace rendering {
class NativeLegacyMaterialExecution;
enum class NativeEngineModelShader : uint8_t { Legacy, GL4 };
enum class NativeEngineModelOwner : uint8_t { Unit, Feature, TemporaryUnit };

// Captured at the actual source draw, not selected again by the consumer.
// Geometry stays in the same map cache. Each source draw freezes the actual
// post-callback local pose; live GL4 pose comes from retained original SSBOs.
struct NativeEngineModelObject {
	NativeEngineModelOwner owner=NativeEngineModelOwner::Unit;
	int id=-1,team=-1,modelType=-1,textureBin=-1;
	std::shared_ptr<const WorldModel> model;
	WorldMatrix world{};
	std::vector<WorldPiecePose> pieces;
	std::array<float,4> teamColor{1,1,1,1};
};
struct NativeEngineModelRaster {
	bool observed=false;
	std::optional<ScreenViewport> viewport;
	std::optional<ScreenScissor> scissor;
};
// The typed original program owner, sampled at Enable, then changed only by
// original uniform writers. Values are not reconstructed from a later unit.
struct NativeEngineModelProgramState {
 bool observed=false,modern=false,shadows=false;
 int matrixMode=0,cameraMode=0,shadingMode=0;
 float teamAlpha=1,shadowDensity=0;
 std::array<std::array<float,4>,3> modelClipPlanes{};
 std::array<float,4> team{},nano{},colorMultiplier{},alphaControl{};
 std::array<float,4> sunDirection{},sunAmbient{},sunDiffuse{},sunSpecular{};
 std::array<float,4> fogColor{},fogParameters{};
 WorldMatrix shadowMatrix{},staticModelMatrix{};
 std::shared_ptr<const NativeModelLightFrame> lights;
};
// Shared original GL clip state belongs to the selected context, not a shader.
// Context initialization and original glEnable/glClipPlane writers establish it.
struct NativeEngineModelClipState {
 uint64_t contextGeneration=0;
 bool observed=false;
 void InitializeContextDefaults(uint64_t actualContextGeneration){
  if(observed||!actualContextGeneration)throw std::logic_error("Original model clip context initialization is missing or repeated");
  contextGeneration=actualContextGeneration;enabled={false,false,false};eyeEquations={};observed=true;
 }
 void Reset()noexcept{contextGeneration=0;observed=false;enabled={};eyeEquations={};}
 void SetEnabled(unsigned plane,bool value){
  if(!observed||plane>=3)throw std::logic_error("Original clip enable has no actual shared context owner");enabled[plane]=value;
 }
 void SetEyeEquation(unsigned plane,const std::array<float,4>& value){
  if(!observed||plane>=3)throw std::logic_error("Original clip equation has no actual shared context owner");eyeEquations[plane]=value;
 }
 std::array<bool,3> enabled{};
 std::array<std::array<float,4>,3> eyeEquations{};
};
struct NativeEngineModelPosePublication {
	bool staticModel=false,arrayModel=false;
 std::array<uint32_t,4> instance{};
 std::shared_ptr<const NativeCUSPublishedBuffer> transforms,modelUniforms;
 std::shared_ptr<const NativeCUSUniformPublication> engineUniforms;
};
struct NativeEngineModelDraw {
	uint64_t sequence=0; // The actual default draw, after any original per-object Lua callback.
	NativeEngineModelRaster raster;
 bool originalStateObserved=false;
 WorldMatrix sourceModelView{},sourceProjection{};
 NativeWorldDrawState sourceWorld;
 ScreenDrawState sourceSurface;
 NativeEngineModelProgramState program;
 std::shared_ptr<const void> compiledOriginalProgram;
 std::array<bool,3> clipEnabled{};
 // Legacy equations are stored in eye coordinates at glClipPlane WRITE time;
 // modern build equations remain model-space; water clip2 is world-space.
 std::array<std::array<float,4>,3> storedClipPlanes{};
 std::array<NativeCUSTextureBinding,4> lightingTextures;
 NativeEngineModelPosePublication pose;
	std::shared_ptr<const NativeEngineModelObject> object;
	std::array<NativeCUSTextureBinding,2> textures;
	NativeEngineModelShader shader=NativeEngineModelShader::Legacy;
	std::array<float,4> nanoColor{1,1,1,0};
	std::array<float,4> colorMultiplier{1,1,1,1};
	std::array<float,4> alphaControl{0.5f,1,0,0};
	std::array<std::array<float,4>,2> modelClipPlanes{};
	bool wireframe=false,polygonOffset=false;
	float polygonOffsetFactor=0,polygonOffsetUnits=0;
};
struct NativeEngineBuildInput {
	NativeEngineModelShader shader=NativeEngineModelShader::Legacy;
	float height=0,progress=0;
	int simulationFrame=0;
	std::array<float,3> nanoColor{};
	bool amdHacks=false;
};
// Original GL4 and GLSL have different flat/fill clipping. This deliberately
// preserves those source branches rather than normalizing their appearance.
std::vector<NativeEngineModelDraw> NativeEngineBuildStages(const NativeEngineBuildInput&);
void ValidateNativeEngineModelDraw(const NativeEngineModelDraw&);

struct NativeEngineLegacyBinObservation {
	int shaderType=0;
	uint32_t shaderName=0,preList=0,postList=0;
	bool deferredPass=false,validForPass=false,drawCall=false,useCamera=true;
	std::vector<int> objects;
};
struct NativeEngineModelPhase {
	uint64_t sourceDrawFrame=0;
	WorldCamera viewCamera;
	bool feature=false,engineObserved=false,legacyObserved=false;
	bool drawReflection=false,drawRefraction=false,alpha=false;
	// World-space plane3 from the actual CWorldDrawer alpha half; no water means absent.
	std::optional<std::array<float,4>> alphaClipPlane;
	// Legacy construction ends its wire stage with GL_FILL without saving
	// GL_POLYGON_BIT. Later source objects and DrawOpaque*Lua inherit that.
	bool engineWireframe=false;
	std::vector<NativeEngineModelDraw> beforeLua,afterLua;
	// An observed nonempty legacy bin is a real remaining producer requirement.
	// Its effects may not be erased even when it currently has no objects.
	std::vector<NativeEngineLegacyBinObservation> legacyBins;
 std::shared_ptr<const NativeLegacyMaterialExecution> legacyExecution;
};
} // namespace rendering
