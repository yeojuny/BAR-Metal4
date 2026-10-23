/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/*
	This workaround fixes the windows slow mouse movement problem
	(happens on full-screen mode + pressing keys).
	The code hacks around the mouse input from DirectInput,
	which SDL uses in full-screen mode.
	Instead it installs a window message proc and reads input from WM_MOUSEMOVE.
	On non-windows, the normal SDL events are used for mouse input

	new:
	It also workarounds a issue with SDL+windows and hardware cursors
	(->it has to block WM_SETCURSOR),
	so it is used now always even in window mode!

	newer:
	SDL_Event struct is used for new input handling.
	Several people confirmed its working.
*/


#include "MouseInput.h"
#include "InputHandler.h"

#include "Game/UI/MouseHandler.h"
#include "Rendering/GlobalRendering.h"
#include "System/MainDefines.h"
#include "System/SafeUtil.h"

#ifdef RECOIL_USE_SDL3
#include "System/SDL3Compat/SDL3Helpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#else
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_syswm.h>
#endif


IMouseInput* mouseInput = nullptr;

IMouseInput::IMouseInput(bool relModeWarp)
{
	inputCon = input.AddHandler([this](const SDL_Event& event) { return this->HandleSDLMouseEvent(event); });
#ifdef RECOIL_USE_SDL3
	// SDL3 uses real per-window relative input. The removed SDL2 warp hint
	// has no equivalent mode switch and is deliberately not sent to SDL3.
	(void)relModeWarp;
#endif
	#if !defined(RECOIL_USE_SDL3) && (!defined(HEADLESS) || defined(RECOIL_METAL4_LIVE_SCENE))
	// Windows 10 FCU (Fall Creators Update) causes spurious SDL_MOUSEMOTION
	// events to be generated with SDL_HINT_MOUSE_RELATIVE_MODE_WARP enabled
	//
	// while Spring did not previously set this hint and SDL defaults to raw
	// input, the update also affects MMB scrolling via SDL_WarpMouseInWindow
	// (our ancient manually implemented method of achieving relative motion)
	//
	// win32 SDL hides these events in 2.0.8 only if mouse->relative_mode_warp
	// (which configures relative mouse mode to *internally* use mouse warping
	// instead of raw input and gets toggled by SDL_SetRelativeMouseMode based
	// on the hint given here); the alternative to RMW would be to *duplicate*
	// the SDL patch in WarpPos
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, relModeWarp? "1": "0");
	#endif
}

IMouseInput::~IMouseInput()
{
	#if !defined(RECOIL_USE_SDL3) && (!defined(HEADLESS) || defined(RECOIL_METAL4_LIVE_SCENE))
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
	#endif
}


#ifdef RECOIL_USE_SDL3
namespace {
int LogicalMouseCoordinate(double value)
{
	return static_cast<int>(std::clamp(std::round(value), double(std::numeric_limits<int>::min()), double(std::numeric_limits<int>::max())));
}
int RelativeMouseDelta(float value, double& remainder)
{
	const double accumulated = double(value) + remainder;
	const int integral = LogicalMouseCoordinate(accumulated);
	remainder = accumulated < double(std::numeric_limits<int>::min()) || accumulated > double(std::numeric_limits<int>::max()) ? 0.0 : accumulated - integral;
	return integral;
}
}
#endif

