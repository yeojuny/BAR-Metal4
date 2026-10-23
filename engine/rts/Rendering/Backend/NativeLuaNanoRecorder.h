/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeLuaNanoData.h"

namespace rendering {
void ValidateNativeLuaNanoDraw(const NativeLuaNanoDraw& draw);
class NativeLuaNanoRecorder {
public:
	void Begin();
	void Record(NativeLuaNanoDraw draw);
	void SetFrameResources(const NativeLuaNanoFrame& resources);
	std::shared_ptr<const NativeLuaNanoFrame> End();
	void Abort() noexcept;
	bool Active() const { return bool(frame); }
private:
	std::shared_ptr<NativeLuaNanoFrame> frame;
};
// This family gate is set only after the native translated program actually
// compiles and is connected. It is never a global GL4/shader/VBO capability.
void SetNativeLuaNanoConsumerAvailable(bool available) noexcept;
bool IsNativeLuaNanoConsumerAvailable() noexcept;
void SetNativeEngineNanoConsumerAvailable(bool available) noexcept;
bool IsNativeEngineNanoConsumerAvailable() noexcept;
}
