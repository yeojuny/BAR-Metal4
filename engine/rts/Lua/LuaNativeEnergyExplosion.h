/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeEnergyExplosionData.h"
struct lua_State;
namespace LuaNativeEnergyExplosion {
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeEnergyExplosionFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
