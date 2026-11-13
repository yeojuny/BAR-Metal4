/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/EngineBridge/NativeOpaqueModelCapture.h"
#include <array>
#include <optional>
#include <new>
namespace rendering::metal4 {
// Main-thread normalized F frontier, valid only across the existing joined
// source/worker handoff. This is NOT permission for younger source capture.
// Retain only F objects: additional U owners would invalidate the reuse proof.
struct MRTFrontier209 {
 uint64_t context=0;
 std::array<std::shared_ptr<const NativeCUSTextureObject>,6> current;
 static std::optional<MRTFrontier209> Capture(const NativeOpaqueModelFrame& source,const NativeTextureRegistry& registry) {
  if(source.passes.size()!=4||!source.passes[2].mrt)return {};
  const auto& f=*source.passes[2].mrt;
  MRTFrontier209 result;result.context=registry.ContextGeneration();
  if(!result.context)return {};
  try {
   for(unsigned i=0;i<6;++i){
    const auto& object=i<5?f.colors[i].current:f.depth.published;
    if(!object)return {};
    result.current[i]=registry.Find(object->name);
    if(!result.current[i]||result.current[i]->identity!=object->identity||
     result.current[i]->backing!=object->backing)return {};
   }
   return result;
  }catch(const std::bad_alloc&){return {};}
 }
};
}
