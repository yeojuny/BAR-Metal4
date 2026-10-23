/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineShadowData.h"
#include "NativeSourceProgramBinding.h"
#include <stdexcept>
namespace rendering {
// Computed ownership only: construction follows the actual compiler return.
// Enable/Disable are called at original ShadowGen program writer positions.
class NativeEngineShadowProgramSource {
 std::array<std::shared_ptr<NativeEngineShadowProgram>,4> originals;
 std::shared_ptr<NativeEngineShadowProgram> selected;
public:
 void Construct(bool modern,std::shared_ptr<const void> compiled){
  Construct(modern?NativeEngineShadowProfile::ModernModel:NativeEngineShadowProfile::LegacyModel,std::move(compiled));
 }
 void Construct(NativeEngineShadowProfile profile,std::shared_ptr<const void> compiled){
  const auto index=static_cast<size_t>(profile);
  if(index>=originals.size()||!compiled||originals[index])throw std::logic_error("Original ShadowGen construction is missing or repeated");
  auto p=std::make_shared<NativeEngineShadowProgram>();p->profile=profile;p->modern=profile==NativeEngineShadowProfile::ModernModel;p->compiledOriginal=std::move(compiled);originals[index]=std::move(p);
 }
 bool Constructed()const noexcept{return originals[0]&&originals[1]&&originals[0]->compiledOriginal&&originals[1]->compiledOriginal;}
 std::shared_ptr<NativeEngineShadowProgram> Enable(bool modern){
  return Enable(modern?NativeEngineShadowProfile::ModernModel:NativeEngineShadowProfile::LegacyModel);
 }
 std::shared_ptr<NativeEngineShadowProgram> Enable(NativeEngineShadowProfile profile){
  auto p=originals.at(static_cast<size_t>(profile));if(!p||!p->compiledOriginal)throw std::runtime_error("Original ShadowGen has no actual compiled selected profile");
  selected=p;OriginalNativeSourceProgramBinding().BindEngine(p->compiledOriginal);return p;
 }
 void RequireSelected()const{
  auto current=OriginalNativeSourceProgramBinding().EngineOwner();
  if(!selected||current.get()!=selected->compiledOriginal.get())throw std::runtime_error("Original shadow default output has another actual source program");
 }
 std::shared_ptr<NativeEngineShadowProgram> Current()const{RequireSelected();return selected;}
 void Disable(){if(!selected)throw std::logic_error("Original ShadowGen Disable has no actual source Enable");OriginalNativeSourceProgramBinding().Unbind();selected.reset();}
 void Reset(){OriginalNativeSourceProgramBinding().Unbind();selected.reset();originals={};}
};
}
