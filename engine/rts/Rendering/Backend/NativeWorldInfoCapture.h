/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldInfoData.h"
#include <span>
#include <string>
namespace rendering {
void ResetNativeWorldInfo();
void SelectNativeWorldInfoMode(const std::string& mode);
void UpdateNativeWorldInfo(); // exact original handler Update, including hidden windows
// Engine capture thread after LOS writers join. Reads the supplied live values,
// not a recorded WorldInfo update. Slots: 0 ground, 1 air, 2 radar, 3 jammer.
std::shared_ptr<const NativeMiniMapCoverage> CaptureNativeWorldInfoCoverage(unsigned slot,const void* owner,
	std::span<const std::uint16_t> values,std::uint32_t width,std::uint32_t height);
std::shared_ptr<const NativeWorldInfoFrame> CaptureNativeWorldInfo();
void RetireNativeWorldInfo(std::uint64_t mapIdentity,std::uint64_t completedSerial);
}