bool IMouseInput::HandleSDLMouseEvent(const SDL_Event& event)
{
#ifdef RECOIL_USE_SDL3
	switch (event.type) {
		case SDL_EVENT_MOUSE_MOTION: {
			if (!std::isfinite(event.motion.x) || !std::isfinite(event.motion.y) || !std::isfinite(event.motion.xrel) || !std::isfinite(event.motion.yrel)) {
				ResetRelativeMotion();
				break;
			}
			mousepos = {LogicalMouseCoordinate(event.motion.x), LogicalMouseCoordinate(event.motion.y)};
			const int dx = RelativeMouseDelta(event.motion.xrel, relativeRemainderX);
			const int dy = RelativeMouseDelta(event.motion.yrel, relativeRemainderY);
			if (mouse != nullptr) mouse->MouseMove(mousepos.x, mousepos.y, dx, dy);
		} break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			if (!std::isfinite(event.button.x) || !std::isfinite(event.button.y)) break;
			mousepos = {LogicalMouseCoordinate(event.button.x), LogicalMouseCoordinate(event.button.y)};
			if (mouse == nullptr || mouse->IsButtonEmulated(event.button.button)) break;
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
				mouse->MousePress(mousepos.x, mousepos.y, event.button.button);
			else
				mouse->MouseRelease(mousepos.x, mousepos.y, event.button.button);
		} break;
		case SDL_EVENT_MOUSE_WHEEL: {
			if (mouse != nullptr && std::isfinite(event.wheel.y)) mouse->MouseWheel(event.wheel.y);
		} break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST: {
			ResetRelativeMotion();
		} break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER: {
			ResetRelativeMotion();
			if (mouse != nullptr) mouse->WindowEnter();
		} break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
			ResetRelativeMotion();
			mousepos = {
				globalRendering->viewPosX + (globalRendering->viewSizeX >> 1),
				globalRendering->viewWindowOffsetY + (globalRendering->viewSizeY >> 1)
			};
			if (mouse != nullptr) mouse->WindowLeave();
		} break;
	}
	return false;
#else

	switch (event.type) {
		case SDL_MOUSEMOTION: {
			mousepos = int2(event.motion.x, event.motion.y);

			if (mouse != nullptr)
				mouse->MouseMove(mousepos.x, mousepos.y, event.motion.xrel, event.motion.yrel);

		} break;
		case SDL_MOUSEBUTTONDOWN: {
			mousepos = int2(event.button.x, event.button.y);

			// suppress if the button is already held via input emulation
			if (mouse != nullptr && !mouse->IsButtonEmulated(event.button.button))
				mouse->MousePress(mousepos.x, mousepos.y, event.button.button);

		} break;
		case SDL_MOUSEBUTTONUP: {
			mousepos = int2(event.button.x, event.button.y);

			if (mouse != nullptr && !mouse->IsButtonEmulated(event.button.button))
				mouse->MouseRelease(mousepos.x, mousepos.y, event.button.button);

		} break;
		case SDL_MOUSEWHEEL: {
			if (mouse != nullptr)
				mouse->MouseWheel(event.wheel.y);

		} break;
		case SDL_WINDOWEVENT: {
			switch (event.window.event) {
				case SDL_WINDOWEVENT_ENTER: {
					if (mouse != nullptr)
						mouse->WindowEnter();
				} break;
				case SDL_WINDOWEVENT_LEAVE: {
					// mouse left window; set pos internally to view center-pixel to prevent endless scrolling
					mousepos = {
						globalRendering->viewPosX          + (globalRendering->viewSizeX >> 1),
						globalRendering->viewWindowOffsetY + (globalRendering->viewSizeY >> 1)
					};

					if (mouse != nullptr)
						mouse->WindowLeave();
				} break;
			}
		} break;
	}

	return false;
#endif
}

//////////////////////////////////////////////////////////////////////

#if defined(_WIN32) && !defined(HEADLESS)

class CWin32MouseInput : public IMouseInput
{
public:
	static CWin32MouseInput* inst;

	LONG_PTR sdl_wndproc;
	HWND wnd;
	HCURSOR hCursor;

