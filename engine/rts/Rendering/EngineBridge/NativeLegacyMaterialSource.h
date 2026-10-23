/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelCapture.h"
#include "Rendering/Backend/NativeEngineModelState.h"
#include "Rendering/Backend/NativeLegacyMaterialData.h"
#include "Rendering/Backend/NativeDisplayListReference.h"
#include "Lua/LuaObjectMaterial.h"
#include <span>
class LuaMaterial;class LuaMatShader;class LuaMatTexture;class CSolidObject;
namespace rendering {
struct NativeLegacyMaterialTexture {
 bool observed=false;uint64_t name=0;unsigned originalTarget=0;
};
struct NativeLegacyMaterialServices {
 NativeEngineModelSourceHooks engine;
 std::function<NativeEngineModelTextureNames()> commonTextures;
 // Resolve only the original already-parsed descriptor, at original Bind.
 // An observed zero is different from an unconnected/dead native provider.
 std::function<NativeLegacyMaterialTexture(const LuaMatTexture&)> resolveTexture;
 // Original Unbind reads target only; it must not load/reparse/GetTextureID.
 std::function<unsigned(const LuaMatTexture&)> textureTarget;
 // Real typed program-location sinks only. Missing/unknown locations fail;
 // RawExec calls these at the original setter, before immutable draw capture.
 std::function<void(unsigned,int,size_t,bool,std::span<const float>)> uniformFloat;
 std::function<void(unsigned,int,size_t,std::span<const int>)> uniformInt;
};
class NativeLegacyMaterialSource {
public:
 NativeLegacyMaterialSource(WorldFrameCapture&,NativeTextureContext&,ui::NativeUIRecorder&,NativeEngineModelClipState&);
 ~NativeLegacyMaterialSource();
 void Capture(NativeEngineModelPhase&,bool deferred,const NativeLegacyMaterialServices&);
 // Called only by the narrow native branches at the ORIGINAL source sites.
 void BeginBins(LuaObjType,LuaMatType,bool deferred);
 void EndBins();
 void ExecuteMaterial(const LuaMaterial&,const LuaMaterial& previous,bool deferred);
 void ExecuteShader(const LuaMatShader&,const LuaMatShader& previous,bool deferred);
 void SetObjectTeamColor(const CSolidObject*,float alpha);
 void DrawBinObject(const CSolidObject*,LuaObjType,const LuaObjectLODMaterial*,bool applyTransform,bool noLuaCall);
 void UniformFloat(unsigned,int,size_t,bool,std::span<const float>);
 void UniformInt(unsigned,int,size_t,std::span<const int>);
private:
 NativeEngineModelCapture models;NativeTextureContext& textures;ui::NativeUIRecorder& recorder;NativeEngineModelClipState& clips;
 NativeEngineModelPhase* phase=nullptr;const NativeLegacyMaterialServices* services=nullptr;
 std::unique_ptr<NativeEngineModelState> engine;
 bool deferred=false,binsActive=false,binsFinished=false;
 int engineModelType=-1;ui::NativeUIMatrixMode savedMode=ui::NativeUIMatrixMode::ModelView;
 NativeEngineModelClipState savedClips;
 void Abort()noexcept;
 void RequirePhase(bool)const;
 void CallLOD(const ui::NativeDisplayListReference&,unsigned rawGLList);
 void SetEnable(unsigned unit,NativeCUSTextureTarget,bool);
 void Bind(unsigned,const LuaMatTexture&);
 void Unbind(unsigned,const LuaMatTexture&);
 void ShadowSampler(unsigned,bool comparison);
};
// Functional source execution scope, never a capability or shader-ready flag.
NativeLegacyMaterialSource* CurrentNativeLegacyMaterialSource()noexcept;
} // namespace rendering
