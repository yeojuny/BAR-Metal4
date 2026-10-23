/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeDistortionData.h"
#include <functional>
struct lua_State;
namespace LuaNativeDistortion {
struct Services {
 std::function<bool()> ready;
 std::function<std::shared_ptr<const rendering::NativeDistortionProgramConsumer>(const rendering::NativeDistortionProgramSource&)> compile;
};
void Configure(Services);
bool Available();
void Reset();
bool PushEntries(lua_State*);
}
