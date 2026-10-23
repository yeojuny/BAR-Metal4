/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEngineModelProgramSource.h"
#include "NativeSourceProgramBinding.h"
#include <stdexcept>
namespace rendering {
void NativeEngineModelProgramSource::Construct(bool modern,bool shadows,bool deferred,std::shared_ptr<const void> compiled){
 auto& slot=programs[Index(modern,shadows,deferred)];
 if(slot||!compiled)throw std::logic_error("Original model program construction is missing or repeated");
 auto next=std::make_shared<Program>();next->compiledOriginal=std::move(compiled);
 auto& v=next->values;v.observed=true;v.modern=modern;v.shadows=shadows;
 // Actual ModelDrawerStateGLSL constructor writes team/nano=(0,0,0,0).
 // GL4 GLSL declaration initializers give colorMult=1, alphaCtrl always-pass,
 // teamAlpha=1 and modes=0. These are program construction, never readiness.
 v.team={0,0,0,0};v.nano={0,0,0,0};v.colorMultiplier={1,1,1,1};v.alphaControl={0,0,0,1};v.teamAlpha=1;v.modelClipPlanes={{{0,0,0,1},{0,0,0,1},{0,0,0,1}}};
 v.staticModelMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 slot=std::move(next);
}
std::shared_ptr<NativeEngineModelProgramSource::Program> NativeEngineModelProgramSource::Enable(bool modern,bool shadows,bool deferred,const NativeEngineModelProgramState& input){
 const auto& slot=programs[Index(modern,shadows,deferred)];
 if(selected||!slot||!input.observed||input.modern!=modern||input.shadows!=shadows||input.cameraMode<0||input.cameraMode>2||(!modern&&input.cameraMode!=0)||(deferred&&input.cameraMode!=0))
  throw std::logic_error("Original model Enable has no corresponding compiled program/source writer");
 auto& v=slot->values;
 if(modern){
  // Original GL4 Enable writes alphaCtrl and cameraMode; the real UBO writer
  // owns lighting/matrices, retained separately by every SourcePose.
  v.alphaControl=input.alphaControl;v.cameraMode=input.cameraMode;
 }else{
  if(!input.lights)throw std::logic_error("Original legacy Enable lacks its one actual light-handler update");
  v.sunDirection=input.sunDirection;v.sunAmbient=input.sunAmbient;v.sunDiffuse=input.sunDiffuse;v.sunSpecular=input.sunSpecular;
  v.shadowDensity=input.shadowDensity;v.shadowMatrix=input.shadowMatrix;v.lights=input.lights;
  // Fixed-function fog is an actual retained source service value, not the
  // deferred frame's later environment capture.
  v.fogColor=input.fogColor;v.fogParameters=input.fogParameters;
 }
 selected=slot;OriginalNativeSourceProgramBinding().BindEngine(slot->compiledOriginal);return slot;
}
void NativeEngineModelProgramSource::Disable(){if(!selected)throw std::logic_error("Original model program disabled without Enable");OriginalNativeSourceProgramBinding().Unbind();selected.reset();}
void NativeEngineModelProgramSource::RequireSelected(const std::shared_ptr<const void>& actual)const{
 if(!selected||!actual||selected->compiledOriginal.get()!=actual.get())
  throw std::runtime_error("Original default model draw has an unsupported actual shader owner");
}
void NativeEngineModelProgramSource::Reset(){OriginalNativeSourceProgramBinding().Unbind();selected.reset();programs={};}
} // namespace rendering