	static LRESULT CALLBACK SpringWndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg) {
			case WM_SETCURSOR: {
				if (inst->hCursor != nullptr) {
					const Uint16 hittest = LOWORD(lParam);

					if (hittest == HTCLIENT) {
						SetCursor(inst->hCursor);
						return TRUE;
					}
				}
			} break;
		}
		return CallWindowProc((WNDPROC)inst->sdl_wndproc, wnd, msg, wParam, lParam);
	}

	void SetWMMouseCursor(void* wmcursor)
	{
		hCursor = (HCURSOR)wmcursor;
	}

	void InstallWndCallback()
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if (!SDL_GetWindowWMInfo(globalRendering->GetWindow(), &info))
			return;

		wnd = info.info.win.window;

		LONG_PTR cur_wndproc = GetWindowLongPtr(wnd, GWLP_WNDPROC);

		if (cur_wndproc != (LONG_PTR)SpringWndProc) {
			sdl_wndproc = GetWindowLongPtr(wnd, GWLP_WNDPROC);
			SetWindowLongPtr(wnd, GWLP_WNDPROC, (LONG_PTR)SpringWndProc);
		}
	}

	CWin32MouseInput(bool relModeWarp): IMouseInput(relModeWarp)
	{
		inst = this;
		hCursor = nullptr;
		sdl_wndproc = 0;
		wnd = 0;

		InstallWndCallback();
	}
	~CWin32MouseInput()
	{
		// reinstall the SDL window proc
		SetWindowLongPtr(wnd, GWLP_WNDPROC, sdl_wndproc);
	}
};

CWin32MouseInput* CWin32MouseInput::inst = nullptr;


alignas(CWin32MouseInput) static std::byte mouseInputMem[sizeof(CWin32MouseInput)];
#else
alignas(IMouseInput) static std::byte mouseInputMem[sizeof(IMouseInput)];
#endif



#if 1
static SDL_Event events[100];
#endif

bool IMouseInput::SetPos(int2 pos)
{
	if (!globalRendering->active)
		return false;

	// calling SDL_WarpMouse at 300fps eats ~5% cpu usage, so only update when needed
	if (pos.x == mousepos.x && pos.y == mousepos.y)
		return false;

	return (mousepos = pos, true);
}

bool IMouseInput::WarpPos(int2 pos)
{
	#if __unix__
		/* Needed for SDL2+Wayland where warping isn't allowed otherwise, works fine with X11.
		 * One would think there should be a corresponding `SDL_ShowCursor(SDL_ENABLE);` below,
		 * but apparently this prevents this work-around from working (?!). */
		#ifdef RECOIL_USE_SDL3
		SDL_HideCursor();
		#else
		SDL_ShowCursor(SDL_DISABLE);
		#endif
	#endif

	SDL_WarpMouseInWindow(globalRendering->GetWindow(), pos.x, pos.y);

	// SDL_WarpMouse generates SDL_MOUSEMOTION events
	// in `middle click scrolling` those SDL generated ones would point into
	// the opposite direction the user moved the mouse, and so events would
	// cancel each other -> camera wouldn't move at all or jitter
	// need to catch the SDL generated events and delete them from its queue
	//
	// NOTE [2018]:
	//   the above comment dates back to 2010, but also describes the recent
	//   Windows 10 FCU bug with relative mode warping which similarly relies
	//   on WMIW
	#if 1
	SDL_PumpEvents();
	#ifdef RECOIL_USE_SDL3
	ResetRelativeMotion();
	SDL_PeepEvents(&events[0], sizeof(events) / sizeof(events[0]), SDL_GETEVENT, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_MOTION);
#else
	SDL_PeepEvents(&events[0], sizeof(events) / sizeof(events[0]), SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION);
#endif
	#else
	// should be equivalent, but for some reason is not
	SDL_FlushEvent(SDL_MOUSEMOTION);
	#endif

	return true;
}



IMouseInput* IMouseInput::GetInstance(bool relModeWarp)
{
	if (mouseInput == nullptr) {
#if defined(_WIN32) && !defined(HEADLESS)
		mouseInput = new (mouseInputMem) CWin32MouseInput(relModeWarp);
#else
		mouseInput = new (mouseInputMem) IMouseInput(relModeWarp);
#endif
	}

	return mouseInput;
}

void IMouseInput::FreeInstance(IMouseInput* mouseInp) {
	assert(mouseInp == mouseInput);
	spring::SafeDestruct(mouseInp);
	memset(mouseInputMem, 0, sizeof(mouseInputMem));
	mouseInput = nullptr;
}

