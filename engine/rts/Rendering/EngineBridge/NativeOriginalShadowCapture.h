/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineShadowCapture.h"
#include "NativeCUSSourceCapture.h"
#include "NativeOriginalShadowParticlesCapture.h"
#include "Rendering/Backend/NativeOriginalShadowData.h"
#include <span>
namespace rendering {
// One context-owned source. This object does not construct programs, change
// shadow selection, publish model buffers, submit work, or announce readiness.
class NativeOriginalShadowCapture {
public:
 NativeOriginalShadowCapture(NativeEngineShadowCapture&,NativeEngineShadowProgramSource&,
  NativeCUSSourceCapture&,NativeOriginalShadowParticlesCapture&,NativeTextureContext&,ui::NativeUIRecorder&);
 std::shared_ptr<const NativeOriginalShadowFrame> Capture(
  std::shared_ptr<const NativeShadowTextureFrame>,const WorldCamera&,
  const NativeEngineShadowCallbacks&,const std::function<void()>& actualDrawShadowPasses);
 // Existing ONE WorldClearHandler: Validate, RecordClear exactly once,
 // then Commit with that returned real source ordinal. No new observer.
 NativeShadowClearAcceptance ValidateClear(unsigned,const ScreenClearValues&,const NativeWorldDrawState&,std::optional<ScreenScissor>)const;
 void CommitClear(const NativeShadowClearAcceptance&,uint64_t actualRecordedSequence);
 // Called ONLY by the original ShadowHandler::DrawShadowPasses native branch.
 void DrawOriginalPasses();
 // Original DrawShadowPass / Basic / ROAM writer positions only.
 void BeginTerrainDraw();
 void EnableTerrainProgram(float borderMinHeight,const std::array<float,4>& mapSize);
 void DisableTerrainProgram();
 void SelectTerrainSquare(int x,int z);
 void SubmitTerrain(const void* positions,size_t vertexCount,size_t stride,
  std::span<const uint32_t> indices,bool indexed);
 // Exact LuaOpenGL Enable/Disable/ResetDrawWorldShadow program writer seams.
 void EnableLuaWorldShadow(bool safeMode);void DisableLuaWorldShadow(bool safeMode);void ResetLuaWorldShadow(bool safeMode);
private:
 NativeEngineShadowCapture& engine;NativeEngineShadowProgramSource& programs;
 NativeOriginalShadowParticlesCapture& particles;
 NativeCUSSourceCapture& cus;NativeTextureContext& textures;ui::NativeUIRecorder& recorder;
 std::optional<NativeShadowClearContract> clearContract;
 NativeOriginalShadowFrame* frame=nullptr;const NativeEngineShadowCallbacks* callbacks=nullptr;
 NativeTerrainShadowFrame* terrainFrame=nullptr;
 bool terrainProgram=false;
 std::optional<std::array<int32_t,2>> terrainSquare;
 std::array<float,4> terrainMapSize{};float terrainBorderMinHeight=0;
 void DrawTerrain();
 void DrawModels(bool feature);void ResetOriginalShadowMatrices();
};
NativeOriginalShadowCapture& CurrentOriginalNativeShadowCapture();
NativeOriginalShadowCapture* CurrentNativeTerrainShadowCapture()noexcept;
}
