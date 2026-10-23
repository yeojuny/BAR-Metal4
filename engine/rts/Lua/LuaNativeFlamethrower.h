/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
struct lua_State;
namespace rendering { struct NativeFlamethrowerFrame; }
namespace LuaNativeFlamethrower {
// Called after the actual original UniformConstants upload, then bracket the
// original later-world callbacks without adding a second callback execution.
void CaptureUniforms();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeFlamethrowerFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
