/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineShadowCapture.h"
#include "Rendering/WorldFrameCapture.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Rendering/Textures/3DOTextureHandler.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Game/Camera.h"
#include "Game/Game.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/Feature.h"
#include "System/Matrix44f.h"
#include "Rendering/Models/3DModelPiece.hpp"
#include "Rendering/Models/3DModelVAO.hpp"
#include "System/MathConstants.h"
#include "Sim/Projectiles/Projectile.h"
#include "Sim/Projectiles/PieceProjectile.h"
#include "Sim/Projectiles/WeaponProjectiles/WeaponProjectile.h"
#include "System/EventHandler.h"
#include <algorithm>
#undef camera
namespace rendering {
namespace {
thread_local NativeEngineShadowCapture* projectileSource=nullptr;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
WorldMatrix Matrix(const CMatrix44f& value){WorldMatrix out;std::copy_n(value.m,16,out.begin());return out;}
}
NativeEngineShadowCapture* CurrentNativeProjectileShadowCapture()noexcept{return projectileSource;}
NativeEngineShadowCapture::NativeEngineShadowCapture(WorldFrameCapture& m,NativeTextureContext& t,ui::NativeUIRecorder& r,NativeEngineModelClipState& c,NativeEngineShadowProgramSource& p):models(m),textures(t),recorder(r),clips(c),programs(p){}
void NativeEngineShadowCapture::Bind(int type,int bin){
 Check(type>=0&&type<MODELTYPE_CNT,"Original shadow helper model type is invalid");
 const auto* material=textureHandlerS3O.GetTexture(bin);
 const auto name=type==MODELTYPE_3DO?textureHandler3DO.GetAtlasTex2ID():(material?material->tex2:0);
 Check(name&&textures.Registry().Find(name),"Original shadow helper texture has no actual source owner");
 textures.History().ActiveTexture(0);textures.History().Enable(0,NativeCUSTextureTarget::Image2D,true);recorder.SyncObservedTextureBinding(0);
 textures.Registry().Bind(0,name,NativeCUSTextureTarget::Image2D);recorder.SyncObservedTextureBinding(0);
}
void NativeEngineShadowCapture::Unbind(){
 // Original helper unbinds/disables the CURRENT unit before ActiveTexture0.
 const auto unit=textures.History().ActiveUnit();Check(bool(unit),"Original shadow helper active unit is unobserved");
 textures.Registry().Bind(*unit,0,NativeCUSTextureTarget::Image2D);if(*unit<2)recorder.SyncObservedTextureBinding(*unit);
 textures.History().Enable(*unit,NativeCUSTextureTarget::Image2D,false);if(*unit<2)recorder.SyncObservedTextureBinding(*unit);textures.History().ActiveTexture(0);
}
void NativeEngineShadowCapture::ModelType(int type,bool begin){if(type==MODELTYPE_3DO)recorder.UpdateWorldState([begin](auto& state){state.cullEnabled=!begin;},false);}
void NativeEngineShadowCapture::Polygon(bool wire){recorder.UpdateWorldState([wire](auto& state){state.frontPolygonMode=state.backPolygonMode=wire?NativePolygonMode::Line:NativePolygonMode::Fill;},false);}
void NativeEngineShadowCapture::SetPlane(unsigned index,const std::array<float,4>& value){
 auto program=programs.Current();if(program->modern){program->modelClipPlanes.at(index)=value;return;}
 CMatrix44f matrix;auto current=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);std::copy(current.begin(),current.end(),matrix.m);bool valid=false;auto inverse=matrix.Invert(&valid);Check(valid,"Original shadow glClipPlane has a singular actual model-view");
 std::array<float,4> eye{};for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)eye[row]+=inverse.m[row*4+col]*value[col];clips.SetEyeEquation(index,eye);
}
void NativeEngineShadowCapture::Freeze(const CUnit* unit,bool modern){
 auto program=programs.Current();Check(program->modern==modern&&frame,"Original shadow unit default has another selected program/source phase");Check(clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration()&&!clips.enabled[2],"Original shadow default draw has unobserved context or unsupported active clip plane2");NativeEngineShadowDraw draw;draw.modern=modern;draw.sequence=NextNativeWorldDrawSequence();draw.compiledOriginal=program->compiledOriginal;draw.alphaControl=program->alphaControl;
 draw.modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);for(size_t plane=0;plane<draw.clipEnabled.size();++plane){draw.clipEnabled[plane]=clips.enabled[plane];draw.clipPlanes[plane]=modern?program->modelClipPlanes[plane]:clips.eyeEquations[plane];}draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;draw.raster={true,draw.surface.viewport,draw.surface.scissor};draw.alphaTexture=textures.History().Binding(0,NativeCUSTextureTarget::Image2D);
 draw.object=models.CaptureEngineUnit(*unit,modern,draw.modelView,recorder.CurrentState().color);if(modern)draw.pose=CaptureNativeEngineModelPose(unit);frame->draws.push_back(std::move(draw));
}
void NativeEngineShadowCapture::Freeze(const CFeature* feature,bool modern){
 auto program=programs.Current();Check(program->modern==modern&&frame,"Original shadow feature default has another selected program/source phase");Check(clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration()&&!clips.enabled[2],"Original shadow default draw has unobserved context or unsupported active clip plane2");NativeEngineShadowDraw draw;draw.modern=modern;draw.sequence=NextNativeWorldDrawSequence();draw.compiledOriginal=program->compiledOriginal;draw.alphaControl=program->alphaControl;
 draw.modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);for(size_t plane=0;plane<draw.clipEnabled.size();++plane){draw.clipEnabled[plane]=clips.enabled[plane];draw.clipPlanes[plane]=modern?program->modelClipPlanes[plane]:clips.eyeEquations[plane];}draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;draw.raster={true,draw.surface.viewport,draw.surface.scissor};draw.alphaTexture=textures.History().Binding(0,NativeCUSTextureTarget::Image2D);
 draw.object=models.CaptureEngineFeature(*feature,modern,draw.modelView,recorder.CurrentState().color);if(modern)draw.pose=CaptureNativeEngineModelPose(feature);frame->draws.push_back(std::move(draw));
}
void NativeEngineShadowCapture::DrawUnit(const CUnit* unit,bool modern){
 // Legacy original DrawUnitTrans pushes/multiplies ONCE around all stages.
 if(!modern){recorder.PushMatrix();recorder.MultMatrix(Matrix(unit->GetTransformMatrix()));}
 const auto emit=[&]{if(modern||!unit->luaDraw||!callbacks->drawUnit(unit))Freeze(unit,modern);};
 if(!unit->beingBuilt||!unit->unitDef->showNanoFrame)emit();
 else{
  const float height=unit->model->CalcDrawHeight(),progress=unit->buildProgress;
  const std::array<std::array<float,4>,2> upper{{{0,-1,0,height*(progress*3)},{0,-1,0,height*(progress*3-1)}}};
  const std::array<std::array<float,4>,2> lower{{{0,1,0,-height*(progress*10-9)},{0,1,0,-height*(progress*3-2)}}};
  recorder.PushAttrib(modern?ui::Polygon:ui::Current);clips.SetEnabled(0,true);clips.SetEnabled(1,true);
  const auto planes=[&](unsigned stage){if(!modern){recorder.PushMatrix();recorder.LoadMatrix(ui::IdentityMatrix());}SetPlane(0,upper[stage]);SetPlane(1,lower[stage]);if(!modern)recorder.PopMatrix();};
  if(!modern&&globalRendering->amdHacks){clips.SetEnabled(0,false);clips.SetEnabled(1,false);}else planes(0);
  Polygon(true);emit();Polygon(false);
  if(!modern&&globalRendering->amdHacks){clips.SetEnabled(0,true);clips.SetEnabled(1,true);}
  if(progress>1.f/3){planes(1);emit();}
  if(modern){SetPlane(0,{0,0,0,1});SetPlane(1,{0,0,0,1});}clips.SetEnabled(1,false);clips.SetEnabled(0,false);
  if(progress>2.f/3)emit();recorder.PopAttrib();
 }
 if(!modern)recorder.PopMatrix();
}
void NativeEngineShadowCapture::DrawFeature(const CFeature* feature,bool modern){
 if(!modern){recorder.PushMatrix();recorder.MultMatrix(Matrix(feature->GetTransformMatrixRef()));}
 if(modern||!feature->luaDraw||!callbacks->drawFeature(feature))Freeze(feature,modern);
 if(!modern)recorder.PopMatrix();
}
std::shared_ptr<const NativeEngineShadowFrame> NativeEngineShadowCapture::CaptureDefault(uint64_t sourceDrawFrame,const WorldCamera& drawingCamera,bool features,const NativeEngineShadowCallbacks& input){
 Check(!frame&&programs.Constructed()&&globalRendering&&sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&game&&game->GetDrawMode()==CGame::gameShadowDraw&&shadowHandler.InShadowPass()&&CCamera::GetActive()&&CCamera::GetActive()->GetCamType()==CCamera::CAMTYPE_SHADOW&&recorder.IsWorldStage(),"Original default shadow source is outside its actual production boundary");
 Check(textures.Initialized()&&textures.UIAttached()&&clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration()&&input.drawUnit&&input.drawFeature,"Original shadow source context/clip/callback owner is unconnected");
 const bool modern=(features?featureDrawer->NativeImplementation():unitDrawer->NativeImplementation())==ModelDrawerTypes::MODEL_DRAWER_GL4;
 auto output=std::make_shared<NativeEngineShadowFrame>();output->sourceDrawFrame=sourceDrawFrame;output->feature=features;output->camera=drawingCamera;frame=output.get();callbacks=&input;
 try{
  if(!modern){recorder.Color({1,1,1,1});recorder.UpdateWorldState([](auto& s){s.offsetFactor=s.offsetUnits=1;s.polygonOffset=true;},false);recorder.SetAlphaTest({true,ScreenAlphaCompare::Greater,.5f});}
  programs.Enable(modern);
  auto bind=[this](int type,int bin){Bind(type,bin);};auto unbind=[this]{Unbind();};auto type=[this](int model,bool begin){ModelType(model,begin);};
  if(features)featureDrawer->VisitNativeShadowEngine(bind,[this](const CFeature* f,bool modern){DrawFeature(f,modern);},type,unbind);
  else unitDrawer->VisitNativeShadowEngine(bind,[this](const CUnit* u,bool modern){DrawUnit(u,modern);},type,unbind);
  programs.Disable();frame=nullptr;callbacks=nullptr;return output;
 }catch(...){frame=nullptr;callbacks=nullptr;throw;}
}
void NativeEngineShadowCapture::AfterOriginalLua(bool modern){if(!modern){auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);recorder.UpdateWorldState([](auto& s){s.polygonOffset=false;},false);}}

