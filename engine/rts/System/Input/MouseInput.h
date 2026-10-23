/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef MOUSE_INPUT_H
#define MOUSE_INPUT_H

#ifdef RECOIL_USE_SDL3
// Preserve the engine camera macro around SDL3 camera-event declarations.
#pragma push_macro("camera")
#undef camera
#include <SDL3/SDL_events.h>
#pragma pop_macro("camera")
#else
#include <SDL_events.h>
#endif
#include "System/Input/InputHandler.h"

#include "System/type2.h"

class IMouseInput
{
public:
	static IMouseInput* GetInstance(bool relModeWarp);
	static void FreeInstance(IMouseInput*);

	IMouseInput() = default;
	IMouseInput(bool relModeWarp);
	virtual ~IMouseInput();

	virtual void InstallWndCallback() {}

	int2 GetPos() const { return mousepos; }

	bool SetPos(int2 pos);
	bool WarpPos(int2 pos);
	bool SetWarpPos(int2 pos) { return (SetPos(pos) && WarpPos(pos)); }

	bool HandleSDLMouseEvent(const SDL_Event& event);
#ifdef RECOIL_USE_SDL3
	void ResetRelativeMotion() { relativeRemainderX = relativeRemainderY = 0.0; }
#endif

	virtual void SetWMMouseCursor(void* wmcursor) {}

protected:
	int2 mousepos;
#ifdef RECOIL_USE_SDL3
	double relativeRemainderX = 0.0;
	double relativeRemainderY = 0.0;
#endif
	InputHandler::HandlerTokenT inputCon;
};

extern IMouseInput* mouseInput;

#endif
