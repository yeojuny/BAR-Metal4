/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <functional>
#include "Rendering/Backend/NativeEngineModelData.h"
#include "Rendering/Backend/NativeEngineModelProgramSource.h"
class CUnit;
class CFeature;
namespace rendering {
class WorldFrameCapture;
class NativeEngineModelState;
// Same original SInstanceData/uploader packing, reusable at the actual shadow
// submission. Both paths retain the same immutable publications, no reupload.
NativeEngineModelPosePublication CaptureNativeEngineModelPose(const CUnit*);
NativeEngineModelPosePublication CaptureNativeEngineModelPose(const CFeature*);
// Raw source state changes (model texture bins and per-object Lua draw
// callbacks) belong to the actual source service, never to GPU preparation.
struct NativeEngineModelSourceHooks {
 NativeEngineModelState* sourceState=nullptr;
 NativeEngineModelClipState* clipState=nullptr;
 // Must verify the actual selected shader after a callback. No ownerless
 // generic shader is allowed to fall through as an original default draw.
 std::function<void()> requireOriginalProgram;
 std::function<void()> disableOriginalProgram;
 std::function<std::shared_ptr<NativeEngineModelProgramSource::Program>(bool modern)> captureProgramEnable;
	std::function<void(int modelType,int textureBin)> bindModelTextures;
	std::function<std::array<NativeCUSTextureBinding,2>()> currentModelTextures;
	std::function<NativeEngineModelRaster()> currentRaster;
	std::function<void(int modelType,bool begin)> modelTypeScope;
	std::function<void(bool)> setWireframe;
	std::function<bool(const CUnit*)> drawUnitLua;
	std::function<bool(const CFeature*)> drawFeatureLua;
};
class NativeEngineModelCapture {
public:
	explicit NativeEngineModelCapture(WorldFrameCapture& world):world(world){}
	std::shared_ptr<NativeEngineModelPhase> BeforeLua(uint64_t,const WorldCamera&,bool feature,const NativeEngineModelSourceHooks&,bool drawReflection=false,bool drawRefraction=false);
	std::shared_ptr<NativeEngineModelPhase> BeforeAlphaLua(uint64_t,const WorldCamera&,bool feature,
		const NativeEngineModelSourceHooks&,std::optional<std::array<float,4>> alphaClipPlane={},bool drawReflection=false,bool drawRefraction=false);
	// Read the actual bins only after the one whole original CUS callback.
	// They are not silently replaced by an empty native list.
	void ObserveAfterLua(NativeEngineModelPhase&,bool deferredPass);
 // Original legacy material DrawBinObject has already selected the object,
 // run its actual callback and retained current source state. No visibility
 // re-selection, callback replay or second uploader is performed here.
 void EmitUnit(NativeEngineModelPhase&,const CUnit*,NativeEngineModelState&);
 void EmitFeature(NativeEngineModelPhase&,const CFeature*,NativeEngineModelState&);
private:
	WorldFrameCapture& world;
};
} // namespace rendering
