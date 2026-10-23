/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeShieldData.h"
namespace rendering {
class NativeShieldRecorder {
public:
	void Begin(std::uint64_t sourceDrawFrame);
	void Record(NativeShieldDraw);
	std::shared_ptr<const NativeShieldFrame> End();
	void Abort() noexcept;
	bool Active() const noexcept { return bool(frame); }
private:
	std::shared_ptr<NativeShieldFrame> frame;
};
void SetNativeShieldConsumerAvailable(bool) noexcept;
bool IsNativeShieldConsumerAvailable() noexcept;
} // namespace rendering
