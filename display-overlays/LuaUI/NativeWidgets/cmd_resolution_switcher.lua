-- SPDX-License-Identifier: GPL-2.0-or-later
-- Delegate the original screen geometry and delayed window-border logic unchanged.
local path = "LuaUI/Widgets/cmd_resolution_switcher.lua"
local source = assert(VFS.LoadFile(path, VFS.ZIP_ONLY), "Original resolution switcher is unavailable")
if type(Spring.GetNativeDisplayGeometry) == "function" then
	local function replace(old, new)
		local first, last = source:find(old, 1, true)
		assert(first and not source:find(old, last + 1, true), "Native resolution switcher anchor changed")
		source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
	end
	replace("WG.screenMode.SetScreenMode = function(index)", "WG.screenMode.SetScreenMode = function(index, force)\n\t\tif force then firstPassDrawFrame = nil end")
	replace("if screenModeIndex ~= prevScreenmode then", "if force or screenModeIndex ~= prevScreenmode then")
	-- Zero preserves the engine's original automatic logical desktop mode.
	-- Physical archive videoMode sizes are never logical Retina window sizes.
	replace("Spring.SetWindowGeometry(screenMode.display, 0, 0, screenMode.width, screenMode.height, true, false)",
		"Spring.SetWindowGeometry(screenMode.display, 0, 0, 0, 0, true, false)")
	replace([=[elseif screenMode.type == windowType.borderless then
		Spring.SetWindowGeometry(
			screenMode.display,
			screenMode.x or 0,
			screenMode.y or 0,
			screenMode.width,
			screenMode.height,
			true,
			true
		)]=], [=[elseif screenMode.type == windowType.borderless then
		Spring.SetWindowGeometry(
			screenMode.display,
			screenMode.x or 0,
			screenMode.y or 0,
			0,
			0,
			true,
			true
		)]=])
end
local chunk, failure = (loadstring or load)(source, "@" .. path .. " [native mode adapter]")
if not chunk then error(failure) end
if setfenv and getfenv then setfenv(chunk, getfenv(1)) end
return chunk()
