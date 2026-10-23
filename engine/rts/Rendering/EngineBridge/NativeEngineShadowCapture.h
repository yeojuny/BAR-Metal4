/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelCapture.h"
#include "Rendering/Backend/NativeEngineShadowProgramSource.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/float3.h"
class CProjectile;
struct S3DModelPiece;
class CMatrix44f;
namespace rendering {
struct NativeEngineShadowCallbacks {
 std::function<bool(const CUnit*)> drawUnit;
 std::function<bool(const CFeature*)> drawFeature;
};
// Actual original shadow visitor only. It never scans main active objects or
// repeats ShouldDraw*/AddShadowMaterialObject after the real selection.
class NativeEngineShadowCapture {
public:
 NativeEngineShadowCapture(WorldFrameCapture&,NativeTextureContext&,ui::NativeUIRecorder&,NativeEngineModelClipState&,NativeEngineShadowProgramSource&);
 std::shared_ptr<const NativeEngineShadowFrame> CaptureDefault(uint64_t,const WorldCamera&,bool features,const NativeEngineShadowCallbacks&);
 // ORIGINAL DrawShadowPassImpl keeps legacy alpha/offset through its Lua
 // callin and disables them immediately before forced-legacy remainder.
 void AfterOriginalLua(bool modern);
 // Scope the ONE original CProjectileDrawer::DrawShadowOpaque invocation.
 std::shared_ptr<const NativeEngineShadowFrame> CaptureProjectiles(uint64_t,const WorldCamera&,const std::function<void()>&);
 void EnableProjectileProgram();void DisableProjectileProgram();
 void BindProjectileTexture(int type,int bin){Bind(type,bin);}
 void UnbindProjectileTexture(){Unbind();}
 void ProjectileModelType(int type,bool begin){ModelType(type,begin);}
 void DrawProjectileModel(const CProjectile*);
 void BeginFlyingPieces();void EndFlyingPieces();
 void DrawFlyingPiece(const S3DModelPiece*,const CMatrix44f&,uint32_t first,uint32_t count);
private:
 WorldFrameCapture& models;NativeTextureContext& textures;ui::NativeUIRecorder& recorder;NativeEngineModelClipState& clips;NativeEngineShadowProgramSource& programs;
 NativeEngineShadowFrame* frame=nullptr;const NativeEngineShadowCallbacks* callbacks=nullptr;
 void Bind(int modelType,int textureBin);void Unbind();void ModelType(int,bool);
 void SetPlane(unsigned,const std::array<float,4>&);void Polygon(bool);
 void Freeze(const CUnit*,bool);void Freeze(const CFeature*,bool);
 void DrawUnit(const CUnit*,bool);void DrawFeature(const CFeature*,bool);
 void DrawProjectilePiece(const S3DModelPiece*,const CMatrix44f&,bool shatter=false,uint32_t first=0,uint32_t count=0);
 void DrawProjectilePieceTree(const S3DModelPiece*,const float3&);
};
NativeEngineShadowCapture* CurrentNativeProjectileShadowCapture()noexcept;
}
