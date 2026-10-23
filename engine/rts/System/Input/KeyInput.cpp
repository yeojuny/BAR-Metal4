/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <algorithm>
#include <functional>
#include <cassert>
#include <cctype>
#include <set>

#ifdef RECOIL_USE_SDL3
#include "System/SDL3Compat/SDL3Helpers.h"
#else
#include <SDL_keyboard.h>
#include <SDL_events.h>
#include <SDL_stdinc.h>
#endif

#include "KeyInput.h"

/**
* @brief keys
*
* Array of possible keys, and which are being pressed
*/

namespace KeyInput {
	static       std::vector<Key> keyVec;
	static       std::vector<Key> scanVec;
	static const std::function<bool(const Key&, const Key&)> keyCmp = [](const Key& a, const Key& b) { return (a.first < b.first); };

	static SDL_Keymod keyMods;

	// keycodes forced down by debug.emulateKey*; re-applied on top of the SDL
	// poll at the end of every Update so they survive the re-poll
	static std::set<int> emulatedKeyCodes;


	bool IsKeyPressed(int keyCode) {
		const auto& pred = keyCmp;
		const auto  iter = std::lower_bound(keyVec.begin(), keyVec.end(), Key{keyCode, false}, pred);

		return (iter != keyVec.end() && iter->first == keyCode && iter->second);
	}

	bool IsScanPressed(int scanCode) {
		const auto& pred = keyCmp;
		const auto  iter = std::lower_bound(scanVec.begin(), scanVec.end(), Key{scanCode, false}, pred);

		return (iter != scanVec.end() && iter->first == scanCode && iter->second);
	}

	void SetScanPressed(int scanCode, bool isPressed) {
		const auto& pred = keyCmp;
		const auto  iter = std::lower_bound(scanVec.begin(), scanVec.end(), Key{scanCode, false}, pred);

		// not reachable for default modifiers
		if (iter == scanVec.end())
			return;

		iter->second = isPressed;
	}

	void SetKeyPressed(int keyCode, bool isPressed) {
		const auto& pred = keyCmp;
		const auto  iter = std::lower_bound(keyVec.begin(), keyVec.end(), Key{keyCode, false}, pred);

		// not reachable for default modifiers
		if (iter == keyVec.end())
			return;

		iter->second = isPressed;
	}

	// mark a code pressed, inserting it (keeping the vector sorted) if the poll
	// never created a slot for it -- headless has no SDL keyboard, so keyVec/scanVec
	// come back empty and plain SetKeyPressed would have nothing to flip
	static void ForcePressed(std::vector<Key>& vec, int code) {
		const auto iter = std::lower_bound(vec.begin(), vec.end(), Key{code, false}, keyCmp);

		if (iter != vec.end() && iter->first == code) {
			iter->second = true;
		} else {
			vec.insert(iter, Key{code, true});
		}
	}

	void SetKeyModState(int mod, bool isPressed) {
		if (isPressed) {
			keyMods = SDL_Keymod(keyMods | mod);
		} else {
			keyMods = SDL_Keymod(keyMods & ~mod);
		}
	}

	bool GetKeyModState(int mod) {
		return (keyMods & mod);
	}

	bool IsKeyEmulated(int keyCode) {
		return emulatedKeyCodes.contains(keyCode);
	}

	void SetKeyEmulated(int keyCode, bool pressed) {
		if (pressed) {
			emulatedKeyCodes.insert(keyCode);
		} else {
			emulatedKeyCodes.erase(keyCode);
		}
	}

	const std::set<int>& GetEmulatedKeys() {
		return emulatedKeyCodes;
	}

	void ClearEmulatedKeys() {
		emulatedKeyCodes.clear();
	}

