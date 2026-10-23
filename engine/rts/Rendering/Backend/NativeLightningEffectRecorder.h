/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeLightningEffectData.h"

namespace rendering {
class NativeLightningEffectRecorder {
public:
	void Begin();
	void Record(NativeLightningEffectDraw draw);
	void SetFrameResources(const NativeLightningEffectFrame& resources);
	std::shared_ptr<const NativeLightningEffectFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeLightningEffectFrame> frame;
};
}
