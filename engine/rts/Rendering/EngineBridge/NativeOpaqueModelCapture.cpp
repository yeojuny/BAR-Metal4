/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeOpaqueModelCapture.h"
#include "NativeWaterLuaCapture.h"
#include "Game/Game.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Rendering/GL/GeometryBuffer.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/IWater.h"
#include "Rendering/Backend/NativeOriginalModelContext.h"
#include "Rendering/Textures/3DOTextureHandler.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "System/EventHandler.h"
#include "Sim/Misc/TeamHandler.h"
#include <stdexcept>

namespace rendering {
namespace {void Check(bool value,const char* why){if(!value)throw std::logic_error(why);}}
bool NativeOpaqueModelFrame::HasWriters()const noexcept{
	for(const auto& pass:passes)if(pass.mrt)return true;
	return false;
}
void CancelNativeOpaqueModels(NativeTextureRegistry& registry,const NativeOpaqueModelFrame& frame)noexcept{
	for(auto i=frame.passes.rbegin();i!=frame.passes.rend();++i)if(i->mrt)CancelNativeCUSDeferredPhase(registry,*i->mrt);
}
NativeOpaqueModelCapture::NativeOpaqueModelCapture(WorldFrameCapture& world,NativeCUSSourceCapture& source,
	NativeTextureContext& context,ui::NativeUIRecorder& ui):models(world),cus(source),textures(context),recorder(ui){}
std::shared_ptr<const NativeOpaqueModelFrame> NativeOpaqueModelCapture::Capture(uint64_t frame,
	const WorldCamera& actualView,const NativeOpaqueModelCallbacks& callbacks,bool drawReflection,bool drawRefraction)
{
	Check(callbacks.ready&&callbacks.ready()&&cus.Ready(),"Original model pass graph has an unconnected selected producer/consumer");
	Check(callbacks.commonTextures&&callbacks.engine.clipState&&callbacks.engine.captureProgramEnable&&callbacks.engine.requireOriginalProgram&&callbacks.engine.disableOriginalProgram&&callbacks.legacyMaterials&&callbacks.postDeferred,"Original opaque model callback owners are incomplete");
	Check(game&&!(drawReflection&&drawRefraction),"Original model source flags or game owner are invalid");
 const auto entryMode=game->GetDrawMode();
 Check(entryMode==(drawReflection?CGame::gameReflectionDraw:(drawRefraction?CGame::gameRefractionDraw:CGame::gameNormalDraw)),"Original opaque-model flags disagree with the actual entry draw mode");
 const auto& water=IWater::GetWater();
 Check(water&&water->DrawReflectionPass()==drawReflection&&water->DrawRefractionPass()==drawRefraction,"Original water flags have not been established at the actual source call");
	Check(recorder.IsWorldStage(),"Original model source callbacks require their actual world command scope");
	const auto* buffer=LuaObjectDrawer::GetGeometryBuffer();
	// These are the original stored producer predicates. The selected native
	// consumer uses this same source query; it does not rewrite HaveGL4/FBO.
	const bool deferredAllowed=!drawReflection&&!drawRefraction&&LuaObjectDrawer::NativeDrawDeferredEnabled()&&buffer&&buffer->Valid()&&CUnitDrawer::CanDrawDeferred();
	auto out=std::make_shared<NativeOpaqueModelFrame>();out->sourceDrawFrame=frame;out->drawReflection=drawReflection;out->drawRefraction=drawRefraction;
	std::shared_ptr<NativeCUSDeferredPhase> unpublishedPhase;
	const auto inheritedViewport=recorder.CurrentState().draw.viewport;
	Check(bool(inheritedViewport),"Original model source viewport has not been observed");
	try{
		for(const bool feature:{false,true}){
			const bool wantDeferred=feature?CFeatureDrawer::DrawDeferred():CUnitDrawer::DrawDeferred();
			const bool wantForward=feature?CFeatureDrawer::DrawForward():CUnitDrawer::DrawForward();
			const auto objectType=feature?LUAOBJ_FEATURE:LUAOBJ_UNIT;
			const bool legacy=(feature?featureDrawer->NativeImplementation():unitDrawer->NativeImplementation())!=ModelDrawerTypes::MODEL_DRAWER_GL4;
   // CModelDrawerBase::DrawImpl outer source writers precede BOTH selected
   // passes, even when sky drawing itself is disabled or no pass is selected.
   if(legacy){auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=true;recorder.SetAlphaTest(alpha);ISky::GetSky()->SetupFog();}
			for(const bool deferred:{true,false}){
				if(deferred?!(wantDeferred&&deferredAllowed):!wantForward)continue;
				NativeOpaqueModelPass pass;pass.sequence=NextNativeWorldDrawSequence();pass.feature=feature;pass.deferred=deferred;
				WorldCamera passView=actualView;
				if(deferred){
					game->SetDrawMode(CGame::gameDeferredDraw);
					passView.viewport={0,0,globalRendering->viewSizeX,globalRendering->viewSizeY};
					recorder.SetViewport({0,0,uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)});
     // Original active SetDepthRange implementation writes clearDepth1 and
     // LEQUAL; reverse-Z is compiled out in the actual GeometryBuffer.
     recorder.SetOriginalClearDepth(1);
     recorder.UpdateWorldState([](auto& state){state.depthCompare=ScreenAlphaCompare::LessEqual;},false);
     const bool clear=LuaObjectDrawer::NativeDeferredClear(objectType);
     if(clear){
      recorder.SetOriginalClearColor({0,0,0,0});
      const auto state=recorder.CurrentWorldState();
      NativeWorldAttachmentClear packet;
      packet.sequence=NextNativeWorldDrawSequence();packet.stage=NativeWorldDrawStage::ModelDeferred;
      packet.mask=static_cast<ScreenAttachmentMask>(ScreenAttachmentColor|ScreenAttachmentDepth);
      packet.values=recorder.CurrentClearValues();packet.colorWrite=state.colorWrite;packet.depthWrite=state.depthWrite;
      packet.frontStencilWriteMask=state.frontStencil.writeMask;packet.scissor=recorder.CurrentState().draw.scissor;
      pass.initialClear=std::move(packet);
     }
     // Only the exact unmasked whole-target source clear can use loadAction.
     // A partial clear must load/COW the actual preceding six image versions.
     unpublishedPhase=std::make_shared<NativeCUSDeferredPhase>(BeginNativeCUSDeferredPhase(textures.Registry(),textures.Geometry(NativeGeometryTextureRole::Models),frame,passView,feature,pass.FullInitialClear()));
				}
				std::shared_ptr<NativeEngineModelPhase> engine;
				// Per-object Lua callbacks can submit genuine CUS draws before a
				// default model/construction stage. Keep that same source frame
				// active through engine, the ONE whole gadget callback, and real
				// legacy material execution; never replay those callbacks later.
				pass.lua=cus.Capture(frame,[&]{
				NativeEngineModelState state(textures,recorder,*callbacks.engine.clipState,callbacks.engine.disableOriginalProgram,callbacks.engine.requireOriginalProgram);
				state.Begin(legacy,CModelDrawerConcept::WireFrameModeRef(),actualView.view,callbacks.commonTextures(),[&]{return callbacks.engine.captureProgramEnable(!legacy);});
				auto engineHooks=callbacks.engine;engineHooks.sourceState=&state;
				// Actual source texture binds and 3DO state transitions run once
				// in the visitor; no second history writer occurs at GPU Prepare.
				engineHooks.bindModelTextures=[&](int type,int bin){
					if(type==MODELTYPE_3DO){state.BindModelTextures(textureHandler3DO.GetAtlasTex1ID(),textureHandler3DO.GetAtlasTex2ID());return;}
					const auto* material=textureHandlerS3O.GetTexture(bin);
					Check(material!=nullptr,"Original model renderer texture bin has no S3O/ASS material");
					state.BindModelTextures(material->tex1,material->tex2);
				};
				engineHooks.currentModelTextures=[&]{return state.CurrentModelTextures();};
				engineHooks.currentRaster=[&]{const auto surface=recorder.CurrentState().draw;return NativeEngineModelRaster{true,surface.viewport,surface.scissor};};
				engineHooks.modelTypeScope=[&](int type,bool begin){
     if(begin){state.PushModelType(type==MODELTYPE_3DO);if(!legacy){if(teamHandler.IsValidTeam(0))state.SetTeamAlpha(1);state.SetColorMultiplier({1,1,1,1});}}
     else state.PopModelType(type==MODELTYPE_3DO);
    };
				engineHooks.setWireframe=[&](bool wire){state.SetWireframe(wire);};
				engine=models.BeforeLua(frame,passView,feature,engineHooks,drawReflection,drawRefraction);
				if(feature)featureDrawer->InvokeNativeOpaqueLua(deferred,drawReflection,drawRefraction);else unitDrawer->InvokeNativeOpaqueLua(deferred,drawReflection,drawRefraction);
				state.End();
				models.ObserveAfterLua(*engine,deferred);
				callbacks.legacyMaterials(*engine,deferred);
				});
				pass.engine=std::move(engine);
				if(deferred){
					FinishNativeCUSDeferredPhase(*unpublishedPhase,pass.lua);
					pass.mrt=std::move(unpublishedPhase);
     recorder.SetOriginalClearDepth(1);
     recorder.UpdateWorldState([](auto& state){state.depthCompare=ScreenAlphaCompare::LessEqual;},false);
					game->SetDrawMode(entryMode);
				}
				out->passes.push_back(std::move(pass));
				if(deferred){
					if(LuaObjectDrawer::NativeDeferredNotify(objectType)){
						out->passes.back().postDeferredSequence=NextNativeWorldDrawSequence();
      out->passes.back().postDeferredFixedFogEnabled=OriginalNativeModelContext().fogEnabled;
						callbacks.postDeferred(feature,[&]{if(feature)eventHandler.DrawFeaturesPostDeferred();else eventHandler.DrawUnitsPostDeferred();});
					}
					// Original LoadViewport is AFTER the PostDeferred callback.
					recorder.SetViewport({globalRendering->viewPosX,globalRendering->viewPosY,uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)});
				}
			}
   if(legacy){
    const auto unit=textures.History().ActiveUnit();Check(bool(unit),"Original legacy DrawImpl cleanup has no observed active texture unit");
    textures.History().Enable(*unit,NativeCUSTextureTarget::Image2D,false);
    if(*unit<2)recorder.SyncObservedTextureBinding(*unit);
    OriginalNativeModelContext().fogEnabled=false; // glDisable preserves color/range.
   }
		}
	}catch(...){
		if(unpublishedPhase)CancelNativeCUSDeferredPhase(textures.Registry(),*unpublishedPhase);
		CancelNativeOpaqueModels(textures.Registry(),*out);
		game->SetDrawMode(entryMode);recorder.SetViewport(*inheritedViewport);throw;
	}
	out->endSequence=NextNativeWorldDrawSequence();
	return out;
}
std::shared_ptr<const NativeOpaqueModelFrame> NativeOpaqueModelCapture::CaptureAlpha(uint64_t frame,
	const WorldCamera& actualView,NativeWorldDrawStage stage,const NativeOpaqueModelCallbacks& callbacks,bool drawReflection,bool drawRefraction)
{
	Check(callbacks.ready&&callbacks.ready()&&cus.Ready()&&callbacks.commonTextures&&callbacks.engine.clipState&&callbacks.engine.captureProgramEnable&&callbacks.engine.requireOriginalProgram&&callbacks.engine.disableOriginalProgram&&callbacks.legacyMaterials,"Original alpha source graph lacks actual model/material/callback owners");
	Check(recorder.IsWorldStage()&&recorder.CurrentWorldStage()==stage,"Original alpha capture lost its actual world stage");
	Check(stage==NativeWorldDrawStage::AlphaBelowWater||stage==NativeWorldDrawStage::AlphaAboveWater||stage==NativeWorldDrawStage::AlphaAll||stage==NativeWorldDrawStage::WaterReflection||stage==NativeWorldDrawStage::WaterRefraction,"Original alpha source has another route");
	Check((stage==NativeWorldDrawStage::WaterReflection)==drawReflection&&(stage==NativeWorldDrawStage::WaterRefraction)==drawRefraction,"Original alpha source route disagrees with its original reflection/refraction arguments");
	auto out=std::make_shared<NativeOpaqueModelFrame>();out->sourceDrawFrame=frame;out->drawReflection=drawReflection;out->drawRefraction=drawRefraction;out->alphaStage=stage;
	std::optional<std::array<float,4>> clip;
	if(stage==NativeWorldDrawStage::AlphaBelowWater)clip=std::array<float,4>{0,-1,0,0};
	if(stage==NativeWorldDrawStage::AlphaAboveWater)clip=std::array<float,4>{0,1,0,0};
	// Original CWorldDrawer alpha entry, before unit and feature SetupAlphaDrawing.
	recorder.UpdateWorldState([](auto& state){state.depthCompare=ScreenAlphaCompare::LessEqual;},false);
	recorder.UpdateBlend([](auto& blend){blend.enabled=true;});
	for(const bool feature:{false,true}){
		NativeOpaqueModelPass pass;pass.sequence=NextNativeWorldDrawSequence();pass.feature=feature;
		const bool legacy=(feature?featureDrawer->NativeImplementation():unitDrawer->NativeImplementation())!=ModelDrawerTypes::MODEL_DRAWER_GL4;
		std::shared_ptr<NativeEngineModelPhase> engine;
		pass.luaEffects=CaptureNativeWaterLuaFrame([&]{
			pass.lua=cus.Capture(frame,[&]{
				NativeEngineModelState state(textures,recorder,*callbacks.engine.clipState,callbacks.engine.disableOriginalProgram,callbacks.engine.requireOriginalProgram);
				state.Begin(legacy,CModelDrawerConcept::WireFrameModeRef(),actualView.view,callbacks.commonTextures(),[&]{return callbacks.engine.captureProgramEnable(!legacy);},true);
				auto hooks=callbacks.engine;hooks.sourceState=&state;
				hooks.bindModelTextures=[&](int type,int bin){
					if(type==MODELTYPE_3DO){state.BindModelTextures(textureHandler3DO.GetAtlasTex1ID(),textureHandler3DO.GetAtlasTex2ID());return;}
					const auto* material=textureHandlerS3O.GetTexture(bin);Check(material!=nullptr,"Original alpha model texture bin lacks its actual S3O/ASS texture pair");state.BindModelTextures(material->tex1,material->tex2);
				};
				hooks.currentModelTextures=[&]{return state.CurrentModelTextures();};
				hooks.currentRaster=[&]{const auto surface=recorder.CurrentState().draw;return NativeEngineModelRaster{true,surface.viewport,surface.scissor};};
				hooks.modelTypeScope=[&](int type,bool begin){if(begin)state.PushModelType(type==MODELTYPE_3DO);else state.PopModelType(type==MODELTYPE_3DO);};
				hooks.setWireframe=[&](bool value){state.SetWireframe(value);};
				engine=models.BeforeAlphaLua(frame,actualView,feature,hooks,clip,drawReflection,drawRefraction);
				if(feature)featureDrawer->InvokeNativeAlphaLua(drawReflection,drawRefraction);else unitDrawer->InvokeNativeAlphaLua(drawReflection,drawRefraction);
				state.End();
				models.ObserveAfterLua(*engine,false);
				callbacks.legacyMaterials(*engine,false);
			});
		});
		pass.engine=std::move(engine);out->passes.push_back(std::move(pass));
	}
	out->endSequence=NextNativeWorldDrawSequence();return out;
}

} // namespace rendering
