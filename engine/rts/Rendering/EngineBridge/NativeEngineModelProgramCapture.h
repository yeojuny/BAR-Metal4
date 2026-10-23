/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeEngineModelData.h"
namespace rendering {
struct NativeEngineModelFogSource {
 bool observed=false;uint64_t sourceDrawFrame=0;
 std::array<float,4> color{},parameters{};
};
// Exactly the original opaque Enable writer boundary; this performs the one
// LightHandler update for legacy, never again during Prepare or another pass.
NativeEngineModelProgramState CaptureNativeEngineModelEnableValues(bool modern,bool shadows,
 const NativeEngineModelFogSource& actualFixedFunctionFog);
} // namespace rendering
