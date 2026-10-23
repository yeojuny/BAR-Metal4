/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <memory>

namespace rendering {
class Metal4WindowSurface;
struct ScreenFrameView;

struct Metal4ScreenPresentationResult {
	bool presented = false;
	bool targetsCompleted = false;
	uint32_t draws = 0;
	uint32_t textureUploads = 0;
};

// Original screen-only controllers use the same native resource/command
// backend and borrowed window as the world. They need no loaded map/camera.
// Main-thread only. Every successful submission is retired before return;
// a failed, unretired submission follows the shared runtime's poison policy.
class Metal4ScreenPresenter {
public:
#ifdef RECOIL_METAL4_LIVE_SCENE
	static constexpr bool DefaultShaderValidation = false;
#else
	static constexpr bool DefaultShaderValidation = true;
#endif
	explicit Metal4ScreenPresenter(Metal4WindowSurface& surface,unsigned samples=1);
	~Metal4ScreenPresenter();
	Metal4ScreenPresentationResult Draw(const ScreenFrameView& frame, bool shaderValidation = DefaultShaderValidation);
	void ClearResources();
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rendering
