/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeMetalInfoData.h"
#include <functional>
namespace rendering {
// Main engine thread only. The consumer must return the exact completion serial
// after real shared GPU retirement. No completion is inferred from preparation.
using NativeMetalInfoConsumer=std::function<std::uint64_t(std::shared_ptr<const NativeMetalInfoFrame>)>;
// Runs before an active update/mode transaction, never from inside its Mutation.
// The native adapter settles prior CPU/Begin/GPU receipts before a Runtime lock.
void SetNativeMetalInfoConsumer(NativeMetalInfoConsumer consumer, std::function<void()> beforeControl = {});
void ResetNativeMetalInfo(); // original per-map handler lifetime; preserves registered consumer
void SelectNativeMetalInfoMode(bool active);
void UpdateNativeMetalInfo(); // original IInfoTextureHandler::Update cadence, including hidden windows
std::shared_ptr<const NativeMetalInfoFrame> CaptureNativeMetalInfo();
}
