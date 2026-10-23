/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <functional>
#include "NativeTextureContext.h"
#include "NativeUIRecorder.h"
#include "WorldFrameData.h"
#include "NativeEngineModelData.h"
#include "NativeEngineModelProgramSource.h"

namespace rendering {
// These are original logical names obtained at EnableTexturesCommon, not GPU
// resource IDs or aliases selected later by a consumer. Zero is meaningful.
struct NativeEngineModelTextureNames {
	bool shadowsLoaded=false;
	uint64_t shadowDepth=0,shadowColor=0,reflection=0,specular=0;
};

// Exact opaque-model GL writer boundary shared with original Lua callbacks.
// This object owns only the Setup/Reset scope it acquired. It does not start
// a Lua world stage, reset the camera, or regenerate any texture.
class NativeEngineModelState {
public:
	NativeEngineModelState(NativeTextureContext&,ui::NativeUIRecorder&,NativeEngineModelClipState&,std::function<void()> actualProgramDisable,std::function<void()> actualProgramRequire);
	~NativeEngineModelState();
	void Begin(bool legacy,bool wireframe,const WorldMatrix& actualView,
		const NativeEngineModelTextureNames&,std::function<std::shared_ptr<NativeEngineModelProgramSource::Program>()> actualProgramEnable,bool alphaPass=false);
	void BindModelTextures(uint64_t texture1,uint64_t texture2);
	void PushModelType(bool original3DO);
	void PopModelType(bool original3DO);
	void SetWireframe(bool);
 bool Wireframe()const{return recorder.CurrentWorldState().frontPolygonMode==NativePolygonMode::Line;}
	std::array<NativeCUSTextureBinding,2> CurrentModelTextures()const;
 // Original DrawUnitTrans pushes once before any per-object callback. End
 // pops the CURRENT matrix mode, exactly as glPopMatrix does in the original.
 void BeginObject(const WorldMatrix&);
 void BeginModernObject();
 void BeginUntransformedObject();
 void EndObject();
 void SetTeamColor(const std::array<float,4>&);
 void SetTeamAlpha(float);
 void SetColorMultiplier(const std::array<float,4>&);
 int SetMatrixMode(int);
 void SetStaticModelMatrix(const WorldMatrix&);
 bool Legacy()const noexcept{return legacy;}
 void SetNanoColor(const std::array<float,4>&);
 void SetClipEnabled(unsigned,bool);
 void SetClipPlane(unsigned,const std::array<float,4>&);
 void BeginBuild(bool actualAMDHacks);
 void EnterBuildStage(const NativeEngineModelDraw&,size_t stage,bool lower);
 void LeaveBuildStage(const NativeEngineModelDraw&,size_t stage);
 void EndBuild();
 void CaptureDefaultDraw(NativeEngineModelDraw&)const;
 void End();
	void Abort()noexcept;
	bool Active()const noexcept{return attributes;}
private:
	NativeTextureContext& context;
	ui::NativeUIRecorder& recorder;
 NativeEngineModelClipState& clips;
 std::function<void()> programDisable,programRequire;
 std::array<bool,3> inheritedClipEnable{};
	NativeEngineModelTextureNames names;
	bool waterClipScope=false;
	bool legacy=false,alphaPass=false,attributes=false,projection=false,modelView=false,object=false,build=false,amdHacks=false,programScope=false,lowerStageClosed=false,objectMatrixPushed=false;
 std::shared_ptr<NativeEngineModelProgramSource::Program> program;

	void ActiveUnit(unsigned);
	void Enable(unsigned,NativeCUSTextureTarget,bool);
	void Bind(unsigned,uint64_t,NativeCUSTextureTarget);
	void ShadowSampling(bool comparison);
	void PopMatrices();
 void CloseLowerBuildScope();
 void SetCameraMode(int);
};
} // namespace rendering
