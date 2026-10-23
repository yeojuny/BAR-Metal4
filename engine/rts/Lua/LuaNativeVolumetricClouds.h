/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeVolumetricCloudData.h"
struct lua_State;
namespace LuaNativeVolumetricClouds {
void BeginFrame(std::uint64_t actualSourceDrawFrame);
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> EndFrame();
std::shared_ptr<const rendering::NativeVolumetricCloudFrame> SnapshotPrefix(std::uint64_t cutoff);
void AbortFrame() noexcept;
void RetireCompletedFrame(std::uint64_t actualSourceDrawFrame);
void Reset() noexcept;
bool PushEntries(lua_State* L);
}
