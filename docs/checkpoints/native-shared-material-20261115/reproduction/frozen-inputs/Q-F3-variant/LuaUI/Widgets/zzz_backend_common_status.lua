-- SPDX-License-Identifier: GPL-2.0-or-later
-- Identical read-only compatibility receipt, outside the measured300..1500 window.
function widget:GetInfo()
 return {name='Backend common status receipt',desc='Read renderer-independent options and widget availability',author='Codex',layer=99999,enabled=true}
end
local function enabled(name)
 if type(widgetHandler.FindWidget)~='function' then return 'unavailable' end
 local ok,value=pcall(widgetHandler.FindWidget,widgetHandler,name)
 return ok and (value and 'true' or 'false') or 'unavailable'
end
function widget:GameFrame(frame)
 if frame~=480 and frame~=1500 then return end
 if frame==480 then
  local option=WG.options and WG.options.getOptionValue and WG.options.getOptionValue('limitoffscreenfps')
  Spring.Echo('[FPSPowerStatus] frame='..frame..' shortName='..Game.gameShortName..' LimitIdleFps='..Spring.GetConfigInt('LimitIdleFps',0)..' VSync='..Spring.GetConfigInt('VSync',-999)..' VSyncGame='..Spring.GetConfigInt('VSyncGame',-999)..' idleInterface='..(WG.limitidlefps==nil and 'absent' or 'present')..' option_limitoffscreenfps='..tostring(option))
 end
 local w,h,x,y=Spring.GetViewGeometry()
 local values={'frame='..frame,'view='..w..','..h..','..x..','..y}
 for _,key in ipairs({'MSAA','MSAALevel','VSync','VSyncGame','Water'}) do values[#values+1]=key..'='..tostring(Spring.GetConfigInt(key,-999)) end
 for _,key in ipairs({'ssao','ssao_quality','bloomdeferred','bloomdeferred_quality','cas_sharpness','water','msaa','vsync'}) do
  local value=WG.options and WG.options.getOptionValue and WG.options.getOptionValue(key)
  values[#values+1]='option_'..key..'='..tostring(value)
 end
 values[#values+1]='widget_cas='..enabled('Contrast Adaptive Sharpen')
 values[#values+1]='widget_bloom='..enabled('Bloom Shader Deferred')
 values[#values+1]='widget_ssao='..enabled('SSAO')
 Spring.Echo('[BackendCommonStatus] '..table.concat(values,' '))
 if frame==1500 then widgetHandler:RemoveWidget(self) end
end
