/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeLightningEffectData.h"
struct lua_State;
namespace LuaNativeLightning {
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeLightningEffectFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State* L);
}
