/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// Explicit test target only; an engine source glob must not gain another main.
#if defined(METAL4_SURFACE_STATE_TEST_MAIN)
#include "Metal4WindowSurface.h"

#include <iostream>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_move_constructible_v<rendering::Metal4WindowSurface>);
static_assert(std::is_move_assignable_v<rendering::Metal4WindowSurface>);
static_assert(!std::is_copy_constructible_v<rendering::Metal4WindowSurface>);
static_assert(!std::is_copy_assignable_v<rendering::Metal4WindowSurface>);

namespace {
unsigned checks = 0;

void Check(bool value, const char* message)
{
	++checks;
	if (!value) throw std::runtime_error(message);
}

template<typename F> void Reject(F action)
{
	bool rejected = false;
	try { action(); } catch (const std::runtime_error&) { rejected = true; }
	Check(rejected, "Invalid state operation was not rejected");
}
} // namespace

int main()
{
	try {
		using rendering::Metal4SurfaceObservation;
		using rendering::Metal4SurfaceState;
		Metal4SurfaceState state(42, 1280, 720);
		Check(!state.Snapshot().CanPresent(), "Unobserved surface should not be presentable");
		Check(state.Snapshot().windowID == 42, "Window identity changed");
		Metal4SurfaceObservation observation{1280, 720, 2560, 1440, true, false};
		Check(state.Observe(observation), "Initial native observation was not recorded");
		Check(state.Snapshot().CanPresent(), "Visible observed surface should be presentable");
		Check(state.Snapshot().logicalWidth == 1280 && state.Snapshot().presentationBackingWidth == 2560, "Logical and backing dimensions were conflated");
		Check(state.Snapshot().renderWidth == 1280 && state.Snapshot().renderHeight == 720, "2x backing implicitly multiplied render pixels");
		const auto revision = state.Snapshot().revision;
		Check(!state.Observe(observation) && state.Snapshot().revision == revision, "Identical observation changed state");
		observation = {1600, 900, 3200, 1800, true, false};
		Check(state.Observe(observation), "2x backing resize was not observed");
		Check(state.Snapshot().renderWidth == 1280 && state.Snapshot().renderHeight == 720, "Resize implicitly changed requested render pixels");
		observation = {1600, 900, 1600, 900, true, false};
		Check(state.Observe(observation), "Move to 1x display was not observed");
		Check(state.Snapshot().renderWidth == 1280 && state.Snapshot().renderHeight == 720, "Display-scale change altered requested pixels");
		Check(state.SetRenderSize(1920, 1080), "Explicit pixel setter failed");
		Check(state.Snapshot().renderWidth == 1920 && state.Snapshot().renderHeight == 1080, "Explicit pixels were not retained");
		Check(state.Snapshot().logicalWidth == 1600 && state.Snapshot().presentationBackingWidth == 1600, "Pixel setter changed window/presentation metadata");
		Check(!state.SetRenderSize(1920, 1080), "Identical explicit pixels changed state");
		Reject([&] { state.SetRenderSize(0, 1080); });
		Reject([&] { state.SetRenderSize(1920, 4097); });
		Check(state.Snapshot().renderWidth == 1920 && state.Snapshot().renderHeight == 1080, "Rejected pixel setter partially mutated state");
		observation.minimized = true;
		Check(state.Observe(observation) && !state.Snapshot().CanPresent(), "Minimize did not suspend presentation");
		observation.minimized = false; observation.visible = false;
		Check(state.Observe(observation) && !state.Snapshot().CanPresent(), "Hidden window remained presentable");
		observation.visible = true; observation.presentationBackingWidth = 0;
		Check(state.Observe(observation) && !state.Snapshot().CanPresent(), "Zero native backing extent remained presentable");
		observation.presentationBackingWidth = 1600;
		Check(state.Observe(observation) && state.Snapshot().CanPresent(), "Restore did not resume presentation");
		Check(state.Snapshot().renderWidth == 1920 && state.Snapshot().renderHeight == 1080, "Minimize/restore changed explicit pixels");
		Check(state.RequestClose() && !state.Snapshot().CanPresent(), "Close request did not stop presentation eligibility");
		Check(!state.RequestClose(), "Repeated close should be idempotent");
		observation = {800, 600, 1600, 1200, true, false};
		state.Observe(observation);
		Check(state.Snapshot().closeRequested && !state.Snapshot().CanPresent(), "Late resize/restore resurrected a closing surface");
		state.Detach();
		const auto detachedRevision = state.Snapshot().revision;
		Check(!state.Snapshot().attached && !state.Snapshot().CanPresent(), "Detached surface remained eligible");
		state.Detach();
		Check(state.Snapshot().revision == detachedRevision, "Repeated detach changed state");
		Reject([&] { state.Observe(observation); });
		Reject([&] { state.SetRenderSize(640, 480); });
		Reject([&] { state.RequestClose(); });
		Reject([] { Metal4SurfaceState invalid(1, 0, 720); });
		Reject([] { Metal4SurfaceState invalid(1, 1280, 4097); });
		std::cout << "Metal surface CPU state tests passed: " << checks << " checks\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "Metal surface state test failed: " << error.what() << '\n';
		return 1;
	}
}
#endif // METAL4_SURFACE_STATE_TEST_MAIN
