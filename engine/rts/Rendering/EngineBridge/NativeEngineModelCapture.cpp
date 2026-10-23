/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineModelCapture.h"
#include "Game/Game.h"
#include "Rendering/Env/IWater.h"
#include "Rendering/Backend/NativeEngineModelState.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Common/ModelDrawerHelpers.h"
#include "Rendering/ModelsDataUploader.h"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/GL/myGL.h"
#include <cstring>
#include <limits>
#include "Rendering/WorldFrameCapture.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Features/Feature.h"
#include "Sim/Misc/GlobalSynced.h"
#include "Sim/Misc/TeamHandler.h"
#include <stdexcept>

namespace rendering {
namespace {
WorldMatrix SourceMatrix(const CMatrix44f& m){WorldMatrix out;std::copy_n(m.m,16,out.begin());return out;}
std::array<float,4> SourceTeam(int team){const auto c=CModelDrawerHelper::GetTeamColor(team,1);return{c.x,c.y,c.z,c.w};}
template<class T> NativeEngineModelPosePublication SourcePose(const T* object){
 // Same original offset owners and SAME SInstanceData constructor as
 // S3DModelVAO::AddToSubmissionImpl / SubmitImmediatelyImpl. No VBO upload.
 const auto transform=transformsUploader.GetElemOffset(object);
 const auto uniforms=modelUniformsStorage.GetObjOffset(object);
 const auto bindPose=transformsUploader.GetElemOffset(object->model);
 if(transform==TransformsMemStorage::INVALID_INDEX||bindPose==TransformsMemStorage::INVALID_INDEX||
    transform>std::numeric_limits<uint32_t>::max()||uniforms>std::numeric_limits<uint32_t>::max()||
    bindPose>std::numeric_limits<uint32_t>::max()||object->model->numPieces>65535)
  throw std::runtime_error("Original GL4 model instance address is unavailable");
 const SInstanceData actual(uint32_t(transform),object->paletteIndex,uint16_t(object->model->numPieces),uint32_t(uniforms),uint32_t(bindPose));
 NativeEngineModelPosePublication out;static_assert(sizeof(actual)==sizeof(out.instance));std::memcpy(out.instance.data(),&actual,sizeof(actual));
 out.transforms=OriginalCUSModelPublication().Transforms();out.modelUniforms=OriginalCUSModelPublication().ModelUniforms();out.engineUniforms=OriginalCUSUniformCapture().Current();
 if(!out.transforms||!out.modelUniforms||!out.engineUniforms)throw std::runtime_error("Original GL4 model has no immutable upload publication");
 ValidateNativeCUSUniformPublication(*out.engineUniforms);return out;
}
}

NativeEngineModelPosePublication CaptureNativeEngineModelPose(const CUnit* unit){if(!unit)throw std::logic_error("Original unit pose capture has no actual object");return SourcePose(unit);}
NativeEngineModelPosePublication CaptureNativeEngineModelPose(const CFeature* feature){if(!feature)throw std::logic_error("Original feature pose capture has no actual object");return SourcePose(feature);}
std::shared_ptr<NativeEngineModelPhase> NativeEngineModelCapture::BeforeLua(
	uint64_t frame,const WorldCamera& viewCamera,bool features,const NativeEngineModelSourceHooks& hooks,bool drawReflection,bool drawRefraction)
{
	if(!hooks.sourceState||!hooks.requireOriginalProgram||!hooks.bindModelTextures||!hooks.currentModelTextures||!hooks.currentRaster||!hooks.modelTypeScope||!hooks.setWireframe||!hooks.drawUnitLua||!hooks.drawFeatureLua)
		throw std::runtime_error("Original engine source capture is missing texture/callback owners");
	if(!game||(drawReflection&&drawRefraction)||(drawReflection&&game->GetDrawMode()!=CGame::gameReflectionDraw)||(drawRefraction&&game->GetDrawMode()!=CGame::gameRefractionDraw)||(!drawReflection&&!drawRefraction&&game->GetDrawMode()!=CGame::gameNormalDraw&&game->GetDrawMode()!=CGame::gameDeferredDraw))
  throw std::logic_error("Original engine visitor flags disagree with the actual source draw mode");
 const auto& water=IWater::GetWater();
 if(!water||water->DrawReflectionPass()!=drawReflection||water->DrawRefractionPass()!=drawRefraction)
  throw std::logic_error("Original engine visitor flags disagree with the actual water source state");
	auto result=std::make_shared<NativeEngineModelPhase>();result->sourceDrawFrame=frame;result->viewCamera=viewCamera;result->feature=features;result->drawReflection=drawReflection;result->drawRefraction=drawRefraction;
	result->engineWireframe=CModelDrawerConcept::WireFrameModeRef();
	auto& state=*hooks.sourceState;

 auto bind=[&](const S3DModel* model){hooks.bindModelTextures(model->type,model->textureType);};
	if(features) {
  featureDrawer->VisitNativeOpaqueEngine(hooks.bindModelTextures,[&](const CFeature* f,bool modern){
   if(!modern&&teamHandler.IsValidTeam(f->team))state.SetTeamColor(SourceTeam(f->team));
   if(modern)state.BeginModernObject();else state.BeginObject(SourceMatrix(f->GetTransformMatrixRef(false)));
   const bool handled=!modern&&f->luaDraw&&hooks.drawFeatureLua(f);
   if(!handled){
    hooks.requireOriginalProgram();
    NativeEngineModelDraw d;d.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
    state.CaptureDefaultDraw(d);d.object=world.CaptureEngineFeature(*f,modern,d.sourceModelView,d.program.team);if(modern)d.pose=SourcePose(f);d.raster=hooks.currentRaster();ValidateNativeEngineModelDraw(d);d.sequence=NextNativeWorldDrawSequence();result->beforeLua.push_back(std::move(d));
   }
   state.EndObject();
  },hooks.modelTypeScope,drawReflection,drawRefraction);
	} else {
  unitDrawer->VisitNativeOpaqueEngine(hooks.bindModelTextures,[&](const CUnit* u,bool modern){
   const auto shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
   // Original team uniform and DrawUnitTrans happen ONCE before all stages.
   if(!modern&&teamHandler.IsValidTeam(u->team))state.SetTeamColor(SourceTeam(u->team));
   if(modern)state.BeginModernObject();else state.BeginObject(SourceMatrix(u->GetTransformMatrix()));
   const bool construction=u->beingBuilt&&u->unitDef->showNanoFrame;
   std::vector<NativeEngineModelDraw> stages;
   if(construction){
    const SColor team=teamHandler.Team(u->team)->color;
    const float3 color=globalRendering->teamNanospray?float3(team.r/255.f,team.g/255.f,team.b/255.f):u->unitDef->nanoColor;
    // Keep raw equations even on AMD: disabling a plane does not erase its
    // old stored equation; the original flat helper still writes both.
    stages=NativeEngineBuildStages({shader,u->model->CalcDrawHeight(),u->buildProgress,gs->frameNum,{color.x,color.y,color.z},false});
    state.BeginBuild(globalRendering->amdHacks);
   }else{NativeEngineModelDraw d;d.shader=shader;stages.push_back(d);}
   for(size_t stage=0;stage<stages.size();++stage){const auto stagePlan=stages[stage];auto d=stagePlan;
    if(construction)state.EnterBuildStage(stagePlan,stage,!stagePlan.polygonOffset);
    const bool handled=!modern&&u->luaDraw&&hooks.drawUnitLua(u);
    if(!handled){
     hooks.requireOriginalProgram();
     // Local piece pose is read after the callback, like DrawUnitModel. The
     // outer transform/projection and pre-call team value are NOT reread.
     state.CaptureDefaultDraw(d);d.object=world.CaptureEngineUnit(*u,modern,d.sourceModelView,d.program.team);if(modern)d.pose=SourcePose(u);d.raster=hooks.currentRaster();ValidateNativeEngineModelDraw(d);d.sequence=NextNativeWorldDrawSequence();result->beforeLua.push_back(d);
    }
    if(construction)state.LeaveBuildStage(stagePlan,stage);
   }
   if(construction)state.EndBuild();
   state.EndObject();
   result->engineWireframe=state.Wireframe();

		},[&](const CUnitDrawerData::TempDrawUnit& u,bool modern){
			const auto* definition=u.GetUnitDef();bind(definition->model);
			CMatrix44f matrix;matrix.Translate(u.pos);matrix.RotateY(u.rotation);
			WorldMatrix transport;std::copy_n(matrix.m,16,transport.begin());
			NativeEngineModelDraw d;d.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
			int oldMatrixMode=0;
   if(teamHandler.IsValidTeam(u.team)){if(modern)state.SetTeamAlpha(1);else state.SetTeamColor(SourceTeam(u.team));}
   if(modern){oldMatrixMode=state.SetMatrixMode(1);state.SetStaticModelMatrix(transport);}
   if(modern)state.BeginModernObject();else state.BeginObject(transport);hooks.requireOriginalProgram();state.CaptureDefaultDraw(d);
			d.object=world.CaptureEngineStatic(definition->model,transport,definition->id,u.team,modern,d.program.team);
   if(modern){
    const auto bindPose=transformsUploader.GetElemOffset(definition->model);
    if(bindPose==TransformsMemStorage::INVALID_INDEX||bindPose>std::numeric_limits<uint32_t>::max())throw std::runtime_error("Original static GL4 bind pose is unavailable");
    const SInstanceData actual(uint32_t(bindPose),uint16_t(u.team),uint16_t(definition->model->numPieces),0,uint32_t(bindPose));
    d.pose.staticModel=true;std::memcpy(d.pose.instance.data(),&actual,sizeof(actual));
    d.pose.transforms=OriginalCUSModelPublication().Transforms();d.pose.modelUniforms=OriginalCUSModelPublication().ModelUniforms();d.pose.engineUniforms=OriginalCUSUniformCapture().Current();
   }d.raster=hooks.currentRaster();ValidateNativeEngineModelDraw(d);d.sequence=NextNativeWorldDrawSequence();result->beforeLua.push_back(std::move(d));state.EndObject();if(modern)state.SetMatrixMode(oldMatrixMode);
		},hooks.modelTypeScope,drawReflection,drawRefraction);
	}
	result->engineWireframe=state.Wireframe();result->engineObserved=true;return result;
}
void NativeEngineModelCapture::EmitUnit(NativeEngineModelPhase& phase,const CUnit* unit,NativeEngineModelState& state){
 if(!phase.engineObserved||!phase.legacyObserved||!unit)throw std::logic_error("Original legacy unit emission is outside its actual remainder phase");
 NativeEngineModelDraw draw;const bool modern=!state.Legacy();draw.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
 state.CaptureDefaultDraw(draw);draw.object=world.CaptureEngineUnit(*unit,modern,draw.sourceModelView,draw.program.team);if(modern)draw.pose=SourcePose(unit);
 draw.raster={true,draw.sourceSurface.viewport,draw.sourceSurface.scissor};ValidateNativeEngineModelDraw(draw);draw.sequence=NextNativeWorldDrawSequence();phase.afterLua.push_back(std::move(draw));
}
void NativeEngineModelCapture::EmitFeature(NativeEngineModelPhase& phase,const CFeature* feature,NativeEngineModelState& state){
 if(!phase.engineObserved||!phase.legacyObserved||!feature)throw std::logic_error("Original legacy feature emission is outside its actual remainder phase");
 NativeEngineModelDraw draw;const bool modern=!state.Legacy();draw.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
 state.CaptureDefaultDraw(draw);draw.object=world.CaptureEngineFeature(*feature,modern,draw.sourceModelView,draw.program.team);if(modern)draw.pose=SourcePose(feature);
 draw.raster={true,draw.sourceSurface.viewport,draw.sourceSurface.scissor};ValidateNativeEngineModelDraw(draw);draw.sequence=NextNativeWorldDrawSequence();phase.afterLua.push_back(std::move(draw));
}
void NativeEngineModelCapture::ObserveAfterLua(NativeEngineModelPhase& phase,bool deferredPass)
{
	if(!phase.engineObserved||phase.legacyObserved)throw std::runtime_error("Original engine remainder phase is missing or repeated");
	const auto expectedMode=phase.drawReflection?CGame::gameReflectionDraw:(phase.drawRefraction?CGame::gameRefractionDraw:(deferredPass?CGame::gameDeferredDraw:CGame::gameNormalDraw));
 const auto& water=IWater::GetWater();
 if(!game||(phase.drawReflection&&phase.drawRefraction)||((phase.drawReflection||phase.drawRefraction)&&deferredPass)||game->GetDrawMode()!=expectedMode||!water||water->DrawReflectionPass()!=phase.drawReflection||water->DrawRefractionPass()!=phase.drawRefraction)
  throw std::logic_error("Original engine remainder observation disagrees with actual game/water pass selection");
	const auto type=phase.feature?LUAOBJ_FEATURE:LUAOBJ_UNIT;
	LuaObjectDrawer::SetDrawPassGlobalLODFactor(type);
	phase.legacyBins=phase.alpha?LuaObjectDrawer::ObserveNativeAlphaMaterialBins(type):LuaObjectDrawer::ObserveNativeOpaqueMaterialBins(type,deferredPass);
	phase.legacyObserved=true;
}
std::shared_ptr<NativeEngineModelPhase> NativeEngineModelCapture::BeforeAlphaLua(uint64_t frame,
	const WorldCamera& viewCamera,bool features,const NativeEngineModelSourceHooks& hooks,
	std::optional<std::array<float,4>> alphaClipPlane,bool drawReflection,bool drawRefraction)
{
	if(!hooks.sourceState||!hooks.requireOriginalProgram||!hooks.bindModelTextures||!hooks.currentRaster||!hooks.modelTypeScope||!hooks.drawUnitLua||!hooks.drawFeatureLua)
		throw std::logic_error("Original alpha engine visitor lacks actual state/texture/callback owners");
	const auto mode=drawReflection?CGame::gameReflectionDraw:(drawRefraction?CGame::gameRefractionDraw:CGame::gameNormalDraw);
	const auto& water=IWater::GetWater();
	if(!game||(drawReflection&&drawRefraction)||game->GetDrawMode()!=mode||!water||water->DrawReflectionPass()!=drawReflection||water->DrawRefractionPass()!=drawRefraction)
		throw std::logic_error("Original alpha visitor disagrees with its actual game/water source flags");
	auto result=std::make_shared<NativeEngineModelPhase>();result->sourceDrawFrame=frame;result->viewCamera=viewCamera;
	result->feature=features;result->drawReflection=drawReflection;result->drawRefraction=drawRefraction;result->alpha=true;result->alphaClipPlane=alphaClipPlane;
	auto& state=*hooks.sourceState;
	const auto emit=[&](NativeEngineModelDraw draw){
		draw.raster=hooks.currentRaster();ValidateNativeEngineModelDraw(draw);draw.sequence=NextNativeWorldDrawSequence();result->beforeLua.push_back(std::move(draw));
	};
	if(features){
		featureDrawer->VisitNativeAlphaEngine(hooks.bindModelTextures,[&](const CFeature* feature,bool modern){
			if(!modern&&teamHandler.IsValidTeam(feature->team)){const auto color=CModelDrawerHelper::GetTeamColor(feature->team,IModelDrawerState::alphaValues.x);state.SetTeamColor({color.x,color.y,color.z,color.w});}
			if(modern)state.BeginModernObject();else state.BeginObject(SourceMatrix(feature->GetTransformMatrixRef(false)));
			if(modern||!feature->luaDraw||!hooks.drawFeatureLua(feature)){
				hooks.requireOriginalProgram();NativeEngineModelDraw draw;draw.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;
				state.CaptureDefaultDraw(draw);draw.object=world.CaptureEngineFeature(*feature,modern,draw.sourceModelView,draw.program.team);if(modern)draw.pose=SourcePose(feature);emit(std::move(draw));
			}
			state.EndObject();
		},[&](int type,bool begin){
			hooks.modelTypeScope(type,begin);
			if(begin&&!state.Legacy()){if(teamHandler.IsValidTeam(0))state.SetTeamAlpha(IModelDrawerState::alphaValues.x);state.SetColorMultiplier({1,1,1,1});}
		},drawReflection,drawRefraction);
	}else{
		unitDrawer->VisitNativeAlphaEngine(hooks.bindModelTextures,[&](const CUnit* unit,bool modern){
			if(!modern&&teamHandler.IsValidTeam(unit->team)){const auto color=CModelDrawerHelper::GetTeamColor(unit->team,IModelDrawerState::alphaValues.x);state.SetTeamColor({color.x,color.y,color.z,color.w});}
			if(modern)state.BeginModernObject();else state.BeginObject(SourceMatrix(unit->GetTransformMatrix()));
			// GLSL DrawUnitTrans retains original construction-stage callbacks;
			// GL4 alpha submits a regular instance even when it is being built.
			const bool construction=!modern&&unit->beingBuilt&&unit->unitDef->showNanoFrame;
			std::vector<NativeEngineModelDraw> stages;
			if(construction){
				const auto team=teamHandler.Team(unit->team)->color;const float3 color=globalRendering->teamNanospray?float3(team[0]/255.f,team[1]/255.f,team[2]/255.f):unit->unitDef->nanoColor;
				stages=NativeEngineBuildStages({NativeEngineModelShader::Legacy,unit->model->CalcDrawHeight(),unit->buildProgress,gs->frameNum,{color.x,color.y,color.z},false});state.BeginBuild(globalRendering->amdHacks);
			}else{NativeEngineModelDraw draw;draw.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;stages.push_back(draw);}
			for(size_t stage=0;stage<stages.size();++stage){const auto plan=stages[stage];auto draw=plan;
				if(construction)state.EnterBuildStage(plan,stage,!plan.polygonOffset);
				if(modern||!unit->luaDraw||!hooks.drawUnitLua(unit)){
					hooks.requireOriginalProgram();state.CaptureDefaultDraw(draw);draw.object=world.CaptureEngineUnit(*unit,modern,draw.sourceModelView,draw.program.team);if(modern)draw.pose=SourcePose(unit);emit(std::move(draw));
				}
				if(construction)state.LeaveBuildStage(plan,stage);
			}
			if(construction)state.EndBuild();state.EndObject();
		},[&](const S3DModel* model,const CMatrix44f& transform,int definition,int team,uint16_t palette,size_t arrayTransform,bool modern){
			if(modern)state.BeginModernObject();else state.BeginObject(SourceMatrix(transform));hooks.requireOriginalProgram();
			NativeEngineModelDraw draw;draw.shader=modern?NativeEngineModelShader::GL4:NativeEngineModelShader::Legacy;state.CaptureDefaultDraw(draw);
			draw.object=world.CaptureEngineStatic(model,SourceMatrix(transform),definition,team,modern,draw.program.team);
			if(modern){
				const size_t bindPose=transformsUploader.GetElemOffset(model);const bool array=arrayTransform!=TransformsMemStorage::INVALID_INDEX;
				if(bindPose==TransformsMemStorage::INVALID_INDEX||bindPose>std::numeric_limits<uint32_t>::max()||(array&&arrayTransform>std::numeric_limits<uint32_t>::max())||model->numPieces>65535)
					throw std::runtime_error("Original alpha static model has no actual uploader address");
				const SInstanceData original(uint32_t(array?arrayTransform:bindPose),palette,uint16_t(model->numPieces),uint32_t(modelUniformsStorage.GetObjOffset(model)),uint32_t(bindPose));
				draw.pose.staticModel=true;draw.pose.arrayModel=array;std::memcpy(draw.pose.instance.data(),&original,sizeof(original));
				draw.pose.transforms=OriginalCUSModelPublication().Transforms();draw.pose.modelUniforms=OriginalCUSModelPublication().ModelUniforms();draw.pose.engineUniforms=OriginalCUSUniformCapture().Current();
			}
			emit(std::move(draw));state.EndObject();
		},hooks.modelTypeScope,state,drawReflection,drawRefraction);
	}
	result->engineObserved=true;result->engineWireframe=state.Wireframe();return result;
}

} // namespace rendering
