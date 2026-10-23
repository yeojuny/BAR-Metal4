/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
struct lua_State;
namespace rendering { struct NativeFireGL4Frame; }
namespace LuaNativeFireGL4 {
// Called after the actual original UniformConstants upload, then bracket the
// original later-world callbacks without adding a second callback execution.
void CaptureUniforms();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeFireGL4Frame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