std::shared_ptr<const NativeEngineShadowFrame> NativeEngineShadowCapture::CaptureProjectiles(uint64_t drawFrame,const WorldCamera& drawing,const std::function<void()>& actualDraw)
{
 Check(!frame&&!projectileSource&&actualDraw&&globalRendering&&drawFrame==uint64_t(globalRendering->drawFrame)&&game&&game->GetDrawMode()==CGame::gameShadowDraw&&shadowHandler.InShadowPass()&&CCamera::GetActive()&&CCamera::GetActive()->GetCamType()==CCamera::CAMTYPE_SHADOW&&recorder.IsWorldStage(),"Original projectile shadow capture lost its actual source boundary");
 auto output=std::make_shared<NativeEngineShadowFrame>();output->sourceDrawFrame=drawFrame;output->camera=drawing;output->projectile=true;
 frame=output.get();projectileSource=this;
 try{actualDraw();projectileSource=nullptr;frame=nullptr;return output;}
 catch(...){projectileSource=nullptr;frame=nullptr;throw;}
}
void NativeEngineShadowCapture::EnableProjectileProgram()
{
 Check(projectileSource==this&&frame&&frame->projectile,"Original projectile program Enable has no source scope");
 programs.Enable(NativeEngineShadowProfile::Projectile);
}
void NativeEngineShadowCapture::DisableProjectileProgram(){programs.Disable();}
void NativeEngineShadowCapture::BeginFlyingPieces(){recorder.UpdateWorldState([](auto& s){s.cullEnabled=false;},false);}
void NativeEngineShadowCapture::EndFlyingPieces(){recorder.UpdateWorldState([](auto& s){s.cullEnabled=true;},false);Unbind();}
void NativeEngineShadowCapture::DrawProjectilePiece(const S3DModelPiece* piece,const CMatrix44f& transform,bool shatter,uint32_t first,uint32_t count)
{
 Check(projectileSource==this&&frame&&piece,"Original projectile piece has no actual source submission");
 const auto program=programs.Current();
 Check(program->profile==NativeEngineShadowProfile::Projectile&&!program->modern,"Original projectile output selected another program");
 const auto geometry=S3DModelVAO::GetInstance().NativeLegacyShadowGeometry();
 Check(bool(geometry),"Original legacy model upload has no immutable geometry publication");
 const auto& indices=geometry->indices;
 if(!shatter){first=piece->indxStart;count=piece->indxCount;}
 Check(first<=indices.size()&&count<=indices.size()-first&&count%3==0,"Original projectile piece submission exceeds its actual index range");
 if(!count)return;
 for(size_t n=first;n<size_t(first)+count;++n)Check(geometry->indices[n]<geometry->vertices.size(),"Original projectile piece has an out-of-range local vertex");
 Check(clips.observed&&clips.contextGeneration==textures.Registry().ContextGeneration()&&!clips.enabled[2],"Original projectile clip state is unobserved or has an unimplemented third plane");
 recorder.PushMatrix();recorder.MultMatrix(Matrix(transform));
 NativeEngineShadowDraw draw;draw.sequence=NextNativeWorldDrawSequence();draw.profile=program->profile;draw.compiledOriginal=program->compiledOriginal;draw.pieceGeometry=geometry;draw.firstIndex=first;draw.indexCount=count;
 draw.modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);
 for(size_t n=0;n<draw.clipEnabled.size();++n){draw.clipEnabled[n]=clips.enabled[n];draw.clipPlanes[n]=clips.eyeEquations[n];}
 draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;draw.raster={true,draw.surface.viewport,draw.surface.scissor};draw.alphaTexture=textures.History().Binding(0,NativeCUSTextureTarget::Image2D);
 frame->draws.push_back(std::move(draw));recorder.PopMatrix();
}
void NativeEngineShadowCapture::DrawProjectilePieceTree(const S3DModelPiece* piece,const float3& root)
{
 const auto& bind=piece->bposeTransform;
 DrawProjectilePiece(piece,Transform(bind.r,bind.t-root,bind.s).ToMatrix());
 for(const auto* child:piece->children)DrawProjectilePieceTree(child,root);
}
void NativeEngineShadowCapture::DrawProjectileModel(const CProjectile* projectile)
{
 Check(projectile&&projectile->model&&projectileSource==this,"Original projectile model has no selected source");
 // ShadowGen has no team-color uniform. Keep the original SetTeamColor call
 // at its caller; capture only the shader inputs used by this submission.
 switch(projectile->weapon*2+projectile->piece){
  case 2:{
   const auto* weapon=static_cast<const CWeaponProjectile*>(projectile);
   recorder.PushMatrix();recorder.MultMatrix(Matrix(weapon->GetTransformMatrix(weapon->GetProjectileType()==WEAPON_MISSILE_PROJECTILE)));
   if(!projectile->luaDraw||!eventHandler.DrawProjectile(projectile))
    for(const auto* piece:weapon->model->pieceObjects)if(piece->HasGeometryData())DrawProjectilePiece(piece,piece->bposeTransform.ToMatrix());
   recorder.PopMatrix();break;
  }
  case 1:{
   const auto* part=static_cast<const CPieceProjectile*>(projectile);
   CMatrix44f transform;transform.Translate(part->drawPos);transform.Rotate(part->GetDrawAngle()*math::DEG_TO_RAD,part->spinVec);
   recorder.PushMatrix();recorder.MultMatrix(Matrix(transform));
   if(!projectile->luaDraw||!eventHandler.DrawProjectile(projectile)){
    if(part->explFlags&PF_Recursive)DrawProjectilePieceTree(part->omp,part->omp->bposeTransform.t);
    else if(part->omp->HasGeometryData()){const auto& bind=part->omp->bposeTransform;DrawProjectilePiece(part->omp,Transform(bind.r,ZeroVector,bind.s).ToMatrix());}
   }
   recorder.PopMatrix();break;
  }
  default:break; // The original switch also emits no model for this case.
 }
}
void NativeEngineShadowCapture::DrawFlyingPiece(const S3DModelPiece* piece,const CMatrix44f& matrix,uint32_t first,uint32_t count)
{
 DrawProjectilePiece(piece,matrix,true,first,count);
}
} // namespace rendering
