/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShadowColorData.h"
namespace rendering {
// Call after original shadowHandler.Update and before Lua shadow callbacks.
// Uses only original draw-side state; no GL upload, simulation or RNG update.
std::shared_ptr<const NativeShadowColorFrame> CaptureNativeShadowColor();
}
