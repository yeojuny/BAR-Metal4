/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelData.h"
namespace rendering {
class NativeLegacyMaterialSource;
// Functional execution ownership, issued after every actual successful
// remainder return, including the original genuinely-empty set path.
// Keeping the original observations prevents an empty-bin readiness shortcut.
class NativeLegacyMaterialExecution {
public:
 bool Matches(const NativeEngineModelPhase& phase,bool expectedDeferredPass)const noexcept{
		if(actualDeferredPass!=expectedDeferredPass||actualPhase!=&phase||sourceDrawFrame!=phase.sourceDrawFrame||feature!=phase.feature||alpha!=phase.alpha||drawReflection!=phase.drawReflection||drawRefraction!=phase.drawRefraction||((drawReflection||drawRefraction)&&expectedDeferredPass)||observations.size()!=phase.legacyBins.size())return false;
  for(size_t i=0;i<observations.size();++i){const auto& a=observations[i];const auto& b=phase.legacyBins[i];
   if(b.deferredPass!=actualDeferredPass||a.shaderType!=b.shaderType||a.shaderName!=b.shaderName||a.preList!=b.preList||a.postList!=b.postList||a.deferredPass!=b.deferredPass||a.validForPass!=b.validForPass||a.drawCall!=b.drawCall||a.useCamera!=b.useCamera||a.objects!=b.objects)return false;
  }return true;
 }
private:
	const NativeEngineModelPhase* actualPhase;uint64_t sourceDrawFrame;bool feature;bool actualDeferredPass;bool drawReflection,drawRefraction,alpha;
 std::vector<NativeEngineLegacyBinObservation> observations;
	explicit NativeLegacyMaterialExecution(const NativeEngineModelPhase& p,bool pass):actualPhase(&p),sourceDrawFrame(p.sourceDrawFrame),feature(p.feature),actualDeferredPass(pass),drawReflection(p.drawReflection),drawRefraction(p.drawRefraction),alpha(p.alpha),observations(p.legacyBins){}
 friend class NativeLegacyMaterialSource;
};
} // namespace rendering
