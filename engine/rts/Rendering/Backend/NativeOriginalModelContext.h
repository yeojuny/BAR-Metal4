/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelData.h"
#include <cmath>
#include "Rendering/EngineBridge/NativeEngineModelProgramCapture.h"
namespace rendering {
// One functional fixed-function source owner per actual renderer context.
// Enable state is saved by original GL_ENABLE_BIT scopes; equations and fog
// remain current until a real writer changes them or that context is destroyed.
struct NativeOriginalModelContext {
 NativeEngineModelClipState clips;
 NativeEngineModelFogSource fog;
 bool fogEnabled=false;
 void Initialize(uint64_t context){clips.InitializeContextDefaults(context);fog={true,0,{0,0,0,0},{0,1,1,1}};fogEnabled=false;}
 void ObserveFog(uint64_t frame,const std::array<float,4>& color,float start,float end,bool enabled){
  if(!clips.observed)throw std::logic_error("Original fog writer lacks its actual native context");
  if(!std::isfinite(start)||!std::isfinite(end)||end==start)
   throw std::runtime_error("Original fog range has no finite native scale");
  fog={true,frame,color,{start,end,1.f,1.f/(end-start)}};fogEnabled=enabled;
 }
 void Reset()noexcept{clips.Reset();fog={};fogEnabled=false;}
};
inline NativeOriginalModelContext& OriginalNativeModelContext(){static NativeOriginalModelContext context;return context;}
} // namespace rendering
