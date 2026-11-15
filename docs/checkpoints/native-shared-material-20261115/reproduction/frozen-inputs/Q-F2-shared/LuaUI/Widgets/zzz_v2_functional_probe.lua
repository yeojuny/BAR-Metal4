function widget:GetInfo()
 return {name='V2 Sepia Functional Probe',desc='Private paused-frame image and relink witness',
         author='Codex',layer=250000,enabled=true}
end

local mode,state,targetDraw,cameraAtPause,pausedAt
local sepiaLevel=0.5
local function drawFrame()
 local lo,hi=Spring.GetDrawFrame()
 return lo+65536*hi
end
local function camera()
 local fields,keys={},{}
 for key in pairs(Spring.GetCameraState()) do keys[#keys+1]=key end
 table.sort(keys)
 local c=Spring.GetCameraState()
 for _,key in ipairs(keys) do
  local value=c[key]
  if type(value)=='number' then fields[#fields+1]=key..'='..string.format('%.9g',value)
  elseif type(value)=='string' then fields[#fields+1]=key..'='..value end
 end
 local w,h,x,y=Spring.GetViewGeometry()
 fields[#fields+1]=string.format('view=%d,%d,%d,%d',w,h,x,y)
 return table.concat(fields,';')
end
local function fail(reason)
 Spring.Echo('[V2Functional] FAILED_'..reason..' mode='..tostring(mode)..' state='..tostring(state)..
             ' sim='..tostring(Spring.GetGameFrame()))
 Spring.Quit()
 state='failed'
end
local function activeSepia()
 if not (WG.sepia and WG.sepia.setSepia and WG.sepia.getSepia and WG.sepia.setShadeUI) then
  return false
 end
 WG.sepia.setSepia(sepiaLevel)
 WG.sepia.setShadeUI(false)
 return math.abs(WG.sepia.getSepia()-sepiaLevel)<0.0001
end
local function capture(kind)
 if Spring.GetGameFrame()~=1500 or camera()~=cameraAtPause then fail('FRAME_OR_CAMERA_CHANGED');return end
 local w,h=Spring.GetViewGeometry()
 local path='LuaUI/Config/v2-'..kind..'.png'
 local ok=gl.SaveImage(0,0,w,h,path)
 Spring.Echo(string.format('[V2Functional] event=image mode=%s kind=%s ok=%s sim=%d draw=%d width=%d height=%d camera=%s',
  mode,kind,tostring(ok),Spring.GetGameFrame(),drawFrame(),w,h,camera()))
 if not ok then fail('IMAGE_WRITE');return end
end
function widget:Initialize()
 mode=Spring.GetConfigString('V2PilotMode','')
 if mode~='old' and mode~='shared' then fail('MODE_CONFIG');return end
 state='await_sepia'
 WG.v2FunctionalDone=false
 Spring.Echo('[V2Functional] event=initialized mode='..mode)
end
function widget:Update()
 if state=='failed' or state=='done' then return end
 local sim=Spring.GetGameFrame()
 if state=='await_sepia' then
  if activeSepia() then
   Spring.Echo(string.format('[V2Functional] event=sepia_active mode=%s sim=%d level=%.3f shade_ui=0',mode,sim,sepiaLevel))
   state='await_pause'
  elseif sim>=240 then fail('SEPIA_NOT_ACTIVE') end
  return
 end
 if state=='await_pause' then
  local _,_,paused=Spring.GetGameSpeed()
  if sim>=1500 and paused then
   if sim~=1500 then fail('PAUSE_FRAME');return end
   cameraAtPause=camera();pausedAt=Spring.GetTimer();targetDraw=drawFrame()+8
   Spring.Echo('[V2Functional] event=pause_ack mode='..mode..' sim=1500 camera='..cameraAtPause)
   state='identity_image'
  end
  return
 end
 if pausedAt and Spring.DiffTimers(Spring.GetTimer(),pausedAt)>60 then fail('PAUSED_TIMEOUT');return end
 if state=='set_variant_config' then
  Spring.SetConfigFloat('V2PilotExposure',1.25,true)
  Spring.SetConfigFloat('V2PilotBiasR',0.10,true)
  Spring.SetConfigFloat('V2PilotBiasG',0.20,true)
  Spring.SetConfigFloat('V2PilotBiasB',0.30,true)
  Spring.Echo(string.format('[V2Functional] event=config_update mode=shared sim=1500 exposure=%.3f bias=%.3f,%.3f,%.3f',
   Spring.GetConfigFloat('V2PilotExposure',-1),Spring.GetConfigFloat('V2PilotBiasR',-1),
   Spring.GetConfigFloat('V2PilotBiasG',-1),Spring.GetConfigFloat('V2PilotBiasB',-1)))
  state='disable_sepia';return
 end
 if state=='disable_sepia' then
  local returned=widgetHandler:DisableWidget('Sepia Tone')
  Spring.Echo('[V2Functional] event=widget_disable_requested mode=shared sim=1500 returned='..tostring(returned))
  state='await_disabled';return
 end
 if state=='await_disabled' then
  if WG.sepia~=nil then return end
  Spring.Echo('[V2Functional] event=widget_disabled mode=shared sim=1500 wg_nil=1')
  state='enable_sepia';return
 end
 if state=='enable_sepia' then
  local returned=widgetHandler:EnableWidget('Sepia Tone')
  Spring.Echo('[V2Functional] event=widget_enable_requested mode=shared sim=1500 returned='..tostring(returned))
  state='await_relink';return
 end
 if state=='await_relink' then
  if not WG.sepia then return end
  if not activeSepia() then fail('WG_RESTORE');return end
  targetDraw=drawFrame()+8
  Spring.Echo('[V2Functional] event=widget_relinked mode=shared sim=1500 wg_restored=1')
  state='variant_image';return
 end
end
function widget:DrawScreen()
 if (state~='identity_image' and state~='variant_image') or drawFrame()<targetDraw then return end
 if state=='identity_image' then
  capture(mode=='old' and 'old-identity' or 'shared-identity')
  if state=='failed' then return end
  if mode=='old' then
   state='done';WG.v2FunctionalDone=true
   Spring.Echo('[V2Functional] event=done mode=old sim=1500')
  else
   state='set_variant_config'
  end
 else
  capture('shared-variant')
  if state=='failed' then return end
  state='done';WG.v2FunctionalDone=true
  Spring.Echo('[V2Functional] event=done mode=shared sim=1500')
 end
end
