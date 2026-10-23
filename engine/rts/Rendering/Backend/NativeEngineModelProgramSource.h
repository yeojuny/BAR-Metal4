/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEngineModelData.h"
namespace rendering {
// One native source counterpart of the four original programs per drawer.
// Its values are written at the same constructor/Enable/setter boundaries;
// draws copy them, while this mutable owner retains the original next-call state.
class NativeEngineModelProgramSource {
public:
 struct Program {
  std::shared_ptr<const void> compiledOriginal;
  NativeEngineModelProgramState values;
 };
 void Construct(bool modern,bool shadows,bool deferred,std::shared_ptr<const void> actualCompiledOriginal);
 std::shared_ptr<Program> Enable(bool modern,bool shadows,bool deferred,const NativeEngineModelProgramState& actualEnableValues);
 void Disable();
 void RequireSelected(const std::shared_ptr<const void>& actualSelectedOriginal)const;
 std::shared_ptr<Program> Selected()const{return selected;}
 bool Constructed()const{for(const auto& program:programs)if(!program||!program->compiledOriginal)return false;return true;}
 void Reset();
private:
 std::array<std::shared_ptr<Program>,8> programs;
 std::shared_ptr<Program> selected;
 static size_t Index(bool modern,bool shadows,bool deferred){return size_t(modern)*4+size_t(shadows)+size_t(deferred)*2;}
};
} // namespace rendering
