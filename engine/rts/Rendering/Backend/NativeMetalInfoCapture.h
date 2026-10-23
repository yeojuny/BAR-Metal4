/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeMetalInfoData.h"
#include <functional>
namespace rendering {
// Main engine thread only. The consumer must return the exact completion serial
// after real shared GPU retirement. No completion is inferred from preparation.
using NativeMetalInfoConsumer=std::function<std::uint64_t(std::shared_ptr<const NativeMetalInfoFrame>)>;
void SetNativeMetalInfoConsumer(NativeMetalInfoConsumer consumer);
void ResetNativeMetalInfo(); // original per-map handler lifetime; preserves registered consumer
void SelectNativeMetalInfoMode(bool active);
void UpdateNativeMetalInfo(); // original IInfoTextureHandler::Update cadence, including hidden windows
std::shared_ptr<const NativeMetalInfoFrame> CaptureNativeMetalInfo();
}
