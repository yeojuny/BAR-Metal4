/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _GLOBAL_RENDERING_INFO_H
#define _GLOBAL_RENDERING_INFO_H

#ifdef RECOIL_USE_SDL3
#include "System/SDL3Compat/SDL3Helpers.h"
#else
#include <SDL_version.h>
#endif
#include "System/type2.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/RenderTypes.h"
#endif

#include <vector>

struct GlobalRenderingInfo {
	struct AvailableVideoMode {
		std::string displayName;
		int32_t displayIndex;
		int32_t width;
		int32_t height;
		int32_t bpp;
		int32_t refreshRate;
	};

	const char* gpuName;
	const char* gpuVendor;

#ifdef RECOIL_METAL4_LIVE_SCENE
	// Own the queried name for the lifetime of the exported C-string pointer.
	// Native device facts are distinct from legacy GL API support flags.
	rendering::DeviceCapabilities nativeDevice;
#endif

	const char* glVersion;
	const char* glVendor;
	const char* glRenderer;
	const char* glslVersion;
	const char* gladVersion;

	const char* sdlDriverName;

	std::array<char, 256> glVersionShort = { 0 };
	std::array<char, 256> glslVersionShort = { 0 };

	int glVersionNum = 0;
	int glslVersionNum = 0;

	bool glContextIsCore;
	int2 glContextVersion;
	int2 gpuMemorySize;

#ifdef RECOIL_USE_SDL3
	recoil::sdl3::VersionInfo sdlVersionCompiled;
	recoil::sdl3::VersionInfo sdlVersionLinked;
#else
	SDL_version sdlVersionCompiled;
	SDL_version sdlVersionLinked;
#endif

	std::vector<AvailableVideoMode> availableVideoModes;
};

extern GlobalRenderingInfo globalRenderingInfo;

#endif

