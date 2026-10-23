/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineModelState.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include "System/Matrix44f.h"

namespace rendering {
namespace {constexpr auto Image=NativeCUSTextureTarget::Image2D;constexpr auto Cube=NativeCUSTextureTarget::Cube;}
NativeEngineModelState::NativeEngineModelState(NativeTextureContext& c,ui::NativeUIRecorder& r,NativeEngineModelClipState& actualClips,std::function<void()> disable,std::function<void()> require):context(c),recorder(r),clips(actualClips),programDisable(std::move(disable)),programRequire(std::move(require)){}
NativeEngineModelState::~NativeEngineModelState(){Abort();}
void NativeEngineModelState::ActiveUnit(unsigned unit){context.History().ActiveTexture(unit);}
void NativeEngineModelState::Enable(unsigned unit,NativeCUSTextureTarget target,bool value){
	context.History().Enable(unit,target,value);
	if(unit<2&&target==Image)recorder.SyncObservedTextureBinding(unit);
}
void NativeEngineModelState::Bind(unsigned unit,uint64_t name,NativeCUSTextureTarget target){
	context.Registry().Bind(unit,name,target);
	if(unit<2&&target==Image)recorder.SyncObservedTextureBinding(unit);
}
void NativeEngineModelState::ShadowSampling(bool comparison){
	const auto b=context.History().Binding(2,Image);
	if(b.knowledge==NativeCUSBindingKnowledge::Unobserved)throw std::logic_error("Original shadow sampler binding was not observed");
	auto sampling=b.object?b.object->sampling:b.defaultSampling;
	sampling.compareMode=comparison?0x884e:0;
	if(comparison)sampling.compareFunction=0x0203;
	sampling.depthMode=0x1909;
	context.Registry().SetBoundSampling(2,Image,sampling);
}
void NativeEngineModelState::SetWireframe(bool value){
	recorder.UpdateWorldState([=](auto& state){state.frontPolygonMode=state.backPolygonMode=value?NativePolygonMode::Line:NativePolygonMode::Fill;},false);
}
void NativeEngineModelState::Begin(bool old,bool wire,const WorldMatrix& view,const NativeEngineModelTextureNames& input,std::function<std::shared_ptr<NativeEngineModelProgramSource::Program>()> actualProgramEnable,bool alpha){
	if(attributes||!context.Initialized()||!context.UIAttached()||!clips.observed||clips.contextGeneration!=context.Registry().ContextGeneration()||!programDisable||!programRequire)throw std::logic_error("Original opaque model scope lacks its actual context or is nested");
 if(!actualProgramEnable)throw std::logic_error("Original model Enable writer is missing");
	legacy=old;alphaPass=alpha;names=input;inheritedClipEnable=clips.enabled;
	recorder.PushAttrib(ui::Enable|ui::Polygon|(alphaPass?ui::Depth:0)|(alphaPass&&legacy?(ui::Blend|ui::Alpha|ui::ColorMask):0));attributes=true;
	try{
		SetWireframe(wire);
		if(!alphaPass)recorder.UpdateWorldState([](auto& state){state.cull=NativeWorldCull::Back;state.cullEnabled=true;},false);
		if(legacy){
			if(!alphaPass)recorder.SetAlphaTest({true,ScreenAlphaCompare::Greater,.5f});
			recorder.MatrixMode(ui::NativeUIMatrixMode::Projection);recorder.PushMatrix();projection=true;recorder.MultMatrix(view);
			recorder.MatrixMode(ui::NativeUIMatrixMode::ModelView);recorder.PushMatrix();modelView=true;recorder.LoadMatrix(ui::IdentityMatrix());
		}
		ActiveUnit(1);Enable(1,Image,true);
		if(names.shadowsLoaded){ActiveUnit(2);Bind(2,names.shadowDepth,Image);Enable(2,Image,true);ShadowSampling(true);ActiveUnit(3);Bind(3,names.shadowColor,Image);}
		ActiveUnit(4);Enable(4,Cube,true);Bind(4,names.reflection,Cube);
		ActiveUnit(5);Enable(5,Cube,true);Bind(5,names.specular,Cube);
		ActiveUnit(0);Enable(0,Image,true);
  program=actualProgramEnable();programScope=true;
  if(!program||!program->compiledOriginal||!program->values.observed||program->values.modern==legacy||program->values.shadows!=names.shadowsLoaded)
   throw std::logic_error("Original model Enable lacks actual typed program values");
  if(!legacy){
   // Original GL4 Enable: bind program, enable clip2 for water, then write
   // SetCameraMode/clipPlane2 before any model or Lua source callback.
   waterClipScope=program->values.cameraMode!=0;
   if(waterClipScope)SetClipEnabled(2,true);
   SetCameraMode(program->values.cameraMode);
			if(alphaPass)program->values.alphaControl={.1f,1,0,0};
		}
		if(alphaPass){
			Enable(0,Image,true);
			auto blend=recorder.CurrentState().draw.blend;blend.enabled=true;
			blend.sourceColor=blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;
			blend.destinationColor=blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;recorder.SetBlend(blend);
			if(legacy)recorder.SetAlphaTest({true,ScreenAlphaCompare::Greater,.1f});
			recorder.UpdateWorldState([](auto& state){state.depthWrite=false;},false);
  }
	}catch(...){Abort();throw;}
}
void NativeEngineModelState::BindModelTextures(uint64_t first,uint64_t second){
	if(!attributes)throw std::logic_error("Original model-bin texture bind is outside opaque scope");
	ActiveUnit(1);Bind(1,second,Image);ActiveUnit(0);Bind(0,first,Image);
}
void NativeEngineModelState::PushModelType(bool threeDO){
	if(!attributes)throw std::logic_error("Original model type push is outside opaque scope");
	if(threeDO)recorder.UpdateWorldState([](auto& state){state.cullEnabled=false;},false);
}
void NativeEngineModelState::PopModelType(bool threeDO){
	if(!attributes)throw std::logic_error("Original model type pop is outside opaque scope");
	if(threeDO)recorder.UpdateWorldState([](auto& state){state.cullEnabled=true;},false);
}
std::array<NativeCUSTextureBinding,2> NativeEngineModelState::CurrentModelTextures()const{
	return{context.History().Binding(0,Image),context.History().Binding(1,Image)};
}
void NativeEngineModelState::BeginObject(const WorldMatrix& transform){
 if(!attributes||object)throw std::logic_error("Original model transform scope is missing or nested");
 if(legacy){recorder.PushMatrix();objectMatrixPushed=true;recorder.MultMatrix(transform);}object=true;
}
void NativeEngineModelState::BeginModernObject(){if(legacy||!attributes||object)throw std::logic_error("Original GL4 object scope is missing or nested");object=true;objectMatrixPushed=false;}
void NativeEngineModelState::BeginUntransformedObject(){if(!attributes||object)throw std::logic_error("Original untransformed model scope is missing or nested");object=true;objectMatrixPushed=false;}
void NativeEngineModelState::EndObject(){
 if(!object||build)throw std::logic_error("Original model transform ended before its stages");
 // Deliberately no MatrixMode restore: the original glPopMatrix operates on
 // the mode left by the callback, including any original stack error.
 if(objectMatrixPushed)recorder.PopMatrix();objectMatrixPushed=false;object=false;
}
void NativeEngineModelState::SetTeamColor(const std::array<float,4>& value){programRequire();program->values.team=value;}
void NativeEngineModelState::SetTeamAlpha(float value){programRequire();program->values.teamAlpha=value;}
void NativeEngineModelState::SetColorMultiplier(const std::array<float,4>& value){programRequire();program->values.colorMultiplier=value;}
int NativeEngineModelState::SetMatrixMode(int mode){programRequire();const int old=program->values.matrixMode;program->values.matrixMode=mode;return old;}
void NativeEngineModelState::SetCameraMode(int mode){
 if(legacy||mode<0||mode>2)throw std::logic_error("Original GL4 camera mode has no corresponding source writer");
 programRequire();program->values.cameraMode=mode;
 SetClipPlane(2,mode==1?std::array<float,4>{0,1,0,0}:(mode==2?std::array<float,4>{0,-1,0,0}:std::array<float,4>{0,0,0,1}));
}
void NativeEngineModelState::SetStaticModelMatrix(const WorldMatrix& value){programRequire();program->values.staticModelMatrix=value;}
void NativeEngineModelState::SetNanoColor(const std::array<float,4>& value){programRequire();program->values.nano=value;}
void NativeEngineModelState::SetClipEnabled(unsigned plane,bool value){
 if(plane>=3)throw std::logic_error("Original model clip plane outside original model family");clips.SetEnabled(plane,value);
}
void NativeEngineModelState::SetClipPlane(unsigned plane,const std::array<float,4>& equation){
 if(plane>=3)throw std::logic_error("Original model clip equation outside original model family");
 if(!legacy){programRequire();program->values.modelClipPlanes[plane]=equation;return;}
 // glClipPlane transforms the equation by inverse-transpose MV when written,
 // independent of MatrixMode and independent of whether this plane is enabled.
 CMatrix44f mv;const auto values=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);
 std::copy(values.begin(),values.end(),mv.m);bool invertible=false;const auto inverse=mv.Invert(&invertible);
 if(!invertible)throw std::runtime_error("Original glClipPlane has singular actual model-view");
 std::array<float,4> eye{};
 for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)eye[row]+=inverse.m[row*4+col]*equation[col];
 clips.SetEyeEquation(plane,eye);
}
void NativeEngineModelState::BeginBuild(bool actualAMDHacks){
 if(!object||build)throw std::logic_error("Original construction scope is missing or nested");
 recorder.PushAttrib(legacy?ui::Current:ui::Polygon);build=true;lowerStageClosed=false;amdHacks=actualAMDHacks;
 SetClipEnabled(0,true);SetClipEnabled(1,true);
}
void NativeEngineModelState::EnterBuildStage(const NativeEngineModelDraw& d,size_t stage,bool lower){
 if(!build)throw std::logic_error("Original construction stage outside its source scope");
 if(!lower)CloseLowerBuildScope();
 SetNanoColor(d.nanoColor);
 if(legacy&&amdHacks&&(stage==0||d.polygonOffset)){
  SetClipEnabled(0,false);if(stage==0)SetClipEnabled(1,false);
 }else{
  SetClipPlane(0,d.modelClipPlanes[0]);if(lower)SetClipPlane(1,d.modelClipPlanes[1]);
 }
 if(stage==0)SetWireframe(true);
 if(d.polygonOffset)recorder.UpdateWorldState([](auto& w){w.offsetFactor=1;w.offsetUnits=1;w.polygonOffset=true;},false);
}
void NativeEngineModelState::LeaveBuildStage(const NativeEngineModelDraw& d,size_t stage){
 if(!build)throw std::logic_error("Original construction stage ended outside its source scope");
 if(stage==0){SetWireframe(false);if(legacy&&amdHacks){SetClipEnabled(0,true);SetClipEnabled(1,true);}}
 if(d.polygonOffset)recorder.UpdateWorldState([](auto& w){w.polygonOffset=false;},false);
}
void NativeEngineModelState::CloseLowerBuildScope(){
 if(lowerStageClosed)return;
 if(!legacy)SetClipPlane(1,{0,0,0,1});SetClipEnabled(1,false);lowerStageClosed=true;
}
void NativeEngineModelState::EndBuild(){
 if(!build)throw std::logic_error("Original construction ended without source Begin");
 CloseLowerBuildScope();SetNanoColor({1,1,1,0});if(!legacy)SetClipPlane(0,{0,0,0,1});
 SetClipEnabled(0,false);recorder.PopAttrib();build=false;
}
void NativeEngineModelState::CaptureDefaultDraw(NativeEngineModelDraw& d)const{
 if(!attributes||!object||!program||!program->values.observed)throw std::logic_error("Original default draw lacks actual source/program scope");
 d.sourceModelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);
 d.sourceProjection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);
 d.sourceWorld=recorder.CurrentWorldState();d.sourceSurface=recorder.CurrentState().draw;
 d.compiledOriginalProgram=program->compiledOriginal;d.program=program->values;d.nanoColor=program->values.nano;d.colorMultiplier=program->values.colorMultiplier;d.alphaControl=program->values.alphaControl;
 d.clipEnabled=clips.enabled;d.storedClipPlanes=legacy?clips.eyeEquations:program->values.modelClipPlanes;
 d.wireframe=d.sourceWorld.frontPolygonMode==NativePolygonMode::Line;
 d.polygonOffset=d.sourceWorld.polygonOffset;d.polygonOffsetFactor=d.sourceWorld.offsetFactor;d.polygonOffsetUnits=d.sourceWorld.offsetUnits;
 d.textures=CurrentModelTextures();
 d.lightingTextures={context.History().Binding(2,Image),context.History().Binding(3,Image),context.History().Binding(4,Cube),context.History().Binding(5,Cube)};
 d.originalStateObserved=true;
}
void NativeEngineModelState::PopMatrices(){
	if(projection){recorder.MatrixMode(ui::NativeUIMatrixMode::Projection);recorder.PopMatrix();projection=false;}
	if(modelView){recorder.MatrixMode(ui::NativeUIMatrixMode::ModelView);recorder.PopMatrix();modelView=false;}
}
void NativeEngineModelState::End(){
	if(!attributes||object||build)throw std::logic_error("Original opaque model scope ended without complete source object stages");
	programDisable();programScope=false;
 if(waterClipScope){SetClipEnabled(2,false);waterClipScope=false;}
	ActiveUnit(1);Enable(1,Image,false);
	if(names.shadowsLoaded){ActiveUnit(2);Bind(2,0,Image);Enable(2,Image,false);ShadowSampling(false);}
	// This is GL_TEXTURE_CUBE_MAP on unit3 in the original helper. Do not
	// normalize it to 2D or erase unit3's retained shadow-color binding.
	ActiveUnit(3);Enable(3,Cube,false);ActiveUnit(4);Enable(4,Cube,false);ActiveUnit(0);Enable(0,Image,false);
	PopMatrices();
	if(legacy&&!alphaPass){auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);}
	recorder.PopAttrib();attributes=false;clips.enabled=inheritedClipEnable;
	// GL_ENABLE_BIT restores enable state only; bin bindings and sampler
	// writes survive. Mirror the restored enables over those current names.
	recorder.SyncObservedTextureBinding(0);recorder.SyncObservedTextureBinding(1);
}
void NativeEngineModelState::Abort()noexcept{
	// Exceptional source capture has no valid draw to consume. Release only
	// owned stacks; keep real texture events already issued before failure.
	if(programScope){try{programDisable();}catch(...){}programScope=false;}
 if(waterClipScope){try{SetClipEnabled(2,false);}catch(...){}waterClipScope=false;}
 if(build){try{recorder.PopAttrib();}catch(...){}build=false;}
 if(object){try{if(objectMatrixPushed)recorder.PopMatrix();}catch(...){}objectMatrixPushed=false;object=false;}
 try{PopMatrices();}catch(...){}
	if(attributes){try{recorder.PopAttrib();}catch(...){}attributes=false;clips.enabled=inheritedClipEnable;}
}
} // namespace rendering
