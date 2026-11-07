-- SPDX-License-Identifier: GPL-2.0-or-later
-- Delegate all original Options UI, callbacks and settings to the pinned game.
-- Native controls are exposed separately only for their real paired consumers.
local archivePath = "LuaUI/Widgets/gui_options.lua"
local expectedSourceSHA512 = "cb9350e30ef56513eb5a5ae40e01ca5ea807db19a299efb2c3fd606d5525f6619dd266172d612858002d0473b845087f0a63d6b5b0f14b4e0f8053c757848fec"
local source = VFS.LoadFile(archivePath, VFS.ZIP_ONLY)
if type(source) ~= "string" then
	error("[BAR Metal4] Original archive Options widget is unavailable")
end
local nativeHardware = Platform
	and Platform.renderBackend == "Metal4"
	and Platform.metal4Device == true
	and Platform.gpuVendor == "Apple"
	and Platform.gpuUnifiedMemory == true
	and Platform.gpuBCTextureCompression == true
if nativeHardware then
	local mapReady = type(gl.NativeTerrainShadingAvailable) == "function" and gl.NativeTerrainShadingAvailable()
	local cusReady = type(gl.NativeCUSMaterialAvailable) == "function" and gl.NativeCUSMaterialAvailable()
	local replacements = {
		{old = [=[if not Platform.glHaveGL4 then]=], new = [=[if barNativeOptions.hardware then
	isPotatoGpu = false
elseif not Platform.glHaveGL4 then]=], count = 1},
		{old = [=[if not isPotatoGpu and not devMode and not devUI then
		options[getOptionByID("advmapshading")] = nil
		Spring.SetConfigInt("AdvMapShading", 1)
		Spring.SendCommands("advmapshading 1")
	end]=], new = [=[if not isPotatoGpu and not devMode and not devUI then
		if not barNativeOptions.map then options[getOptionByID("advmapshading")] = nil end
		if not barNativeOptions.hardware then
			Spring.SetConfigInt("AdvMapShading", 1)
			Spring.SendCommands("advmapshading 1")
		end
	end]=], count = 1},
		{old = [=[if Spring.GetConfigInt("cus2", 1) ~= 1 then]=], new = [=[if not barNativeOptions.hardware and Spring.GetConfigInt("cus2", 1) ~= 1 then]=], count = 1},
		{old = [=[options[getOptionByID("cusgl4")] = nil]=], new = [=[if not barNativeOptions.cus then options[getOptionByID("cusgl4")] = nil end]=], count = 2},
	}

    local sourceIdentity = VFS.CalculateHash(source, 1)
    if sourceIdentity == "a5698ac222bd5a130a98b0ff524ddd9822e9742b68d320ba555dced78a9f4e31be54267a67f0dd43bae7bab5878af969fe72baaf1e37b59d012bacb00bbde319" then
        replacements = {
            {old = [=[		{
			id = "healthbarsscale",
			group = "ui",
			category = types.advanced,
			prefix = BAR.I18N("ui.settings.option.healthbars"),
			name = BAR.I18N("ui.settings.option.healthbarsscale"),
			type = "slider",
			min = 0.6,
			max = 2.0,
			step = 0.1,
			value = 1,
			description = "",
			widgetConfig = { "Health Bars GL4", "barScale", api = "healthbars", set = "setScale" },
		},
]=], new = [=[		{
			id = "healthbarsscale",
			group = "ui",
			category = types.advanced,
			prefix = BAR.I18N("ui.settings.option.healthbars"),
			name = BAR.I18N("ui.settings.option.healthbarsscale"),
			type = "slider",
			min = 0.6,
			max = 2.0,
			step = 0.1,
			value = (WG.healthbars and WG.healthbars.getScale and WG.healthbars.getScale()) or 2,
			description = "",
			widgetConfig = { "Health Bars GL4", "barScale", api = "healthbars", get = "getScale", set = "setScale" },
			onload = function(option)
				if WG.healthbars and WG.healthbars.getScale then option.value = WG.healthbars.getScale() end
			end,
		},
]=], count = 1},
            {old = [=[		{
			id = "healthbarsheight",
			parent = "healthbarsscale",
			group = "ui",
			category = types.advanced,
			name = BAR.I18N("ui.settings.option.healthbarsheight"),
			type = "slider",
			min = 0.7,
			max = 2,
			step = 0.1,
			value = (WG.healthbars ~= nil and WG.healthbars.getHeight() or 0.9),
			description = "",
			onload = function(option)
				loadWidgetData("Health Bars GL4", "healthbarsheight", { "barHeight" })
			end,
			onchange = function(option, value)
				saveOptionValue("Health Bars GL4", "healthbars", "setHeight", { "barHeight" }, value)
				widgetHandler:DisableWidget("Health Bars GL4")
				widgetHandler:EnableWidget("Health Bars GL4")
			end,
		},
]=], new = [=[		{
			id = "healthbarsheight",
			parent = "healthbarsscale",
			group = "ui",
			category = types.advanced,
			name = BAR.I18N("ui.settings.option.healthbarsheight"),
			type = "slider",
			min = 0.7,
			max = 2,
			step = 0.1,
			value = (WG.healthbars ~= nil and WG.healthbars.getHeight() or 2),
			description = "",
			onload = function(option)
				loadWidgetData("Health Bars GL4", "healthbarsheight", { "barHeight" })
				if WG.healthbars and WG.healthbars.getHeight then option.value = WG.healthbars.getHeight() end
			end,
			onchange = function(option, value)
				saveOptionValue("Health Bars GL4", "healthbars", "setHeight", { "barHeight" }, value)
				widgetHandler:DisableWidget("Health Bars GL4")
				widgetHandler:EnableWidget("Health Bars GL4")
			end,
		},
]=], count = 1},
            {old = [=[		{
			id = "vsync",
			group = "gfx",
			category = types.basic,
			name = BAR.I18N("ui.settings.option.vsync"),
			type = "select",
			options = {
				BAR.I18N("ui.settings.option.select_off"),
				BAR.I18N("ui.settings.option.select_enabled"),
				BAR.I18N("ui.settings.option.select_adaptive"),
			},
			value = 2,
			description = BAR.I18N("ui.settings.option.vsync_descr"),
			onload = function(option)
				local vsync = Spring.GetConfigInt("VSyncGame", -1)
				if vsync > 0 then
					option.value = 2
				elseif vsync < 0 then
					option.value = 3
				else
					option.value = 1
				end
			end,
			onchange = function(option, value)
				-- VSyncGame is the mode (1 on, -1 adaptive, 0 off), times VSyncFraction here and in the lobby
				local mode = 0
				if value == 2 then
					mode = 1
				elseif value == 3 then
					mode = -1
				end
				Spring.SetConfigInt("VSync", mode * Spring.GetConfigInt("VSyncFraction", 1))
				Spring.SetConfigInt("VSyncGame", mode) -- stored here as assurance cause lobby/game also changes vsync when idle and lobby could think game has set vsync 4 after a hard crash
			end,
		},
]=], new = [=[		{
			id = "vsync",
			group = "gfx",
			category = types.basic,
			name = BAR.I18N("ui.settings.option.vsync"),
			type = "select",
			options = {
				BAR.I18N("ui.settings.option.select_off"),
				BAR.I18N("ui.settings.option.select_enabled"),
				BAR.I18N("ui.settings.option.select_adaptive"),
			},
			value = 1,
			description = BAR.I18N("ui.settings.option.vsync_descr"),
			onload = function(option)
				local vsync = Spring.GetConfigInt("VSyncGame", -1)
				if vsync > 0 then
					option.value = 2
				elseif vsync < 0 then
					option.value = 3
				else
					option.value = 1
				end
			end,
			onchange = function(option, value)
				-- VSyncGame is the mode (1 on, -1 adaptive, 0 off), times VSyncFraction here and in the lobby
				local mode = 0
				if value == 2 then
					mode = 1
				elseif value == 3 then
					mode = -1
				end
				Spring.SetConfigInt("VSync", mode * Spring.GetConfigInt("VSyncFraction", 1))
				Spring.SetConfigInt("VSyncGame", mode) -- stored here as assurance cause lobby/game also changes vsync when idle and lobby could think game has set vsync 4 after a hard crash
			end,
		},
]=], count = 1},
            {old = [=[Spring.GetConfigInt("VSyncGame", -1)]=], new = [=[Spring.GetConfigInt("VSyncGame", 0)]=], count = 4},
            {old = [=[if not Platform.glHaveGL4 then]=], new = [=[if barNativeOptions.hardware then
    isPotatoGpu = false
elseif not Platform.glHaveGL4 then]=], count = 1},
            {old = [=[if not isPotatoGpu and not devMode and not devUI then
		removeOptions({ "advmapshading" })
		Spring.SetConfigInt("AdvMapShading", 1)
		Spring.SendCommands("advmapshading 1")
	end]=], new = [=[if not isPotatoGpu and not devMode and not devUI then
        if not barNativeOptions.map then removeOptions({ "advmapshading" }) end
        if not barNativeOptions.hardware then
            Spring.SetConfigInt("AdvMapShading", 1)
            Spring.SendCommands("advmapshading 1")
        end
    end]=], count = 1},
            {old = [=[if Spring.GetConfigInt("cus2", 1) ~= 1 then]=], new = [=[if not barNativeOptions.hardware and Spring.GetConfigInt("cus2", 1) ~= 1 then]=], count = 1},
            {old = [=[removeOptions({ "cusgl4", "water" })]=], new = [=[if barNativeOptions.cus then removeOptions({ "water" }) else removeOptions({ "cusgl4", "water" }) end]=], count = 1},
            {old = [=[removeOptions({ "cusgl4" })]=], new = [=[if not barNativeOptions.cus then removeOptions({ "cusgl4" }) end]=], count = 1},
        }
    end
    local matches = sourceIdentity == expectedSourceSHA512 or sourceIdentity == "a5698ac222bd5a130a98b0ff524ddd9822e9742b68d320ba555dced78a9f4e31be54267a67f0dd43bae7bab5878af969fe72baaf1e37b59d012bacb00bbde319"
	local candidate = source
	for _, patch in ipairs(replacements) do
		local at, count, parts = 1, 0, {}
		while true do
			local first, last = string.find(candidate, patch.old, at, true)
			if not first then parts[#parts + 1] = string.sub(candidate, at); break end
			parts[#parts + 1] = string.sub(candidate, at, first - 1)
			parts[#parts + 1] = patch.new
			at, count = last + 1, count + 1
		end
		if count ~= patch.count then matches = false end
		candidate = table.concat(parts)
	end
	if matches then
		source = "local barNativeOptions={hardware=true,map=" .. tostring(mapReady == true)
			.. ",cus=" .. tostring(cusReady == true) .. "}\n" .. candidate
		Spring.Echo("[BAR Metal4] Original Options: verified Apple Metal hardware profile; device="
			.. tostring(Platform.gpu) .. "; GL4 API support=" .. tostring(Platform.glHaveGL4))
		Spring.Echo("[BAR Metal4] Original Options native controls: terrain=" .. tostring(mapReady == true)
			.. "; unit materials=" .. tostring(cusReady == true))
	else
		Spring.Echo("[BAR Metal4] Options hardware/control adapter not applied: original source identity changed")
	end
end
local compile = loadstring or load
local nativeDisplayOptions
if type(Spring.GetNativeDisplayGeometry) == "function" then
	-- The engine verifies this wrapper; pin all RAW executable helpers it loads.
	local helperSHA512 = {
		native_display_options = "e16ea98d3e894cbc0175ffd166c464d9e08659dc8e2fbb43b6c586bb572fff33c5a1708b34ed766b725349e6cf537992faa704bc8b57c02015c994e7d4174a7e",
		native_display_patch = "eed306c90d740ff701f7b829c88863c287845e51a3b9f8f453cd4edce49aff26cc6affe737f5309c004df50cfa032f25dc8b2318f6f0481e2558eb196e47fba8",
	}
	local function include(name)
		local path = "LuaUI/NativeWidgets/Include/" .. name .. ".inc"
		local text = assert(VFS.LoadFile(path, VFS.RAW_ONLY), "Native display adapter unavailable: " .. path)
		assert(helperSHA512[name] and VFS.CalculateHash(text, 1) == helperSHA512[name],
			"Native display adapter identity changed: " .. path)
		local chunk, failure = compile(text, "@" .. path)
		if not chunk then error(failure) end
		if setfenv and getfenv then setfenv(chunk, getfenv(1)) end
		return chunk()
	end
	nativeDisplayOptions = include("native_display_options")
	source = include("native_display_patch")(source)
end
local chunk, loadError = compile(source, "@" .. archivePath .. " [native hardware/control adapter]")
if not chunk then error(loadError) end
if setfenv and getfenv then setfenv(chunk, getfenv(1)) end
local result = chunk(nativeDisplayOptions)
if nativeDisplayOptions then nativeDisplayOptions.installOverlay(widget) end
return result
