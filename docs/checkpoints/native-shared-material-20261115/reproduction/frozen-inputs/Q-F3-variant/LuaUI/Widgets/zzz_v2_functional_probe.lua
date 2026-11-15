function widget:GetInfo()
 return {name='V2 Sepia Functional Probe',desc='Private initial-uniform paused-frame image',
         author='Codex',layer=250000,enabled=true}
end
local state,targetDraw,cameraAtPause,pausedAt
local function drawFrame()
 local lo,hi=Spring.GetDrawFrame();return lo+65536*hi
end
local function camera()
 local c=Spring.GetCameraState();local keys,out={},{}
 for key in pairs(c) do keys[#keys+1]=key end;table.sort(keys)
 for _,key in ipairs(keys) do
  local value=c[key]
  if type(value)=='number' then out[#out+1]=key..'='..string.format('%.9g',value)
  elseif type(value)=='string' then out[#out+1]=key..'='..value end
 end
 local w,h,x,y=Spring.GetViewGeometry();out[#out+1]=string.format('view=%d,%d,%d,%d',w,h,x,y)
 return table.concat(out,';')
end
local function fail(reason)
 Spring.Echo('[V2Functional] FAILED_'..reason..' mode=shared state='..tostring(state)..
             ' sim='..tostring(Spring.GetGameFrame()))
 state='failed';Spring.Quit()
end
function widget:Initialize()
 if Spring.GetConfigString('V2PilotMode','')~='shared' then fail('MODE_CONFIG');return end
 state='await_sepia';WG.v2FunctionalDone=false
 Spring.Echo('[V2Functional] event=initialized mode=shared variant_initial=1')
end
function widget:Update()
 if state=='failed' or state=='done' then return end
 local sim=Spring.GetGameFrame()
 if state=='await_sepia' then
  if WG.sepia and WG.sepia.setSepia and WG.sepia.getSepia and WG.sepia.setShadeUI then
   WG.sepia.setSepia(0.5);WG.sepia.setShadeUI(false)
   if math.abs(WG.sepia.getSepia()-0.5)<0.0001 then
    Spring.Echo('[V2Functional] event=sepia_active mode=shared sim='..sim..' level=0.500 shade_ui=0')
    state='await_pause'
   end
  elseif sim>=240 then fail('SEPIA_NOT_ACTIVE') end
  return
 end
 if state=='await_pause' then
  local _,_,paused=Spring.GetGameSpeed()
  if sim>=1500 and paused then
   if sim~=1500 then fail('PAUSE_FRAME');return end
   cameraAtPause=camera();pausedAt=Spring.GetTimer();targetDraw=drawFrame()+8
   Spring.Echo('[V2Functional] event=pause_ack mode=shared sim=1500 camera='..cameraAtPause)
   state='variant_image'
  end
  return
 end
 if pausedAt and Spring.DiffTimers(Spring.GetTimer(),pausedAt)>60 then fail('PAUSED_TIMEOUT') end
end
function widget:DrawScreen()
 if state~='variant_image' or drawFrame()<targetDraw then return end
 if Spring.GetGameFrame()~=1500 or camera()~=cameraAtPause then fail('FRAME_OR_CAMERA_CHANGED');return end
 local w,h=Spring.GetViewGeometry()
 local ok=gl.SaveImage(0,0,w,h,'LuaUI/Config/v2-shared-variant.png')
 Spring.Echo(string.format('[V2Functional] event=image mode=shared kind=shared-variant ok=%s sim=%d draw=%d width=%d height=%d camera=%s',
  tostring(ok),Spring.GetGameFrame(),drawFrame(),w,h,camera()))
 if not ok then fail('IMAGE_WRITE');return end
 state='done';WG.v2FunctionalDone=true
 Spring.Echo('[V2Functional] event=done mode=shared sim=1500 relink_tested=0')
end
