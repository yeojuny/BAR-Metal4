/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeLegacyMaterialSource.h"
#include "Rendering/Backend/NativeLegacyMaterialUniformSink.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/LuaObjectDrawer.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Features/FeatureDrawer.h"
#include "Rendering/Common/ModelDrawerHelpers.h"
#include "Rendering/GlobalRendering.h"
#include "Lua/LuaMaterial.h"
#include "Lua/LuaOpenGLUtils.h"
#include "Game/Camera.h"
#include "Game/Game.h"
#include "Rendering/Env/IWater.h"
#include "Sim/Units/Unit.h"
#include "Sim/Features/Feature.h"
#include "Sim/Misc/TeamHandler.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
thread_local NativeLegacyMaterialSource* active=nullptr;
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
NativeCUSTextureTarget Target(unsigned value){switch(value){case GL_TEXTURE_2D:return NativeCUSTextureTarget::Image2D;case GL_TEXTURE_3D:return NativeCUSTextureTarget::Image3D;case GL_TEXTURE_CUBE_MAP:return NativeCUSTextureTarget::Cube;case GL_TEXTURE_2D_MULTISAMPLE:return NativeCUSTextureTarget::Image2DMultisample;default:throw std::runtime_error("Original legacy material target has no actual native state owner");}}
size_t Components(unsigned type){switch(type){case GL_FLOAT:case GL_INT:return 1;case GL_FLOAT_VEC2:case GL_INT_VEC2:return 2;case GL_FLOAT_VEC3:case GL_INT_VEC3:return 3;case GL_FLOAT_VEC4:case GL_INT_VEC4:return 4;case GL_FLOAT_MAT3:return 9;case GL_FLOAT_MAT4:return 16;default:throw std::runtime_error("Original legacy uniform type has no actual typed native sink");}}
void RequireOriginalPass(const NativeEngineModelPhase& phase,bool deferred){
	Check(game&&!(phase.drawReflection&&phase.drawRefraction)&&(!(phase.drawReflection||phase.drawRefraction)||!deferred)&&(!phase.alpha||!deferred),"Original legacy material phase has incompatible actual pass flags");
 const auto mode=phase.drawReflection?CGame::gameReflectionDraw:(phase.drawRefraction?CGame::gameRefractionDraw:(deferred?CGame::gameDeferredDraw:CGame::gameNormalDraw));
 const auto& water=IWater::GetWater();
 Check(game->GetDrawMode()==mode&&water&&water->DrawReflectionPass()==phase.drawReflection&&water->DrawRefractionPass()==phase.drawRefraction,"Original legacy material phase disagrees with actual game/water source state");
}
WorldMatrix Matrix(const CMatrix44f& matrix){WorldMatrix out;std::copy_n(matrix.m,16,out.begin());return out;}
}
NativeLegacyMaterialSource* CurrentNativeLegacyMaterialSource()noexcept{return active;}
NativeLegacyMaterialSource::NativeLegacyMaterialSource(WorldFrameCapture& world,NativeTextureContext& t,ui::NativeUIRecorder& r,NativeEngineModelClipState& c):models(world),textures(t),recorder(r),clips(c){}
NativeLegacyMaterialSource::~NativeLegacyMaterialSource(){Abort();}
void NativeLegacyMaterialSource::RequirePhase(bool pass)const{
 Check(active==this&&phase&&services&&deferred==pass&&phase->engineObserved&&phase->legacyObserved,"Original legacy material writer is outside its actual source phase");
 RequireOriginalPass(*phase,pass);
}
void NativeLegacyMaterialSource::Capture(NativeEngineModelPhase& source,bool pass,const NativeLegacyMaterialServices& input){
 Check(!active&&!phase&&!source.legacyExecution&&source.engineObserved&&source.legacyObserved,"Original legacy material source capture is missing or repeated");
 RequireOriginalPass(source,pass);
 Check(globalRendering&&source.sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&recorder.IsWorldStage(),"Original legacy material source lost its actual main frame/stage");
 Check(input.engine.clipState==&clips&&clips.observed&&textures.Initialized()&&textures.UIAttached()&&input.commonTextures&&input.engine.captureProgramEnable&&input.engine.disableOriginalProgram&&input.engine.requireOriginalProgram&&input.engine.drawUnitLua&&input.engine.drawFeatureLua&&input.resolveTexture&&input.textureTarget,"Original legacy material source services are unconnected");
 for(const auto& observation:source.legacyBins)Check(observation.deferredPass==pass,"Original legacy material observations belong to another actual pass");
 phase=&source;services=&input;deferred=pass;binsFinished=false;active=this;
 try{
  // The ONE original function still owns material/bin/object ordering, actual
  // DrawMaterial callin, final defMat transition and ClearBins side effects.
		if(source.feature){ScopedModelDrawerImpl<CFeatureDrawer> originalLegacyScope(true,false,false);if(source.alpha)LuaObjectDrawer::DrawAlphaMaterialObjects(LUAOBJ_FEATURE,false);else LuaObjectDrawer::DrawOpaqueMaterialObjects(LUAOBJ_FEATURE,pass);}
		else{ScopedModelDrawerImpl<CUnitDrawer> originalLegacyScope(true,false,false);if(source.alpha)LuaObjectDrawer::DrawAlphaMaterialObjects(LUAOBJ_UNIT,false);else LuaObjectDrawer::DrawOpaqueMaterialObjects(LUAOBJ_UNIT,pass);}
  RequireOriginalPass(source,pass);
  Check(!binsActive&&!engine,"Original legacy material scope did not finish its actual default transition");
  Check(source.legacyBins.empty()||binsFinished,"Allocated original legacy bins were not actually executed/cleared");
  source.legacyExecution=std::shared_ptr<const NativeLegacyMaterialExecution>(new NativeLegacyMaterialExecution(source,pass));
  active=nullptr;phase=nullptr;services=nullptr;
 }catch(...){Abort();throw;}
}
void NativeLegacyMaterialSource::BeginBins(LuaObjType objectType,LuaMatType materialType,bool pass){
	RequirePhase(pass);const auto expected=phase->alpha?LuaObjectDrawer::GetDrawPassAlphaMat():LuaObjectDrawer::GetDrawPassOpaqueMat();
	Check(!binsActive&&!binsFinished&&materialType==expected&&objectType==(phase->feature?LUAOBJ_FEATURE:LUAOBJ_UNIT),"Original legacy source bins selected another material/object pass");
 savedMode=recorder.CurrentMatrixMode();savedClips=clips;
 recorder.PushAttrib(ui::Texture|ui::Enable);binsActive=true;
	recorder.SetAlphaTest({true,ScreenAlphaCompare::Greater,phase->alpha?.1f:.5f});
	if(phase->alpha){auto blend=recorder.CurrentState().draw.blend;blend.enabled=true;blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;recorder.SetBlend(blend);}
}
void NativeLegacyMaterialSource::EndBins(){
 RequirePhase(deferred);Check(binsActive&&!engine,"Original legacy source bins ended with an active shader scope");
 // Called at ORIGINAL glPopAttrib after ORIGINAL ClearBins. TRANSFORM_BIT
 // restores matrix MODE and clip state, never matrix stack contents.
 recorder.PopAttrib();binsActive=false;recorder.SyncObservedTextureBinding(0);recorder.SyncObservedTextureBinding(1);recorder.MatrixMode(savedMode);clips=savedClips;binsFinished=true;
}
void NativeLegacyMaterialSource::ExecuteShader(const LuaMatShader& current,const LuaMatShader& previous,bool pass){
 RequirePhase(pass);Check(binsActive,"Original legacy shader transition lacks its bin scope");
 if(current.type==previous.type){
  if(current.type==LuaMatShader::LUASHADER_GL&&current.openglID!=previous.openglID){OriginalNativeSourceProgramBinding().BindOriginalGL(current.openglID);throw std::runtime_error("Original legacy custom shader has no actual native program/geometry consumer");}
  return;
 }
 if(previous.IsEngineType()){
  Check(bool(engine)&&engineModelType==int(previous.type),"Original legacy shader reset has no actual setup owner");engine->PopModelType(previous.type==LuaMatShader::LUASHADER_3DO);engine->End();engine.reset();engineModelType=-1;
 }else if(previous.IsCustomType())OriginalNativeSourceProgramBinding().Unbind();
 if(current.IsEngineType()){
  Check(camera&&!engine,"Original legacy engine shader setup lacks its actual camera or is nested");
  const bool legacy=unitDrawer->NativeImplementation()!=ModelDrawerTypes::MODEL_DRAWER_GL4;
  Check(legacy,"Original legacy local-model geometry lacks an actual modern instance-attribute source");
  engine=std::make_unique<NativeEngineModelState>(textures,recorder,clips,services->engine.disableOriginalProgram,services->engine.requireOriginalProgram);
		engine->Begin(legacy,CModelDrawerConcept::WireFrameModeRef(),Matrix(camera->GetViewMatrix()),services->commonTextures(),[&]{return services->engine.captureProgramEnable(!legacy);},phase->alpha);
  engine->PushModelType(current.type==LuaMatShader::LUASHADER_3DO);engineModelType=int(current.type);
 }else if(current.IsCustomType()){
  OriginalNativeSourceProgramBinding().BindOriginalGL(current.openglID);
  throw std::runtime_error("Original legacy custom shader has no actual native program/geometry consumer");
 }else Check(current.type==LuaMatShader::LUASHADER_NONE,"Original legacy shader type is unsupported");
}
void NativeLegacyMaterialSource::ExecuteMaterial(const LuaMaterial& current,const LuaMaterial& previous,bool pass){
 RequirePhase(pass);Check(binsActive,"Original legacy material transition lacks its actual bins");
 current.ExecuteNativeTransitionLists(previous);
 ExecuteShader(current.shaders[pass],previous.shaders[pass],pass);
 // Keep this ORIGINAL body: it consumes guRNG.NextVector even when every
 // uniform location is invalid. RawExec alone routes actual typed writes.
 current.uniforms[pass].Execute();
 for(int unit=0;unit<std::max(current.texCount,previous.texCount);++unit)if(previous.textures[unit]!=current.textures[unit]){
  textures.History().ActiveTexture(unsigned(unit));Unbind(unsigned(unit),previous.textures[unit]);Bind(unsigned(unit),current.textures[unit]);
 }
 textures.History().ActiveTexture(0);
 if(current.useCamera!=previous.useCamera){if(current.useCamera)recorder.PopMatrix();else{recorder.PushMatrix();recorder.LoadMatrix(ui::IdentityMatrix());}}
 if(current.cullingMode!=previous.cullingMode)recorder.UpdateWorldState([&](auto& state){
  state.cullEnabled=current.cullingMode!=0;if(current.cullingMode){switch(current.cullingMode){case GL_FRONT:state.cull=NativeWorldCull::Front;break;case GL_BACK:state.cull=NativeWorldCull::Back;break;case GL_FRONT_AND_BACK:state.cull=NativeWorldCull::FrontAndBack;break;default:throw std::runtime_error("Original legacy material culling mode unsupported");}}
 },false);
}
void NativeLegacyMaterialSource::SetObjectTeamColor(const CSolidObject* object,float alpha){
 RequirePhase(deferred);Check(object&&engine,"Original legacy default team uniform lacks its actual selected program");
 if(!teamHandler.IsValidTeam(object->team))return;
 if(engine->Legacy()){const auto value=CModelDrawerHelper::GetTeamColor(object->team,alpha);engine->SetTeamColor({value.x,value.y,value.z,value.w});}
 else engine->SetTeamAlpha(alpha);
}
void NativeLegacyMaterialSource::CallLOD(const ui::NativeDisplayListReference& reference,unsigned raw){
 if(reference.Identity()){if(auto list=reference.Resolve())recorder.CallList(std::move(list));return;}
 Check(raw==0,"Original legacy LOD raw GL list has no actual typed native owner");
}
void NativeLegacyMaterialSource::DrawBinObject(const CSolidObject* object,LuaObjType type,const LuaObjectLODMaterial* lod,bool transform,bool noLuaCall){
 RequirePhase(deferred);Check(object&&lod&&engine,"Original legacy default model output has no actual typed program/source owner");
 services->engine.requireOriginalProgram();
 if(transform&&engine->Legacy()){
  if(type==LUAOBJ_UNIT)engine->BeginObject(Matrix(static_cast<const CUnit*>(object)->GetTransformMatrix()));
  else if(type==LUAOBJ_FEATURE)engine->BeginObject(Matrix(static_cast<const CFeature*>(object)->GetTransformMatrixRef()));
  else throw std::runtime_error("Original legacy default object kind unsupported");
 }else engine->BeginUntransformedObject();
 CallLOD(lod->nativePreDisplayList,lod->preDisplayList);
 // Original DrawBinObject passes lodCall=true: full model, no nano-stage
 // replacement. The actual DrawUnit/Feature callback runs exactly once.
 if(type==LUAOBJ_UNIT){const auto* unit=static_cast<const CUnit*>(object);
  if(noLuaCall||!unit->luaDraw||!services->engine.drawUnitLua(unit)){services->engine.requireOriginalProgram();models.EmitUnit(*phase,unit,*engine);}
 }else if(type==LUAOBJ_FEATURE){const auto* feature=static_cast<const CFeature*>(object);
  if(noLuaCall||!feature->luaDraw||!services->engine.drawFeatureLua(feature)){services->engine.requireOriginalProgram();models.EmitFeature(*phase,feature,*engine);}
 }else throw std::runtime_error("Original legacy default object kind unsupported");
 CallLOD(lod->nativePostDisplayList,lod->postDisplayList);engine->EndObject();
}
void NativeLegacyMaterialSource::SetEnable(unsigned unit,NativeCUSTextureTarget target,bool value){textures.History().Enable(unit,target,value);if(unit<2&&target==NativeCUSTextureTarget::Image2D)recorder.SyncObservedTextureBinding(unit);}
void NativeLegacyMaterialSource::ShadowSampler(unsigned unit,bool comparison){
 const auto binding=textures.History().Binding(unit,NativeCUSTextureTarget::Image2D);Check(binding.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original material shadow sampler has unobserved actual binding");
 auto sampling=binding.object?binding.object->sampling:binding.defaultSampling;sampling.compareMode=comparison?GL_COMPARE_R_TO_TEXTURE:GL_NONE;if(comparison)sampling.compareFunction=GL_LEQUAL;sampling.depthMode=GL_LUMINANCE;
 textures.Registry().SetBoundSampling(unit,NativeCUSTextureTarget::Image2D,sampling);if(unit<2)recorder.SyncObservedTextureBinding(unit);
}
void NativeLegacyMaterialSource::Unbind(unsigned unit,const LuaMatTexture& texture){
 if(texture.type==LuaMatTexture::LUATEX_NONE)return;
 if(texture.type==LuaMatTexture::LUATEX_SHADOWMAP)ShadowSampler(unit,false);
 if(!texture.enable)return;
 SetEnable(unit,Target(services->textureTarget(texture)),false);
}
void NativeLegacyMaterialSource::Bind(unsigned unit,const LuaMatTexture& texture){
 const auto actual=services->resolveTexture(texture);Check(actual.observed,"Original material texture lacks its actual parsed native provider");const auto target=Target(actual.originalTarget);
 if(actual.name){textures.Registry().Bind(unit,actual.name,target);if(unit<2&&target==NativeCUSTextureTarget::Image2D)recorder.SyncObservedTextureBinding(unit);
  // Original Bind enables ONLY2D; cube/volume/array remain unchanged here.
  if(texture.enable&&target==NativeCUSTextureTarget::Image2D)SetEnable(unit,target,true);
 }else if(!texture.enable&&target==NativeCUSTextureTarget::Image2D)SetEnable(unit,target,false);
 if(texture.type==LuaMatTexture::LUATEX_SHADOWMAP)ShadowSampler(unit,true);
}
void NativeLegacyMaterialSource::UniformFloat(unsigned type,int location,size_t count,bool transpose,std::span<const float> values){RequirePhase(deferred);Check(bool(services->uniformFloat),"Original legacy uniform location has no actual typed program sink");services->uniformFloat(type,location,count,transpose,values);}
void NativeLegacyMaterialSource::UniformInt(unsigned type,int location,size_t count,std::span<const int> values){RequirePhase(deferred);Check(bool(services->uniformInt),"Original legacy uniform location has no actual typed program sink");services->uniformInt(type,location,count,values);}
void NativeLegacyMaterialSource::Abort()noexcept{
 if(engine){engine->Abort();engine.reset();}engineModelType=-1;
 if(binsActive){try{recorder.PopAttrib();recorder.SyncObservedTextureBinding(0);recorder.SyncObservedTextureBinding(1);recorder.MatrixMode(savedMode);clips=savedClips;}catch(...){}binsActive=false;}
 if(active==this)active=nullptr;phase=nullptr;services=nullptr;binsFinished=false;
}
bool NativeLegacyUniformFloat(unsigned type,int location,size_t count,bool transpose,const float* values){
 auto* source=CurrentNativeLegacyMaterialSource();if(!source)return false;const size_t components=Components(type);Check(count<=std::numeric_limits<size_t>::max()/components&&values,"Original legacy uniform storage bounds unsupported");source->UniformFloat(type,location,count,transpose,{values,count*components});return true;
}
bool NativeLegacyUniformInt(unsigned type,int location,size_t count,const int* values){
 auto* source=CurrentNativeLegacyMaterialSource();if(!source)return false;const size_t components=Components(type);Check(count<=std::numeric_limits<size_t>::max()/components&&values,"Original legacy integer uniform storage bounds unsupported");source->UniformInt(type,location,count,{values,count*components});return true;
}
} // namespace rendering
