-- SPDX-License-Identifier: GPL-2.0-or-later
-- The pinned engine removed AdvSky; ISky now chooses SkyBox/ModernSky itself.
-- Execute every other original startup setting, including its exact order.
local path = "LuaIntro/springconfig.lua"
local source = VFS.LoadFile(path, VFS.ZIP_ONLY)
if type(source) ~= "string" then error("[BAR Metal4] Original startup settings unavailable") end
if Platform and Platform.renderBackend == "Metal4" and Platform.metal4Device == true then
	local expected = "bd1545f0f13bfba189a1f4759bc14d11644dc1b1145731c8dcb5cdc44b99210516875a17fb380e5f1dff92a65fab89a3b120dc2b3701e76beea6160555b5f89c"
	if (VFS.CalculateHash(source, 1) ~= expected and VFS.CalculateHash(source, 1) ~= "9995aaf5d2dcebd47f214fce189807c57110d87dabeb625935ae8bcbb416d35a99f6b54b5a1779bd725e653321dfd4e997c337fcd5a430de58bd98e6d9b60fa0" and VFS.CalculateHash(source, 1) ~= "edcdec0417d7baa1b10493f5b41a96d62f501e39e187dd4e0949ec61fdc53894a1c23961865f97b1dc525da0575b415c7e0e4974e61c8318d101c8a6cdce046f") then
		error("[BAR Metal4] Original startup settings identity changed; review required")
	end
	local old = 'Spring.SetConfigInt("AdvSky", 0)'
	local first, last = string.find(source, old, 1, true)
	if not first or string.find(source, old, last + 1, true) then
		error("[BAR Metal4] Obsolete sky-setting call identity changed")
	end
	source = string.sub(source, 1, first - 1)
		.. "-- AdvSky removed by the pinned engine; original ISky selects the sky."
		.. string.sub(source, last + 1)
end
local chunk, message = (loadstring or load)(source, "@" .. path)
if not chunk then error(message) end
if setfenv and getfenv then setfenv(chunk, getfenv(1)) end
return chunk()
