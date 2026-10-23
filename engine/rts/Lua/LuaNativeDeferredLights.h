/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/NativeDeferredLightData.h"

#include <functional>

struct lua_State;
namespace LuaNativeDeferredLights {
struct Services {
	std::function<bool()> ready;
	std::function<std::shared_ptr<const rendering::NativeDeferredLightProgramConsumer>(const rendering::NativeDeferredLightProgramSource&)> compile;
};
void Configure(Services);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeDeferredLightFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
} // namespace LuaNativeDeferredLights
