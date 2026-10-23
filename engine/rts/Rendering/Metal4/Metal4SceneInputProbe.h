/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace rendering::scene {

enum class InputProbeState { Disabled, Waiting, Running, Passed, Failed };

struct InputProbeResult {
	InputProbeState state = InputProbeState::Disabled;
	std::string phase = "disabled";
	int unitID = -1;
	std::uint64_t simulationFrame = 0;
	std::string reason;
};

// Opt-in isolated integration evidence, not human/gameplay acceptance. Only
// RECOIL_METAL4_SCENE_INPUT_PROBE=1 enables it. The caller owns the SDL3 window
// and invokes both methods on its normal main-thread engine event/frame path.
// Input enters solely through SDL_PushEvent; no camera, selection, command or
// unit mutators are called. Ordinary query helpers may update derived caches.
class Metal4SceneInputProbe {
public:
	explicit Metal4SceneInputProbe(SDL_Window* window);
	~Metal4SceneInputProbe();
	Metal4SceneInputProbe(const Metal4SceneInputProbe&) = delete;
	Metal4SceneInputProbe& operator=(const Metal4SceneInputProbe&) = delete;

	const InputProbeResult& Update();
	void ObserveEvent(const SDL_Event& event);
	const InputProbeResult& Result() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace rendering::scene
