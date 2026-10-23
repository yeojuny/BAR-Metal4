/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>

namespace rendering {
struct NativeLuaNanoFrame;
struct NativeProjectileEffectFrame;
struct NativeEnergyExplosionFrame;
struct NativeLightningEffectFrame;
struct NativeFireSmokeFrame;
struct NativeSSAOFrame;

// One original Lua event callback in one actual camera/pass scope. Separate
// bundles prevent reflection/refraction/water-post calls from entering the
// main DrawWorld stream or sharing another pass's frame-level uniforms.
struct NativeWaterLuaFrame {
	std::shared_ptr<const NativeLuaNanoFrame> nano;
	std::shared_ptr<const NativeProjectileEffectFrame> projectile;
	std::shared_ptr<const NativeEnergyExplosionFrame> energy;
	std::shared_ptr<const NativeLightningEffectFrame> lightning;
	std::shared_ptr<const NativeFireSmokeFrame> fireSmoke;
	std::shared_ptr<const NativeSSAOFrame> ssao;
};
}
