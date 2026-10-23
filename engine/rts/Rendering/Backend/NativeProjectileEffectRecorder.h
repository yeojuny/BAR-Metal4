/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeProjectileEffectData.h"
namespace rendering {
class NativeProjectileEffectRecorder {
public:
	void Begin();
	void Record(NativeProjectileEffectDraw draw);
	void SetFrameResources(const NativeProjectileEffectFrame& resources);
	std::shared_ptr<const NativeProjectileEffectFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeProjectileEffectFrame> frame;
};
void SetNativeProjectileEffectsConsumerAvailable(bool available) noexcept;
bool IsNativeProjectileEffectsConsumerAvailable() noexcept;
}
