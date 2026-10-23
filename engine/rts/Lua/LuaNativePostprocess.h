/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePostprocessData.h"
struct lua_State;
namespace rendering { class NativePostprocessRecorder; }
namespace LuaNativePostprocess {
void BeginFrame(std::uint32_t framebufferLogicalWidth, std::uint32_t framebufferLogicalHeight);
std::shared_ptr<const rendering::NativePostprocessFrame> EndFrame();
void AbortFrame() noexcept;
// Only actual completed screen-dependent submission may acknowledge this
// serial. An offscreen panel-target-only completion cannot do so.
void RetireCompletedFrame(std::uint64_t serial);
void Reset() noexcept;
rendering::NativePostprocessRecorder& GetRecorder();
bool PushEntries(lua_State* L);
}
