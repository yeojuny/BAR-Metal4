/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeModelLightData.h"
namespace rendering {
// Call at actual legacy model Enable / preview emission, not world capture:
// original decay/tracking/LOS depends on that call site's current engine state.
std::shared_ptr<const NativeModelLightFrame> CaptureNativeModelLights();
}
