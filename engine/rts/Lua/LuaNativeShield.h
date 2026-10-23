/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
struct lua_State;
namespace rendering { struct NativeShieldFrame; struct NativeShieldTextureContext; }
namespace LuaNativeShield {
void SetTextureContext(const rendering::NativeShieldTextureContext*); // Same context owner as CUS; null before owner destruction.
bool PushEntries(lua_State*);
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeShieldFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
}
