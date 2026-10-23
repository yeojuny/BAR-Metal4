/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeOriginalShadowCapture.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Env/GrassDrawer.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeOriginalModelContext.h"
#include "Map/ReadMap.h"
#include "Map/BaseGroundDrawer.h"
#include "NativeWaterLuaCapture.h"
#include "System/EventHandler.h"
#include "Game/Game.h"
#include "Game/Camera.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#undef camera
namespace rendering {
namespace {
NativeOriginalShadowCapture* actualSource=nullptr;
thread_local NativeOriginalShadowCapture* terrainSource=nullptr;
void Check(bool v,const char* why){if(!v)throw std::logic_error(why);}
}
NativeOriginalShadowCapture& CurrentOriginalNativeShadowCapture(){Check(actualSource,"Original shadow callback is outside its actual source capture");return *actualSource;}
NativeOriginalShadowCapture* CurrentNativeTerrainShadowCapture()noexcept{return terrainSource;}
NativeOriginalShadowCapture::NativeOriginalShadowCapture(NativeEngineShadowCapture& e,NativeEngineShadowProgramSource& p,NativeCUSSourceCapture& c,NativeOriginalShadowParticlesCapture& a,NativeTextureContext& t,ui::NativeUIRecorder& r):engine(e),programs(p),particles(a),cus(c),textures(t),recorder(r){}
std::shared_ptr<const NativeOriginalShadowFrame> NativeOriginalShadowCapture::Capture(
 std::shared_ptr<const NativeShadowTextureFrame> namedFrame,const WorldCamera& drawing,
 const NativeEngineShadowCallbacks& input,const std::function<void()>& actualDrawShadowPasses)
{
 Check(!actualSource&&!frame&&!clearContract&&namedFrame&&namedFrame->sourceDrawFrame&&globalRendering&&namedFrame->sourceDrawFrame==uint64_t(globalRendering->drawFrame),"Original shadow capture has another active source/frame");
 Check(game&&game->GetDrawMode()==CGame::gameShadowDraw&&CCamera::GetActive()&&CCamera::GetActive()->GetCamType()==CCamera::CAMTYPE_SHADOW&&recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque,"Original shadow capture lost its actual pre-IBL camera/draw/stage boundary");
 Check(shadowHandler.HasNativeTextureOwner()&&textures.Initialized()&&textures.UIAttached()&&programs.Constructed()&&cus.Ready()&&input.drawUnit&&input.drawFeature&&actualDrawShadowPasses,"Original shadow programs/source/callback services are unconnected");
 auto output=std::make_shared<NativeOriginalShadowFrame>();output->textures=std::move(namedFrame);output->sourceDrawFrame=output->textures->sourceDrawFrame;output->camera=drawing;output->originalSelectedBits=shadowHandler.shadowGenBits;
 output->beginSequence=NextNativeWorldDrawSequence();frame=output.get();callbacks=&input;actualSource=this;
 try{
  clearContract.emplace(output->textures,output->beginSequence);
  // Exact original CreateShadows entry writes. The existing texture history
  // and recorder mirror are one source owner, with no duplicate event.
  recorder.SetBlend(ScreenBlendState{.enabled=false});
  auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);
  const auto active=textures.History().ActiveUnit();Check(bool(active),"Original shadow entry active texture unit is unobserved");
  textures.History().Enable(*active,NativeCUSTextureTarget::Image2D,false);if(*active<2)recorder.SyncObservedTextureBinding(*active);
  recorder.Color({1,1,1,1});recorder.UpdateWorldState([](auto& s){s.depthTest=true;s.depthWrite=true;},false);
  recorder.ClearAttachments(ScreenAttachmentDepth); // actual CURRENT depth clear, mask/scissor
  // Begin/End the actual CUS source once around the original complete pass.
  // A dark sun invokes neither geometry nor gadget callbacks, but its named
  // depth publication still reaches the real common depth clear below.
  output->cus=cus.Capture(output->sourceDrawFrame,[&]{if(output->textures->redrawColor)actualDrawShadowPasses();});
  recorder.UpdateWorldState([](auto& s){s.colorWrite={true,true,true,true};s.combinedColorWrite=s.colorWrite;},false);
  output->endSequence=NextNativeWorldDrawSequence();output->clearProof=clearContract->Seal(output->endSequence);clearContract.reset();actualSource=nullptr;frame=nullptr;callbacks=nullptr;return output;
 }catch(...){clearContract.reset();actualSource=nullptr;frame=nullptr;callbacks=nullptr;throw;}
}
NativeShadowClearAcceptance NativeOriginalShadowCapture::ValidateClear(unsigned mask,const ScreenClearValues& values,const NativeWorldDrawState& state,std::optional<ScreenScissor> scissor)const{
 Check(frame&&clearContract&&recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque&&state==recorder.CurrentWorldState()&&scissor==recorder.CurrentState().draw.scissor&&values==recorder.CurrentClearValues(),"Original shadow clear metadata is not the sole recorder's current source event");
 return clearContract->Validate(mask,values,state,std::move(scissor));
}
void NativeOriginalShadowCapture::CommitClear(const NativeShadowClearAcceptance& pending,uint64_t sequence){
 Check(frame&&clearContract&&recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque,"Original shadow clear commit left its actual source scope");clearContract->Commit(pending,sequence);
}
void NativeOriginalShadowCapture::DrawModels(bool feature)
{
 Check(frame&&callbacks&&shadowHandler.InShadowPass(),"Original shadow model source lost its actual pass");
 const bool modern=(feature?featureDrawer->NativeImplementation():unitDrawer->NativeImplementation())==ModelDrawerTypes::MODEL_DRAWER_GL4;
 frame->models[feature]=engine.CaptureDefault(frame->sourceDrawFrame,frame->camera,feature,*callbacks);
 // This ONE whole unit callin contains CUS tree/feature/unit/skinning pass16;
 // do not call it per model/bin or once again for feature CUS.
 if(feature)eventHandler.DrawShadowFeaturesLua();else eventHandler.DrawShadowUnitsLua();
 engine.AfterOriginalLua(modern);
 const auto type=feature?LuaObjType::LUAOBJ_FEATURE:LuaObjType::LUAOBJ_UNIT;
 Check(!LuaObjectDrawer::HasNativeShadowMaterialBins(type),"Allocated original legacy shadow material bins require their actual shadow shader/uniform/list/texture executor");
 // Genuinely-empty original remainder still executes its actual setup and
 // DrawMaterialBins/ClearBins path. No fabricated execution proof is issued.
 if(feature)featureDrawer->InvokeNativeShadowRemainder();else unitDrawer->InvokeNativeShadowRemainder();
}
void NativeOriginalShadowCapture::DrawOriginalPasses()
{
 Check(frame&&callbacks&&shadowHandler.InShadowPass()&&frame->textures->redrawColor&&!frame->colorClearSequence,"Original shadow passes were invoked outside their actual selected positive-sun source");
 recorder.PushAttrib(ui::Polygon|ui::Enable);
 recorder.UpdateWorldState([](auto& s){s.cullEnabled=true;s.cull=NativeWorldCull::Back;},false);
 eventHandler.DrawWorldShadow();
 recorder.UpdateWorldState([](auto& s){s.colorWrite={true,true,true,false};s.combinedColorWrite=s.colorWrite;},false);
 frame->colorClearSequence=NextNativeWorldDrawSequence();
 // Existing actual common WorldClearHandler is the one source event owner.
 recorder.ClearAttachments(ScreenAttachmentColor,ScreenColor{1,1,1,0});
 recorder.UpdateWorldState([](auto& s){s.colorWrite={false,false,false,false};s.combinedColorWrite=s.colorWrite;},false);
 const unsigned selected=frame->originalSelectedBits;
 // This bit selects the original grass entry, not another tree/model scan.
 // At the pinned engine revision DrawShadow's drawing body is commented out;
 // invoke that real function so its selection remains owned by the engine.
 if(selected&CShadowHandler::SHADOWGEN_BIT_TREE)grassDrawer->DrawShadow();
 if(selected&CShadowHandler::SHADOWGEN_BIT_PROJ)
  frame->projectiles=engine.CaptureProjectiles(frame->sourceDrawFrame,frame->camera,[]{projectileDrawer->DrawShadowOpaque();});
 if(selected&CShadowHandler::SHADOWGEN_BIT_MODEL){DrawModels(false);DrawModels(true);}
 recorder.UpdateWorldState([](auto& s){s.cull=NativeWorldCull::Back;},false);
 if(selected&CShadowHandler::SHADOWGEN_BIT_MAP)DrawTerrain();
 if(selected&CShadowHandler::SHADOWGEN_BIT_PROJ){
  const auto& request=frame->textures->color.backing->request;
  frame->transparent=particles.Capture(frame->sourceDrawFrame,request.size,request.colored,
   []{projectileDrawer->DrawShadowTransparent();});
  // Keep the real callback once, even when ShouldSubmit produced no particle
  // draw. It shares this source's existing stage, program and global ordinals.
  frame->transparentLua=CaptureNativeWaterLuaFrame([]{eventHandler.DrawShadowPassTransparent();});
 }
 recorder.PopAttrib();
}
void NativeOriginalShadowCapture::DrawTerrain()
{
 Check(frame&&!terrainFrame&&!terrainSource&&readMap&&readMap->GetGroundDrawer(),"Original MAP shadow has no actual ground drawer/source scope");
 auto output=std::make_shared<NativeTerrainShadowFrame>();output->sourceDrawFrame=frame->sourceDrawFrame;
 output->contextGeneration=textures.Registry().ContextGeneration();output->camera=frame->camera;
 terrainFrame=output.get();terrainSource=this;terrainProgram=false;terrainSquare.reset();
 const auto close=[&]{terrainFrame=nullptr;terrainSource=nullptr;terrainProgram=false;terrainSquare.reset();};
 try{
  readMap->GetGroundDrawer()->DrawShadowPass();
  Check(output->drawInvoked&&!terrainProgram,"Original MAP shadow drawer was omitted or left its program bound");
  frame->terrain=std::move(output);close();
 }catch(...){close();throw;}
}
void NativeOriginalShadowCapture::BeginTerrainDraw()
{
 Check(terrainSource==this&&terrainFrame&&!terrainFrame->drawInvoked&&shadowHandler.InShadowPass(),"Original MAP shadow Draw is missing or repeated");
 terrainFrame->drawInvoked=true;
}
void NativeOriginalShadowCapture::EnableTerrainProgram(float minimum,const std::array<float,4>& size)
{
 Check(terrainSource==this&&terrainFrame&&terrainFrame->drawInvoked&&!terrainFrame->drawEntered&&!terrainProgram,"Original MAP shadow Enable left its selected Draw");
 Check(std::isfinite(minimum)&&std::all_of(size.begin(),size.end(),[](float x){return std::isfinite(x)&&x>0;}),"Original MAP shadow map-size/border uniform is invalid");
 programs.Enable(NativeEngineShadowProfile::Terrain);terrainProgram=true;
 terrainFrame->drawEntered=true;terrainBorderMinHeight=minimum;terrainMapSize=size;
}
void NativeOriginalShadowCapture::DisableTerrainProgram()
{
 Check(terrainSource==this&&terrainFrame&&terrainProgram,"Original MAP shadow Disable has no actual Enable");
 programs.RequireSelected();programs.Disable();terrainProgram=false;
}
void NativeOriginalShadowCapture::SelectTerrainSquare(int x,int z)
{
 Check(terrainSource==this&&terrainFrame&&terrainProgram&&x>=0&&z>=0,"Original MAP shadow square is outside its mesh traversal");
 terrainSquare=std::array<int32_t,2>{x,z};
}
void NativeOriginalShadowCapture::SubmitTerrain(const void* positions,size_t count,size_t stride,
 std::span<const uint32_t> indices,bool indexed)
{
 Check(terrainSource==this&&terrainFrame&&terrainProgram&&terrainSquare&&
  recorder.CurrentWorldStage()==NativeWorldDrawStage::ShadowOpaque&&!recorder.HasActiveTarget(),"Original MAP shadow submission lost its source/program/target");
 const auto program=programs.Current();Check(program->profile==NativeEngineShadowProfile::Terrain,"Original MAP shadow has another source program");
 const size_t drawCount=indexed?indices.size():count;if(!drawCount)return;
 Check(positions&&count&&stride>=sizeof(std::array<float,3>)&&count<=std::numeric_limits<size_t>::max()/stride&&
  drawCount%3==0&&drawCount<=uint64_t(std::numeric_limits<int32_t>::max()),"Original MAP shadow triangle extent is invalid");
 const auto& clips=OriginalNativeModelContext().clips;
 Check(clips.observed&&clips.contextGeneration==terrainFrame->contextGeneration&&
  textures.Registry().ContextGeneration()==terrainFrame->contextGeneration,"Original MAP shadow lost its actual immutable context/clip state");
 NativeTerrainShadowDraw draw;draw.compiledOriginal=program->compiledOriginal;draw.indexed=indexed;
 draw.vertices.resize(count);const auto* bytes=static_cast<const std::byte*>(positions);
 for(size_t n=0;n<count;++n)std::memcpy(draw.vertices[n].data(),bytes+n*stride,sizeof(draw.vertices[n]));
 for(uint32_t index:indices)Check(index<count,"Original MAP shadow index exceeds its actual source vertices");
 draw.indices.assign(indices.begin(),indices.end());draw.mapSize=terrainMapSize;draw.texSquare=*terrainSquare;draw.borderMinHeight=terrainBorderMinHeight;
 draw.modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);draw.projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);
 draw.clipEnabled=clips.enabled;draw.clipPlanes=clips.eyeEquations;
 draw.world=recorder.CurrentWorldState();draw.surface=recorder.CurrentState().draw;
 draw.heightTexture=textures.History().Binding(1,NativeCUSTextureTarget::Image2D);
 draw.sequence=NextNativeWorldDrawSequence();terrainFrame->draws.push_back(std::move(draw));
}
void NativeOriginalShadowCapture::ResetOriginalShadowMatrices(){
 // Exact original ResetWorldShadowMatrices; do not reset raster/attributes.
 auto identity=ui::IdentityMatrix();WorldMatrix projection=identity;
 projection[0]=projection[5]=projection[10]=2;projection[12]=projection[13]=projection[14]=-1;
 recorder.MatrixMode(ui::NativeUIMatrixMode::Texture);recorder.LoadMatrix(identity);
 recorder.MatrixMode(ui::NativeUIMatrixMode::Projection);recorder.LoadMatrix(projection);
 WorldMatrix modelView;std::copy_n(shadowHandler.GetShadowMatrixRaw(),16,modelView.begin());
 recorder.MatrixMode(ui::NativeUIMatrixMode::ModelView);recorder.LoadMatrix(modelView);
}
void NativeOriginalShadowCapture::EnableLuaWorldShadow(bool safe){
 Check(frame&&shadowHandler.InShadowPass(),"Original Lua shadow Enable is outside its actual source");
 if(safe){recorder.PushAttrib(ui::Blend|ui::Alpha|ui::Enable|ui::Depth|ui::Polygon|ui::Line|ui::Point|ui::ColorMask|ui::Viewport);recorder.ResetDrawingState();OriginalNativeSourceProgramBinding().Unbind();}
 recorder.UpdateWorldState([](auto& s){s.colorWrite={false,false,false,false};s.combinedColorWrite=s.colorWrite;s.offsetFactor=s.offsetUnits=1;s.polygonOffset=true;},false);programs.Enable(false);
}
void NativeOriginalShadowCapture::DisableLuaWorldShadow(bool safe){
 Check(frame,"Original Lua shadow Disable has no actual source");recorder.UpdateWorldState([](auto& s){s.polygonOffset=false;},false);programs.Disable();ResetOriginalShadowMatrices();
 if(safe)recorder.PopAttrib();OriginalNativeSourceProgramBinding().Unbind();
}
void NativeOriginalShadowCapture::ResetLuaWorldShadow(bool safe){if(!safe)return;ResetOriginalShadowMatrices();recorder.ResetDrawingState();OriginalNativeSourceProgramBinding().Unbind();recorder.UpdateWorldState([](auto& s){s.colorWrite={false,false,false,false};s.combinedColorWrite=s.colorWrite;s.offsetFactor=s.offsetUnits=1;s.polygonOffset=true;},false);}
}
