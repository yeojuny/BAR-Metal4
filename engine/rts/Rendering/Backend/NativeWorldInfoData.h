/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeMiniMapData.h"
#include "Rendering/Backend/NativeMetalInfoData.h"
namespace rendering {
enum class NativeWorldInfoMode { Normal, Los, Metal };
struct NativeWorldInfoUpdate {
	std::uint64_t serial=0;
	bool clear=false; // original default/LOS clear(.5,.5,.5,1), including padded area
	std::shared_ptr<const NativeMiniMapInfo> inputs;
	// Original hash-table iteration updates info before radar before ground
	// LOS. The radar image consumed by this combiner used an older ground LOS.
	std::shared_ptr<const NativeMiniMapCoverage> radarGroundLos;
	bool radarGroundGlobalLos=false;
};
struct NativeWorldInfoFrame {
	std::uint64_t mapIdentity=0,modeGeneration=0,serial=0;
	std::uint32_t sourceDrawFrame=0,squaresX=0,squaresZ=0,paddedSquaresX=0,paddedSquaresZ=0;
	NativeWorldInfoMode mode=NativeWorldInfoMode::Normal;
	std::vector<NativeWorldInfoUpdate> updates;
	std::shared_ptr<const NativeMetalInfoFrame> metal;
};
// Image identity is independent of the last CPU update's draw stamp. Normal
// mode intentionally reuses its completed image; SetMode queues a real clear
// before the next periodic Update. Neither case requires a fabricated update.
inline bool SameNativeWorldInfoVersion(const NativeWorldInfoFrame& a,const NativeWorldInfoFrame& b)
{
	return a.mapIdentity==b.mapIdentity && a.modeGeneration==b.modeGeneration &&
		a.serial==b.serial && a.mode==b.mode && a.squaresX==b.squaresX &&
		a.squaresZ==b.squaresZ && a.paddedSquaresX==b.paddedSquaresX &&
		a.paddedSquaresZ==b.paddedSquaresZ;
}
void ValidateNativeWorldInfoFrame(const NativeWorldInfoFrame&);
}
