/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldInfoData.h"
#include <string>
namespace rendering {
void ResetNativeWorldInfo();
void SelectNativeWorldInfoMode(const std::string& mode);
void UpdateNativeWorldInfo(); // exact original handler Update, including hidden windows
std::shared_ptr<const NativeWorldInfoFrame> CaptureNativeWorldInfo();
void RetireNativeWorldInfo(std::uint64_t mapIdentity,std::uint64_t completedSerial);
}
