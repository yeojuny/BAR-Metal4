/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <string>

#include "VerticalSync.h"
#include "GL/myGL.h"
#include "System/SpringMath.h"
#include "System/Config/ConfigHandler.h"
#include "System/Log/ILog.h"

#include <SDL_video.h>

static constexpr int MAX_ADAPTIVE_INTERVAL = -6;
static constexpr int MAX_STANDARD_INTERVAL = +6;

static CVerticalSync instance;


#if defined(RECOIL_METAL4_LIVE_SCENE)
static constexpr int DEFAULT_VSYNC = 0;
CONFIG(int, VSyncGame).defaultValue(0).description("Preferred game display synchronization mode; 0 off, positive standard, negative adaptive.");
#else
static constexpr int DEFAULT_VSYNC = -1;
#endif
CONFIG(int, VSync).
	defaultValue(DEFAULT_VSYNC).
	minimumValue(MAX_ADAPTIVE_INTERVAL).
	maximumValue(MAX_STANDARD_INTERVAL).
	description(
		"Synchronize buffer swaps with vertical blanking interval."
		" Modes are -N (adaptive), +N (standard), or 0 (disabled)."
	);

CVerticalSync* CVerticalSync::GetInstance()
{
	return &instance;
}

void CVerticalSync::WrapNotifyOnChange()
{
#if defined(RECOIL_METAL4_LIVE_SCENE)
	// Before SDL/window creation: discard transient idle VSync, preserving the
	// user's preferred game mode/fraction. No notification or persistence here.
	const long long requested = static_cast<long long>(configHandler->GetInt("VSyncGame")) *
		configHandler->GetIntSafe("VSyncFraction", 1);
	const int initialInterval = static_cast<int>(std::clamp(requested,
		static_cast<long long>(MAX_ADAPTIVE_INTERVAL), static_cast<long long>(MAX_STANDARD_INTERVAL)));
	configHandler->Set("VSync", initialInterval, true, false);
#endif
	configHandler->NotifyOnChange(this, {"VSync"});
}

void CVerticalSync::WrapRemoveObserver()
{
	// can't do this in the dtor because VerticalSync outlives configHandler
	configHandler->RemoveObserver(this);
}

void CVerticalSync::ConfigNotify(const std::string& key, const std::string& value)
{
	SetInterval(configHandler->GetInt("VSync"));
}


void CVerticalSync::Toggle()
{
	// no-arg switch, select smallest interval
	#if defined(RECOIL_USE_SDL3)
	int swapInterval = 0;
	SDL_GL_GetSwapInterval(&swapInterval);
	#else
	const int swapInterval = SDL_GL_GetSwapInterval();
	#endif
	switch (std::clamp(swapInterval, -1, 1)) {
		case -1: { SetInterval( 0); } break;
		case  0: { SetInterval(+1); } break;
		case +1: { SetInterval(-1); } break;
		default: {} break;
	}
}

void CVerticalSync::SetInterval() { SetInterval(configHandler->GetInt("VSync")); }
void CVerticalSync::SetInterval(int i)
{
	// recursion is already prevented (Set only notifies on changed
	// values), this just avoids making the SDL calls a second time
	if ((i = std::clamp(i, MAX_ADAPTIVE_INTERVAL, MAX_STANDARD_INTERVAL)) == interval)
		return;

	configHandler->Set("VSync", interval = i);

	#if defined HEADLESS
	return;
	#endif

	// adaptive (delay swap iff frame-rate > vblank-rate)
	#if defined(RECOIL_USE_SDL3)
	const auto setSwapInterval = [](int value) { return SDL_GL_SetSwapInterval(value); };
	#else
	const auto setSwapInterval = [](int value) { return SDL_GL_SetSwapInterval(value) == 0; };
	#endif
	if (interval < 0 && setSwapInterval(interval)) {
		LOG("[VSync::%s] interval=%d (adaptive)", __func__, interval);
		return;
	}
	// standard (<interval> vblanks per swap)
	if (interval > 0 && setSwapInterval(interval)) {
		LOG("[VSync::%s] interval=%d (standard)", __func__, interval);
		return;
	}

	// disabled (never wait for vblank)
	SDL_GL_SetSwapInterval(0);
	LOG("[VSync::%s] interval=%d (disabled)", __func__, interval);
}
