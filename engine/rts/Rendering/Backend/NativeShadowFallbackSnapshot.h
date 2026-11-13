/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShadowTextureSource.h"
#include <array>
namespace rendering {
// Exact named backings, not names to be re-resolved on the render worker.
// Readiness is checked at consumption, after previous GPU retirement.
struct NativeShadowFallbackSnapshot {
 bool attached=false;
 std::array<std::shared_ptr<const NativeShadowTextureBacking>,2> images{};
 template<class Source,class Handler> static NativeShadowFallbackSnapshot Capture(Source& source,const Handler& handler) {
  NativeShadowFallbackSnapshot result;
  result.attached=source.Attached()&&handler.HasNativeTextureOwner();
  if(result.attached)result.images={source.Image(handler.GetShadowTextureID()),source.Image(handler.GetColorTextureID())};
  return result;
 }
};
}
