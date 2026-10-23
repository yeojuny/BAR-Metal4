/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldDrawData.h"
namespace rendering {
struct NativeInheritedClipPlaneSource {unsigned index;float sign;};
std::optional<NativeInheritedClipPlaneSource> NativeInheritedWaterClipPlaneSource(NativeWorldDrawStage);
// Applies only the original index. Alpha plane3 survives SSAO disable0..2.
void DisableNativeInheritedWaterClipPlane(NativeWorldDrawState&,NativeWorldDrawStage,unsigned actualIndex);
// glClipPlane stores the equation in eye coordinates at the original call.
// IWater/WorldDrawer call with identity modelview. Lua's subsequent matrix
// changes affect vertices, never that stored equation. Returned coordinates
// match NativeWorldDrawVertex after its explicit zero-to-one conversion.
std::optional<std::array<float,4>> NativeInheritedWaterClipPlane(NativeWorldDrawStage,
 const std::array<float,16>& currentProjection,bool zeroToOneDepth);
}
