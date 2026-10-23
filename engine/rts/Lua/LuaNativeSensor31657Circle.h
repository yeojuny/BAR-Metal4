/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeSensor31657CircleData.h"
struct lua_State;
namespace LuaNativeSensor31657Circle {
// Parent Metal family supplies its real compiled Impl ownership, never a flag.
void SetConsumerLifetime(std::shared_ptr<const void> actualCompiledCircleFamily);
bool Available();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeSensor31657CircleFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
