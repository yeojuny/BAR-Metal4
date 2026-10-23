/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
struct lua_State;
class LuaVBOImpl;
namespace rendering { struct NativeGameDecalFrame; }
namespace LuaNativeGameDecal {
// Original WG.decalsgl4 shares its buffer with PiP. Retain that exact owner and
// mutable storage; the consumer captures an immutable generation at each draw.
struct PiPBuffer {
	std::shared_ptr<void> keeper;
	std::shared_ptr<LuaVBOImpl> value;
};
// Closed type probe: unknown userdata is not a GameDecal buffer.
PiPBuffer BorrowPiPBuffer(lua_State*, int stackIndex);
// Immediately after original UniformConstants.Update; before any draw callin.
void CaptureUniforms();
void BeginWorldFrame();
std::shared_ptr<const rendering::NativeGameDecalFrame> EndWorldFrame();
void AbortWorldFrame() noexcept;
void Reset();
bool PushEntries(lua_State*);
}
