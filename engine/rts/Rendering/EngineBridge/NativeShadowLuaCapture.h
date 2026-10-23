/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
namespace rendering {
struct NativeShadowColorFrame;
struct NativeWaterLuaFrame;
// Actual original transparent-shadow callin after the engine projectile draw.
// The generic stream uses its own stage; typed effects retain a separate bundle.
std::shared_ptr<const NativeWaterLuaFrame> CaptureNativeShadowLuaFrame(const NativeShadowColorFrame& frame);
}
