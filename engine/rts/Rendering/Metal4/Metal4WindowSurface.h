/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Metal4SurfaceState.h"
#include <memory>

struct SDL_Window;
union SDL_Event;

namespace rendering {

enum class Metal4SurfacePixelFormat { BGRA8Unorm, BGRA8UnormSRGB };

struct Metal4SurfaceConfig {
	uint32_t renderWidth = 1024;
	uint32_t renderHeight = 1024;
	Metal4SurfacePixelFormat pixelFormat = Metal4SurfacePixelFormat::BGRA8Unorm;
	bool displaySync = true;
	bool framebufferOnly = true;
};

struct Metal4SurfaceEventResult {
	bool relevant = false;
	bool stateChanged = false;
	bool closeRequested = false;
};

struct Metal4WindowSurfaceNativeAccess;

// The engine owns SDL video initialization, the SDL_Window and event pumping.
// This move-only object owns exactly one SDL_MetalView attached to that borrowed
// Metal window. Creation/event/update/native access/detach/destruction must run
// on the macOS main thread. Retire all GPU work, then destroy/detach this object,
// then SDL_DestroyWindow/SDL_Quit. Detach does not wait for or cancel GPU work.
class Metal4WindowSurface {
public:
	// Convenience entry point selects the default Metal device once. A shared
	// renderer can instead inject its existing device through private NativeAccess.
	Metal4WindowSurface(SDL_Window* engineWindow, const Metal4SurfaceConfig& config);
	~Metal4WindowSurface();
	Metal4WindowSurface(Metal4WindowSurface&&) noexcept;
	Metal4WindowSurface& operator=(Metal4WindowSurface&&) noexcept;
	Metal4WindowSurface(const Metal4WindowSurface&) = delete;
	Metal4WindowSurface& operator=(const Metal4WindowSurface&) = delete;

	// Forward events from the owner's existing loop; this does not poll, consume
	// or synthesize SDL events. Events for other windows/global quit are ignored.
	Metal4SurfaceEventResult HandleEvent(const SDL_Event& event);
	bool Refresh();
	bool SetRenderSize(uint32_t widthPixels, uint32_t heightPixels);
	bool RequestClose();
	Metal4SurfaceSnapshot Snapshot() const;
	void Detach();

private:
	struct Impl;
	explicit Metal4WindowSurface(std::unique_ptr<Impl> implementation);
	std::unique_ptr<Impl> impl;
	friend struct Metal4WindowSurfaceNativeAccess;
};

} // namespace rendering
