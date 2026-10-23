/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeWaterLuaCapture.h"
#include <array>
#include <stdexcept>
#include "Lua/LuaNativeNano.h"
#include "Lua/LuaNativeProjectileEffects.h"
#include "Lua/LuaNativeEnergyExplosion.h"
#include "Lua/LuaNativeLightning.h"
#include "Lua/LuaNativeFireSmoke.h"
#include "Lua/LuaNativeSSAO.h"

namespace rendering {
std::shared_ptr<const NativeWaterLuaFrame> CaptureNativeWaterLuaFrame(const std::function<void()>& callback)
{
	if (!callback)
		throw std::invalid_argument("Native water Lua capture requires its original callback");
	auto result = std::make_shared<NativeWaterLuaFrame>();
	std::array<bool, 6> owned{};
	static constexpr std::array<void (*)(), 6> begin = {
		LuaNativeNano::BeginWorldFrame, LuaNativeProjectileEffects::BeginWorldFrame,
		LuaNativeEnergyExplosion::BeginWorldFrame, LuaNativeLightning::BeginWorldFrame,
		LuaNativeFireSmoke::BeginWorldFrame, LuaNativeSSAO::BeginWorldFrame,
	};
	static constexpr std::array<void (*)() noexcept, 6> abort = {
		LuaNativeNano::AbortWorldFrame, LuaNativeProjectileEffects::AbortWorldFrame,
		LuaNativeEnergyExplosion::AbortWorldFrame, LuaNativeLightning::AbortWorldFrame,
		LuaNativeFireSmoke::AbortWorldFrame, LuaNativeSSAO::AbortWorldFrame,
	};
	try {
		for (std::size_t i = 0; i < begin.size(); ++i) {
			begin[i]();
			owned[i] = true;
		}
		callback();
		result->nano = LuaNativeNano::EndWorldFrame();
		owned[0] = false;
		result->projectile = LuaNativeProjectileEffects::EndWorldFrame();
		owned[1] = false;
		result->energy = LuaNativeEnergyExplosion::EndWorldFrame();
		owned[2] = false;
		result->lightning = LuaNativeLightning::EndWorldFrame();
		owned[3] = false;
		result->fireSmoke = LuaNativeFireSmoke::EndWorldFrame();
		owned[4] = false;
		result->ssao = LuaNativeSSAO::EndWorldFrame();
		owned[5] = false;
	} catch (...) {
		// A rejected nested Begin belongs to its outer caller. Abort only the
		// scopes actually acquired here, never the caller's existing capture.
		for (std::size_t i = owned.size(); i-- > 0;)
			if (owned[i]) abort[i]();
		throw;
	}
	return result;
}
}
