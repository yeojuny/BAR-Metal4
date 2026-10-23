/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <SDL3/SDL_video.h>
#include <array>

namespace recoil::mac {

struct DisplayGeometry {
	SDL_DisplayID id = 0;
	SDL_Rect bounds{};
	SDL_Rect usable{};
	SDL_Rect safe{};
	SDL_Rect BorderlessBounds() const;
};

// Optional launch-only display hint from the lobby: full logical desktop bounds,
// never an Electron/SDL monitor index, work area, safe viewport or pixel extent.
// Zero means absent, malformed or no longer matched; retain the original fallback.
SDL_DisplayID RequestedLaunchDisplay();

// Coordinates are SDL logical points. Cocoa backing scale is never a render
// multiplier. A null window first uses a matched launcher hint, then the mouse.
DisplayGeometry QueryDisplayGeometry(SDL_Window* window, SDL_DisplayID display = 0);
SDL_Rect ScaledSafeBounds(const DisplayGeometry& display, int width, int height);
SDL_DisplayMode SelectFullscreenMode(const DisplayGeometry& display, int width, int height);
SDL_Rect WindowSafeViewport(SDL_Window* window);
std::array<int, 4> WindowBorders(SDL_Window* window); // top, left, bottom, right
void ApplyWindowGeometry(SDL_Window* window, bool fullscreen, bool borderless,
	int x, int y, int width, int height, int minimumWidth, int minimumHeight);

} // namespace recoil::mac
