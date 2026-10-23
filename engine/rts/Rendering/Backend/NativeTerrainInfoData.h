/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldInfoData.h"
namespace rendering {
inline constexpr uint32_t NativeTerrainInfoEnabledFlag=16384u;
inline constexpr uint32_t NativeTerrainInfoLosFlag=32768u;
// Original SMFRenderState::Enable: HAVE_INFOTEX=IsEnabled();
// infoTexIntensityMul=1+InMetalMode(). The fragment subtracts .5 only at1.
constexpr uint32_t NativeTerrainInfoFlags(NativeWorldInfoMode mode) {
 return mode==NativeWorldInfoMode::Normal?0u:NativeTerrainInfoEnabledFlag|
  (mode==NativeWorldInfoMode::Los?NativeTerrainInfoLosFlag:0u);
}
constexpr float NativeTerrainInfoIntensity(NativeWorldInfoMode mode) {
 return mode==NativeWorldInfoMode::Metal?2.f:1.f;
}
} // namespace rendering
