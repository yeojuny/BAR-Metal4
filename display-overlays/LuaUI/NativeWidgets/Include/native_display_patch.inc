-- SPDX-License-Identifier: GPL-2.0-or-later
-- Exact anchors in the pinned BAR Options source. A changed source fails closed.
local function replace(source, old, new)
	local first, last = source:find(old, 1, true)
	assert(first and not source:find(old, last + 1, true), "[BAR native] Options display adapter anchor changed")
	return source:sub(1, first - 1) .. new .. source:sub(last + 1)
end
return function(source)
	source = replace(source, [=[		if gpuMem < 3300 then
			if Spring.GetConfigInt("MSAALevel", 2) > 2 then
				Spring.SetConfigInt("MSAALevel", 2)
			end
		end]=], [=[		-- Native MSAA remains an independent explicit setting, including first launch.]=])
	source = replace(source, [=[	local option = row.option
	local inset = math.floor(metrics.rowHeight * 0.12)
	local r = { x1, bottom + inset, right, top - inset }]=], [=[	local option = row.option
	if option.segmented then
		local inset = math.floor(metrics.rowHeight * 0.12)
		local width = (right - x1) / #option.options
		row.segments = {}
		row.control = { x1, bottom + inset, right, top - inset }
		for i, label in ipairs(option.options) do
			local r = { x1 + (i - 1) * width, bottom + inset, x1 + i * width - 2, top - inset }
			row.segments[i] = r
			UiSelector(r[1], r[2], r[3], r[4])
			if i == option.value then UiSelectHighlight(r[1], r[2], r[3], r[4], metrics.csSmall, 0.3, look.white) end
			local shown = tostring(label)
			local available = math.max(1, r[3] - r[1] - math.max(4, metrics.inset * 2))
			local textWidth = font:GetTextWidth(shown)
			local fontSize = textWidth > 0 and math.min(metrics.controlFs, available / textWidth) or metrics.controlFs
			font:Print(look.value .. shown, (r[1] + r[3]) * 0.5, cy, fontSize, "cov")
		end
		return x1
	end
	local inset = math.floor(metrics.rowHeight * 0.12)
	local r = { x1, bottom + inset, right, top - inset }]=])
	source = replace(source, [=[		elseif o and option.type == "select" and inRect(o, mx, my) then
			if playSounds then]=], [=[		elseif o and option.type == "select" and inRect(o, mx, my) then
			if option.segmented then
				for value, rect in ipairs(row.segments or {}) do
					if inRect(rect, mx, my) then
						applyOptionValue(option, value)
						if playSounds then Spring.PlaySoundFile(sounds.buttonClick, 0.6, "ui") end
						return
					end
				end
				return
			end
			if playSounds then]=])
	source = replace(source, [=[	indexOptions()

	-- add custom added options (done via WG.options.addOption)]=], [=[	nativeDisplayOptions.configure(options, screenModes, displays, selectedDisplay, types.basic)
	indexOptions()

	-- add custom added options (done via WG.options.addOption)]=])
	source = replace(source, [=[	-- Filter last, the checks above need every option. Also rebuilds the window.
	applyFilter()
end]=], [=[	-- Filter last, the checks above need every option. Also rebuilds the window.
	applyFilter()
	nativeDisplayOptions.bootstrap(getOption, presetKeys, presets, applyOptionValue)
end]=])
	return "local nativeDisplayOptions = ...\n" .. source
end
