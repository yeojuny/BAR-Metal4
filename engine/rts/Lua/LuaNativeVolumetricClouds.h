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
bool HasSubmittedCapacity(std::uint64_t identity) noexcept;
void ValidateSubmittedCapacity(std::uint64_t identity);
void ValidateHeldFrame(std::uint64_t identity);
void HoldSubmittedFrame(std::uint64_t sourceDrawFrame);
void TransferPreparedFrame(std::uint64_t identity) noexcept;
void ValidateRestoreHeldFrame(std::uint64_t identity);
void RestoreHeldFrame(std::uint64_t identity) noexcept;
void QuarantineHeldFrame(std::uint64_t identity) noexcept;
void Reset() noexcept;
bool PushEntries(lua_State* L);
}
