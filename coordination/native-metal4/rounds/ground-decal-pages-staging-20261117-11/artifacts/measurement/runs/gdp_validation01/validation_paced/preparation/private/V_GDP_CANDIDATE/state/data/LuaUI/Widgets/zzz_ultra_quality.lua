function widget:GetInfo()
 return {name='Ultra Effective Probe',desc='Apply current local26 Ultra before combat',author='Codex',layer=99990,enabled=true}
end

local requested,requestedAt,glSamples=false,nil,nil

function widget:DrawScreen()
 if requested and glSamples==nil and type(gl.GetNumber)=='function' then
  local ok,value=pcall(gl.GetNumber,0x80A9) -- GL_SAMPLES, scoped current framebuffer.
  if ok and type(value)=='number' then glSamples=value end
 end
end

function widget:Update()
 local options=WG.options
 if not requested then
  if not options or not options.getOptionValue or not options.applyOptionValue then return end
  local initial=options.getOptionValue('preset')
  if initial==nil then return end
  -- Current local26 normal presetKeys={low,medium,high,ultra,custom}.
  options.applyOptionValue('preset',4)
  Spring.SendCommands('option limitoffscreenfps 0')
  Spring.SendCommands('option limitidlefps 0')
  options.applyOptionValue('vsync',1)
  requested=true;requestedAt=Spring.GetTimer()
  Spring.Echo('[UltraPreset] requested_index=4 initial_index='..tostring(initial))
  return
 end
 if Spring.DiffTimers(Spring.GetTimer(),requestedAt)<1 then return end
 local vw,vh=Spring.GetViewGeometry()
 local ww,wh=Spring.GetWindowGeometry()
 local selected=options and options.getOptionValue and options.getOptionValue('preset')
 local offscreen=options and options.getOptionValue and options.getOptionValue('limitoffscreenfps')
 local idle=options and options.getOptionValue and options.getOptionValue('limitidlefps')
 Spring.Echo(string.format('[UltraPreset] observed=1 graphicsPreset=%s selected_index=%s view=%d,%d window=%d,%d MSAA=%d MSAALevel=%d MinSampleShadingRate=%.6f VSync=%d VSyncGame=%d option_limitoffscreenfps=%s option_limitidlefps=%s gl_samples_drawscreen=%s',
  tostring(Spring.GetConfigString('graphicsPreset','unknown')),tostring(selected),vw,vh,ww,wh,
  Spring.GetConfigInt('MSAA',-1),Spring.GetConfigInt('MSAALevel',-1),
  Spring.GetConfigFloat('MinSampleShadingRate',-1),Spring.GetConfigInt('VSync',-1),
  Spring.GetConfigInt('VSyncGame',-1),tostring(offscreen),tostring(idle),tostring(glSamples)))
 widgetHandler:RemoveWidget(widget)
end
