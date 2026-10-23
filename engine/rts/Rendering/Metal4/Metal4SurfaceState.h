/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <stdexcept>

namespace rendering {

struct Metal4SurfaceSnapshot {
	uint32_t windowID = 0;
	uint32_t logicalWidth = 0;
	uint32_t logicalHeight = 0;
	uint32_t presentationBackingWidth = 0;
	uint32_t presentationBackingHeight = 0;
	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
	uint64_t revision = 0;
	bool attached = true;
	bool visible = false;
	bool minimized = false;
	bool closeRequested = false;

	bool CanPresent() const
	{
		return attached && visible && !minimized && !closeRequested &&
			logicalWidth > 0 && logicalHeight > 0 &&
			presentationBackingWidth > 0 && presentationBackingHeight > 0;
	}
};

struct Metal4SurfaceObservation {
	uint32_t logicalWidth = 0;
	uint32_t logicalHeight = 0;
	uint32_t presentationBackingWidth = 0;
	uint32_t presentationBackingHeight = 0;
	bool visible = false;
	bool minimized = false;
};

// Pure CPU state used by the actual surface. Window/backing changes never infer
// render dimensions; only the owner's explicit pixel setter may change them.
class Metal4SurfaceState {
public:
	Metal4SurfaceState(uint32_t windowID, uint32_t renderWidth, uint32_t renderHeight)
	{
		ValidateRenderSize(renderWidth, renderHeight);
		state.windowID = windowID;
		state.renderWidth = renderWidth;
		state.renderHeight = renderHeight;
	}

	const Metal4SurfaceSnapshot& Snapshot() const { return state; }

	bool Observe(const Metal4SurfaceObservation& observation)
	{
		RequireAttached();
		const bool changed = state.logicalWidth != observation.logicalWidth || state.logicalHeight != observation.logicalHeight ||
			state.presentationBackingWidth != observation.presentationBackingWidth || state.presentationBackingHeight != observation.presentationBackingHeight ||
			state.visible != observation.visible || state.minimized != observation.minimized;
		state.logicalWidth = observation.logicalWidth;
		state.logicalHeight = observation.logicalHeight;
		state.presentationBackingWidth = observation.presentationBackingWidth;
		state.presentationBackingHeight = observation.presentationBackingHeight;
		state.visible = observation.visible;
		state.minimized = observation.minimized;
		state.revision += changed;
		return changed;
	}

	bool SetRenderSize(uint32_t width, uint32_t height)
	{
		RequireAttached();
		ValidateRenderSize(width, height);
		const bool changed = state.renderWidth != width || state.renderHeight != height;
		state.renderWidth = width;
		state.renderHeight = height;
		state.revision += changed;
		return changed;
	}

	bool RequestClose()
	{
		RequireAttached();
		const bool changed = !state.closeRequested;
		state.closeRequested = true;
		state.revision += changed;
		return changed;
	}

	void Detach()
	{
		if (!state.attached) return;
		state.attached = false;
		state.visible = false;
		++state.revision;
	}

private:
	static void ValidateRenderSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > 4096 || height > 4096)
			throw std::runtime_error("Metal surface render size requires explicit pixels in 1..4096");
	}

	void RequireAttached() const
	{
		if (!state.attached)
			throw std::runtime_error("Metal surface is detached");
	}

	Metal4SurfaceSnapshot state;
};

} // namespace rendering
