-- SPDX-License-Identifier: GPL-2.0-or-later
-- Native presentation controls; original BAR callbacks retain window/input ownership.
local M = {}
local function enabled()
	return type(Spring.GetNativeDisplayGeometry) == "function"
end
local function selectedMode()
	if Spring.GetConfigInt("WindowBorderless", 0) == 1 then return 3 end
	return Spring.GetConfigInt("Fullscreen", 0) == 1 and 1 or 2
end
local function savedKey(name, display)
	return "Native" .. name .. "Display" .. display
end

function M.configure(options, modes, displays, selectedDisplay, category)
	if not enabled() or not WG.screenMode then return end
	local resolution, displayOption, resolutionAt
	for i, option in ipairs(options) do
		if option.id == "resolution" then resolution, resolutionAt = option, i end
		if option.id == "display" then displayOption = option end
	end
	if not resolution then return end
	local appliedDisplay = selectedDisplay
	local activeMode = selectedMode()
	local choices = {}
	local modeOption = {
		id = "native_display_mode", parent = "display", group = "gfx", category = category,
		name = "Screen mode", type = "select", segmented = true,
		options = { BAR.I18N("ui.resolutionswitcher.fullscreen"), BAR.I18N("ui.resolutionswitcher.window"), BAR.I18N("ui.resolutionswitcher.borderless") },
		value = activeMode,
	}
	local function modeIndex(kind)
		for index, mode in ipairs(modes) do
			if mode.display == selectedDisplay and mode.type == kind then return index end
		end
	end
	local function refresh()
		choices, resolution.options = {}, {}
		local function add(w, h, index, label)
			for _, choice in ipairs(choices) do
				if choice.width == w and choice.height == h then return end
			end
			choices[#choices + 1] = { width = w, height = h, index = index }
			resolution.options[#choices] = label or (w .. " × " .. h)
		end
		if activeMode == 1 then
			add(0, 0, modeIndex(1), "Automatic")
			add(1920, 1080, modeIndex(1))
			add(2560, 1440, modeIndex(1))
			add(3840, 2160, modeIndex(1))
			local w = Spring.GetConfigInt(savedKey("FullscreenRenderWidth", selectedDisplay), -1)
			local h = Spring.GetConfigInt(savedKey("FullscreenRenderHeight", selectedDisplay), -1)
			if w < 0 or h < 0 then
				w = selectedDisplay == appliedDisplay and Spring.GetConfigInt("NativeFullscreenRenderWidth", 0) or 0
				h = selectedDisplay == appliedDisplay and Spring.GetConfigInt("NativeFullscreenRenderHeight", 0) or 0
			end
			if w > 0 and h > 0 and w <= 4096 and h <= 4096 then add(w, h, modeIndex(1)) end
			resolution.value = 1
			for i, choice in ipairs(choices) do
				if choice.width == w and choice.height == h then resolution.value = i end
			end
		elseif activeMode == 2 then
			local w = Spring.GetConfigInt(savedKey("WindowedWidth", selectedDisplay), -1)
			local h = Spring.GetConfigInt(savedKey("WindowedHeight", selectedDisplay), -1)
			if selectedMode() == 2 and selectedDisplay == appliedDisplay then
				local cw, ch, _, _, top, left, bottom, right = Spring.GetWindowGeometry()
				w, h = cw + left + right, ch + top + bottom
			end
			for index, mode in ipairs(modes) do
				if mode.display == selectedDisplay and mode.type == 3 then add(mode.width, mode.height, index) end
			end
			resolution.value = 1
			for i, choice in ipairs(choices) do
				if choice.width == w and choice.height == h then resolution.value = i end
			end
		else
			local index = modeIndex(2)
			if index then add(modes[index].width, modes[index].height, index, "Automatic") end
			resolution.value = 1
		end
		-- Empty native enumeration must never create a selectable invalid value.
		if #choices == 0 then resolution.options = { "Unavailable" }; resolution.value = 1 end
	end
	local function applyChoice(force)
		local choice = choices[resolution.value]
		if not choice or not choice.index then return end
		if activeMode == 1 then
			Spring.SetConfigInt("NativeFullscreenRenderWidth", choice.width)
			Spring.SetConfigInt("NativeFullscreenRenderHeight", choice.height)
			Spring.SetConfigInt(savedKey("FullscreenRenderWidth", selectedDisplay), choice.width)
			Spring.SetConfigInt(savedKey("FullscreenRenderHeight", selectedDisplay), choice.height)
		elseif activeMode == 2 then
			Spring.SetConfigInt(savedKey("WindowedWidth", selectedDisplay), choice.width)
			Spring.SetConfigInt(savedKey("WindowedHeight", selectedDisplay), choice.height)
		end
		-- Preserve the original display-relative SelectedScreenMode contract.
		local relative = 0
		for index, mode in ipairs(modes) do
			if mode.display == selectedDisplay then relative = relative + 1 end
			if index == choice.index then break end
		end
		Spring.SetConfigInt("SelectedScreenMode", relative)
		-- Render pixels are independent of the existing logical fullscreen window.
		-- Reapplying the archive's physical display mode here would double Retina
		-- window/view/input dimensions. Only a real mode/display transition owns it.
		local samePresentation = (activeMode == 1 or activeMode == 3) and selectedMode() == activeMode
			and Spring.GetConfigInt("Fullscreen", 0) == 1
		if not samePresentation or selectedDisplay ~= appliedDisplay then
			WG.screenMode.SetScreenMode(choice.index, force)
		end
		appliedDisplay = selectedDisplay
	end
	resolution.onload = function()
		-- Observe externally changed window/fullscreen modes without changing geometry.
		activeMode = selectedMode()
		modeOption.value = activeMode
		refresh()
	end
	resolution.onchange = function(_, value)
		resolution.value = value
		applyChoice(false)
	end
	modeOption.onchange = function(_, value)
		activeMode = value
		refresh()
		applyChoice(true)
	end
	if displayOption then
		local original = displayOption.onchange
		displayOption.onchange = function(option, value)
			original(option, value)
			selectedDisplay = value
			refresh()
		end
	end
	refresh()
	table.insert(options, resolutionAt, modeOption)
	table.insert(options, resolutionAt + 2, {
		id = "native_resolution_overlay", group = "gfx", category = category,
		name = "Resolution diagnostics", type = "bool", configInt = "NativeResolutionOverlay",
		value = Spring.GetConfigInt("NativeResolutionOverlay", 0) > 0,
		description = "Show actual render pixels, logical window and safe viewport dimensions.",
	})
end

local bootstrapAttempted = false
function M.bootstrap(getOption, presetKeys, presets, apply)
	if not enabled() or bootstrapAttempted or Spring.GetConfigString("NativeInitialGraphicsPreset", "") ~= "ultra" then return end
	local preset = getOption("preset")
	local ultra
	for i, key in ipairs(presetKeys) do if key == "ultra" then ultra = i end end
	if not preset or not ultra then return end
	bootstrapAttempted = true
	local ok, failure = pcall(function()
		apply(preset, ultra, true)
		assert(Spring.GetConfigString("graphicsPreset", "") == "ultra", "preset readback")
		for id, value in pairs(presets.ultra) do
			local option = getOption(id)
			assert(not option or option.value == value, "preset option readback: " .. id)
		end
	end)
	if ok then
		Spring.SetConfigString("NativeInitialGraphicsPreset", "")
		Spring.Echo("[BAR native] Initial original Ultra preset applied")
	else
		Spring.Echo("[BAR native] Initial Ultra preset remains pending: " .. tostring(failure))
	end
end

function M.overlayLines(g)
	if type(g) ~= "table" then return end
	for _, key in ipairs({ "renderWidth", "renderHeight", "windowWidth", "windowHeight", "backingWidth", "backingHeight", "viewWidth", "viewHeight" }) do
		local v = g[key]
		if type(v) ~= "number" or v ~= v or v <= 0 or v == math.huge then return end
	end
	for _, key in ipairs({ "viewX", "viewY", "mouseTopOffset" }) do
		local v = g[key]
		if type(v) ~= "number" or v ~= v or math.abs(v) == math.huge then return end
	end
	return {
		string.format("Render %.0f × %.0f px | Backing %.0f × %.0f px", g.renderWidth, g.renderHeight, g.backingWidth, g.backingHeight),
		string.format("Window %.0f × %.0f | Safe view %.0f × %.0f @ %.0f,%.0f | Mouse top %.0f", g.windowWidth, g.windowHeight, g.viewWidth, g.viewHeight, g.viewX, g.viewY, g.mouseTopOffset),
	}, 12, g.viewHeight - 20
end

function M.installOverlay(widget)
	local draw, elapsed, lines, x, y, size = widget.DrawScreen, 1
	local update = widget.Update
	widget.Update = function(self, dt)
		if update then update(self, dt) end
		elapsed = elapsed + (dt or 0)
		if elapsed < 0.25 then return end
		elapsed = 0
		lines = nil
		if not enabled() or Spring.GetConfigInt("NativeResolutionOverlay", 0) <= 0 then return end
		local g = Spring.GetNativeDisplayGeometry()
		lines, x, y = M.overlayLines(g)
		if lines then
			-- Fit both lines within the logical safe viewport, including small windows.
			size = math.min(12, (g.viewWidth - 24) / math.max(gl.GetTextWidth(lines[1]), gl.GetTextWidth(lines[2])))
			if size < 5 or y - size * 2 < 8 then lines = nil end
		end
	end
	widget.DrawScreen = function(self, ...)
		if draw then draw(self, ...) end
		if not lines or Spring.GetConfigInt("NativeResolutionOverlay", 0) <= 0 or Spring.IsGUIHidden() then return end
		for i, line in ipairs(lines) do gl.Text(line, x, y - (i - 1) * (size + 3), size, "o") end
	end
end

return M
