/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
struct lua_State;
namespace rendering { struct NativeNapalmPreviewFrame; }
namespace LuaNativeNapalmPreview {
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeNapalmPreviewFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
