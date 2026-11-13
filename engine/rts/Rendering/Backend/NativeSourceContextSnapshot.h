/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <stdexcept>
namespace rendering {
// Values only, captured on main for a joined worker invocation. This is not a
// license to destroy/replace the context while that invocation is outstanding.
struct NativeSourceContextSnapshot {
 struct ModelGeometry { bool depthPresent=false; uint32_t width=0,height=0; };
 bool initialized=false;
 uint64_t generation=0;
 ModelGeometry model;
 template<class Context,class Role> static NativeSourceContextSnapshot Capture(Context& context,Role models) {
  NativeSourceContextSnapshot result;result.initialized=context.Initialized();
  if(result.initialized){
   result.generation=context.Registry().ContextGeneration();
   const auto& names=context.Geometry(models);
   result.model={names.Name(5)!=0,names.Width(),names.Height()};
  }
  return result;
 }
 uint64_t ContextGeneration() const { RequireContext();return generation; }
 ModelGeometry Models() const { RequireContext();return model; }
private:
 void RequireContext() const {
  if(!initialized||!generation)throw std::runtime_error("Captured native source context is not initialized");
 }
};
}
