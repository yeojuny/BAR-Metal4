/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#if !defined(RECOIL_USE_SDL3)
#error "SDL3 source helpers require RECOIL_USE_SDL3"
#endif

#ifndef SDL_ENABLE_OLD_NAMES
#define SDL_ENABLE_OLD_NAMES 1
#endif

// Camera.h's engine macro must not rewrite SDL3's camera declarations.
#pragma push_macro("camera")
#undef camera
#include <SDL3/SDL.h>
#pragma pop_macro("camera")

namespace recoil::sdl3 {

struct VersionInfo {
	int major;
	int minor;
	int patch;
};

inline VersionInfo GetVersionInfo()
{
	const int version = SDL_GetVersion();
	return {SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version)};
}

inline int DisplayCount()
{
	int count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	SDL_free(displays);
	return count;
}

inline SDL_DisplayID DisplayIdFromIndex(int index)
{
	int count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	const SDL_DisplayID id = (displays != nullptr && index >= 0 && index < count) ? displays[index] : 0;
	SDL_free(displays);
	return id;
}

inline int DisplayIndexFromId(SDL_DisplayID id)
{
	int count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	int index = -1;
	for (int i = 0; displays != nullptr && i < count; ++i) {
		if (displays[i] == id) {
			index = i;
			break;
		}
	}
	SDL_free(displays);
	return index;
}

} // namespace recoil::sdl3
