/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/NativeOutlineData.h"

#include <functional>

struct lua_State;
namespace LuaNativeOutline {
struct Services {
	std::function<bool()> ready;
	std::function<std::shared_ptr<const rendering::NativeOutlineProgramConsumer>(const rendering::NativeOutlineProgramSource&)> compile;
};
void Configure(Services);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeOutlineFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
} // namespace LuaNativeOutline
