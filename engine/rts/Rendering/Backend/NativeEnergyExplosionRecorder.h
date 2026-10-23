/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeEnergyExplosionData.h"
namespace rendering {
class NativeEnergyExplosionRecorder {
public:
	void Begin();
	void Record(NativeEnergyExplosionDraw);
	void SetFrameResources(const NativeEnergyExplosionFrame&);
	std::shared_ptr<const NativeEnergyExplosionFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept {return bool(frame);}
private:
	std::shared_ptr<NativeEnergyExplosionFrame> frame;
};
}
