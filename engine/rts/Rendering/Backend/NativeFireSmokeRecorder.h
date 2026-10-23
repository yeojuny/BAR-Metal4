/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeFireSmokeData.h"
namespace rendering {
class NativeFireSmokeRecorder {
public:
	void Begin();
	void Record(NativeFireSmokeDraw draw);
	void SetFrameResources(const NativeFireSmokeFrame& resources);
	std::shared_ptr<const NativeFireSmokeFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeFireSmokeFrame> frame;
};
// Publish only after real programs and the complete world-family hooks exist.
void SetNativeFireSmokeConsumerAvailable(bool available) noexcept;
bool IsNativeFireSmokeConsumerAvailable() noexcept;
}
