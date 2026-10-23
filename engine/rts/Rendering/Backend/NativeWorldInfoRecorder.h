/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeWorldInfoData.h"
namespace rendering {
class NativeWorldInfoRecorder {
public:
	NativeWorldInfoRecorder();
	void Reset();
	bool IsInitialized()const noexcept{return frame.modeGeneration!=0;}
	void SetMode(NativeWorldInfoMode,std::uint32_t x,std::uint32_t z,std::uint32_t px,std::uint32_t pz);
	// Called at every original InfoTextureHandler::Update, not minimap cadence.
	void Update(std::shared_ptr<const NativeMiniMapInfo> raw,std::uint32_t drawFrame);
	std::shared_ptr<const NativeWorldInfoFrame> Capture(std::shared_ptr<const NativeMetalInfoFrame> metal={}) const;
	void Retire(std::uint64_t mapIdentity,std::uint64_t completedSerial);
private:
	NativeWorldInfoFrame frame;
	std::shared_ptr<const NativeMiniMapInfo> previous;
	std::shared_ptr<const NativeMiniMapCoverage> previousRadarGround;
	bool previousRadarGroundGlobal=false;
};
}
