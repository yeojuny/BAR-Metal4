/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeMiniMapData.h"
namespace rendering {
// Engine-thread capture; never performs GL/GPU work or changes synced data.
class NativeMiniMapCapture {
public:
	std::shared_ptr<const NativeMiniMapBackground> Capture(const NativeMiniMapState& state);
	// PiP samples original raw LOS/radar independently of the current map view.
	// This captures the same viewer data without changing that global mode.
	std::shared_ptr<const NativeMiniMapBackground> CapturePiP(const NativeMiniMapState& state);
	void Reset();
private:
	std::shared_ptr<const NativeMiniMapBackground> Capture(const NativeMiniMapState& state, bool individualImages);
	std::shared_ptr<const NativeMiniMapBackground> cached;
	bool cachedIndividualImages = false;
};
}
