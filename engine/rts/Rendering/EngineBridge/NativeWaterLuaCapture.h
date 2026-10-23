/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <functional>
#include "Rendering/Backend/NativeWaterLuaFrame.h"

namespace rendering {
// Caller owns/restores active camera, IWater flags, original game draw mode and
// generic world stage. This only brackets the five existing typed producers;
// it never resets the common world sequence or advances simulation state.
std::shared_ptr<const NativeWaterLuaFrame> CaptureNativeWaterLuaFrame(const std::function<void()>& callback);
}
