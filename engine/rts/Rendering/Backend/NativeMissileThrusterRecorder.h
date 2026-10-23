/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeMissileThrusterData.h"
namespace rendering {
class NativeMissileThrusterRecorder {
public:
	void Begin();
	void Record(NativeMissileThrusterDraw draw);
	void SetFrameResources(const NativeMissileThrusterFrame& resources);
	std::shared_ptr<const NativeMissileThrusterFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeMissileThrusterFrame> frame;
};
// Publish only after real programs and the complete world-family hooks exist.
void SetNativeMissileThrusterConsumerAvailable(bool available) noexcept;
bool IsNativeMissileThrusterConsumerAvailable() noexcept;
}
