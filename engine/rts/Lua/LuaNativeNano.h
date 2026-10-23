/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeLuaNanoData.h"
#include <memory>
struct lua_State;

namespace LuaNativeNano {
// Independent later-world scope. Begin follows immutable engine-world capture;
// original DrawWorld owns the Lua emission updates, never a screen callback.
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeLuaNanoFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State* L);
}
