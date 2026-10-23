/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "System/Sync/SHA512.hpp"
#include <stdexcept>
#include <string>
#include <string_view>

// Private unsynced LuaMenu text adaptation. The original VFS selector and
// original full widget remain the CPU owner. No global Fullscreen API policy.
namespace NativeLuaMenuDisplayAdapter {
inline constexpr std::string_view path = "luamenu/widgets/gui_settings_window.lua";
inline constexpr std::string_view sourceSHA512 = "cf793c7e2ebf1a5b8b13cf190e1f6275717b23ada03692932413c39e6dfb57a79718c70b6d46b1cfbedf279e4f244b115b5cb67b60a93d3c373a7de03183d496";

inline bool Adapt(std::string& source)
{
	sha512::raw_digest raw;
	sha512::hex_digest hex{};
	sha512::calc_digest({source.data(), source.data() + source.size()}, raw);
	sha512::dump_digest(raw, hex);
	// A user-selected/custom or different-version widget is not this source.
	// Preserve the VFS-selected bytes rather than substituting the pinned menu.
	if (std::string_view(hex.data()) != sourceSHA512) return false;
	const auto replaceOnce = [&](std::string_view before, std::string_view after) {
		const auto position = source.find(before);
		if (position == std::string::npos || source.find(before, position + before.size()) != std::string::npos)
			throw std::runtime_error("Pinned native LuaMenu display adaptation requires source review");
		source.replace(position, before.size(), after);
	};
	replaceOnce(R"display(local currentMode = false
)display", R"display(local currentMode = false
local currentModeInLobby, currentGameManualFullscreen
)display");
	replaceOnce(R"display(	if mode == currentMode and (not borderOverride) then
		if not ((mode == 4 or mode == 6) and ManualBorderlessChange((mode == 4 and "manualBorderless") or "manualWindowed")) then
			return
		end
	end

	local Configuration = WG.Chobby.Configuration
	local prevMode = currentMode
	-- All non-trivial mode transitions need a delayed retry because
	-- SetConfigInt bounces may not fully apply within a single frame.
	local needAgressiveSetting = prevMode and (prevMode ~= mode)
)display", R"display(	local Configuration = WG.Chobby.Configuration
	-- The donor normalizes only the new-game command. Its explicit key comes
	-- from game_settings (with its fixed override), never the current cfg file.
	local gameManualFullscreen
	if mode == 4 and not inLobby then
		local explicit = Configuration.game_settings.Fullscreen
		local settingsOverride = Configuration.fixedSettingsOverride
		gameManualFullscreen = explicit ~= nil and ((settingsOverride and settingsOverride.Fullscreen) or explicit) or 0
	end
	local manualContextChanged = mode == 4 and
		(currentModeInLobby ~= inLobby or currentGameManualFullscreen ~= gameManualFullscreen)
	if mode == currentMode and (not borderOverride) and not manualContextChanged then
		if not ((mode == 4 or mode == 6) and ManualBorderlessChange((mode == 4 and "manualBorderless") or "manualWindowed")) then
			return
		end
	end

	local prevMode = currentMode
	-- All non-trivial mode transitions need a delayed retry because
	-- SetConfigInt bounces may not fully apply within a single frame.
	local needAgressiveSetting = prevMode and (prevMode ~= mode or manualContextChanged)
)display");
	replaceOnce(R"display(	currentMode = mode

	if Configuration.doNotSetAnySpringSettings then
)display", R"display(	currentMode = mode
	currentModeInLobby = inLobby
	currentGameManualFullscreen = gameManualFullscreen

	if Configuration.doNotSetAnySpringSettings then
)display");
	replaceOnce(R"display(		Spring.SetConfigInt("WindowBorderless", (mode == 4 and 1) or 0, false)
		Spring.SetConfigInt("Fullscreen", (mode == 4 and 1) or 0)
)display", R"display(		Spring.SetConfigInt("WindowBorderless", (mode == 4 and 1) or 0, false)
		local fullscreen = (mode == 4 and 1) or 0
		if mode == 4 and not inLobby then fullscreen = gameManualFullscreen end
		Spring.SetConfigInt("Fullscreen", fullscreen)
)display");
	return true;
}
} // namespace NativeLuaMenuDisplayAdapter