	/**
	* Tests SDL keystates and sets values in key array
	*/
	void Update(int fakeMetaKey)
	{
		int numKeys = 0;
#ifdef RECOIL_USE_SDL3
		const bool* kbState = SDL_GetKeyboardState(&numKeys);
		static_assert(SDLK_A == 'a' && SDLK_Z == 'z'); // Existing engine/Lua ASCII identity.
#else
		const uint8_t* kbState = SDL_GetKeyboardState(&numKeys);
#endif

		keyMods = SDL_GetModState();

		keyVec.clear();
		keyVec.reserve(numKeys);
		scanVec.clear();
		scanVec.reserve(numKeys);

		for (int i = 0; i < numKeys; ++i) {
			const auto scanCode = (SDL_Scancode)i;
#ifdef RECOIL_USE_SDL3
			const auto keyCode = SDL_GetKeyFromScancode(scanCode, SDL_KMOD_NONE, false);
#else
			const auto keyCode  = SDL_GetKeyFromScancode(scanCode);
#endif

			keyVec.emplace_back(keyCode, kbState[scanCode] != 0);
			scanVec.emplace_back(scanCode, kbState[scanCode] != 0);
		}

		std::sort(keyVec.begin(), keyVec.end(), keyCmp);
		std::sort(scanVec.begin(), scanVec.end(), keyCmp);

		SetKeyModState(KMOD_GUI, IsKeyPressed(fakeMetaKey));
		SetKeyPressed(SDLK_LALT  , GetKeyModState(KMOD_ALT  ));
		SetKeyPressed(SDLK_LCTRL , GetKeyModState(KMOD_CTRL ));
		SetKeyPressed(SDLK_LGUI  , GetKeyModState(KMOD_GUI  ));
		SetKeyPressed(SDLK_LSHIFT, GetKeyModState(KMOD_SHIFT));
		SetKeyPressed(SDL_SCANCODE_LALT  , GetKeyModState(KMOD_ALT  ));
		SetKeyPressed(SDL_SCANCODE_LCTRL , GetKeyModState(KMOD_CTRL ));
		SetKeyPressed(SDL_SCANCODE_LGUI  , GetKeyModState(KMOD_GUI  ));
		SetKeyPressed(SDL_SCANCODE_LSHIFT, GetKeyModState(KMOD_SHIFT));

		// OR the emulated keys back in: the poll above only reflects real hardware,
		// so anything held via debug.emulateKey* has to be re-applied here to show
		// up in IsKeyPressed / GetKeyModState / GetPressedKeys
		for (const int keyCode: emulatedKeyCodes) {
			ForcePressed(keyVec, keyCode);
#ifdef RECOIL_USE_SDL3
			ForcePressed(scanVec, SDL_GetScancodeFromKey(SDL_Keycode(keyCode), nullptr));
#else
			ForcePressed(scanVec, SDL_GetScancodeFromKey((SDL_Keycode)keyCode));
#endif

			switch (keyCode) {
				case SDLK_LALT:   case SDLK_RALT:   SetKeyModState(KMOD_ALT  , true); break;
				case SDLK_LCTRL:  case SDLK_RCTRL:  SetKeyModState(KMOD_CTRL , true); break;
				case SDLK_LGUI:   case SDLK_RGUI:   SetKeyModState(KMOD_GUI  , true); break;
				case SDLK_LSHIFT: case SDLK_RSHIFT: SetKeyModState(KMOD_SHIFT, true); break;
			}
		}
	}

	const std::vector<Key>& GetPressedKeys()
	{
		return keyVec;
	}

	const std::vector<Key>& GetPressedScans()
	{
		return scanVec;
	}

	void ReleaseAllKeys()
	{
		for (const auto& key: keyVec) {
			auto keycode  = (SDL_Keycode)key.first;
#ifdef RECOIL_USE_SDL3
			auto scancode = SDL_GetScancodeFromKey(keycode, nullptr);
#else
			auto scancode = SDL_GetScancodeFromKey(keycode);
#endif

			if (keycode == SDLK_NUMLOCKCLEAR || keycode == SDLK_CAPSLOCK || keycode == SDLK_SCROLLLOCK)
				continue;

			if (!KeyInput::IsKeyPressed(keycode))
				continue;

			SDL_Event event{};
#ifdef RECOIL_USE_SDL3
			event.type = event.key.type = SDL_EVENT_KEY_UP;
			event.key.down = false;
			event.key.key = keycode;
			event.key.mod = SDL_KMOD_NONE;
			event.key.scancode = scancode;
			if (SDL_Window* focus = SDL_GetKeyboardFocus(); focus != nullptr)
				event.key.windowID = SDL_GetWindowID(focus);
#else
			event.type = event.key.type = SDL_KEYUP;
			event.key.state = SDL_RELEASED;
			event.key.keysym.sym = keycode;
			event.key.keysym.mod = 0;
			event.key.keysym.scancode = scancode;
#endif
			SDL_PushEvent(&event);
		}
	}
} // namespace KeyInput
