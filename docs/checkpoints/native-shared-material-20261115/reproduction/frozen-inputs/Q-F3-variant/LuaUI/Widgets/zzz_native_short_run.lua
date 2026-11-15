function widget:GetInfo()return{name='Explicit Matched Product Gameplay',desc='Start only, fixed excluded-warmup view, observe, normal exit',author='Codex',layer=100000,enabled=true}end
local opened,warm,started,last,firstDraw,lastDraw,lastSim,nextSample= nil,nil,nil,nil,nil,nil,nil,1
local rows,requests,cameraRestored={},0,false
local externalProfiles=false
local expectedCamera='dist=5000.000000000;dx=0.000000026;dy=-0.835686207;dz=-0.549207270;fov=45.000000000;mode=2.000000000;name=spring;px=5149.000000000;py=204.180068970;pz=596.000000000;rx=2.560177326;ry=0.471238941;rz=0.000000000;view=1512,950,0,0'
local fixedX,fixedY,fixedZ,fixedDX,fixedDY,fixedDZ
local function cameraStillFixed()
 local x,y,z=Spring.GetCameraPosition()
 local dx,dy,dz=Spring.GetCameraDirection()
 return math.abs(x-fixedX)<0.01 and math.abs(y-fixedY)<0.01 and math.abs(z-fixedZ)<0.01
    and math.abs(dx-fixedDX)<0.000001 and math.abs(dy-fixedDY)<0.000001 and math.abs(dz-fixedDZ)<0.000001
end
local function budgetMark(which)
 local f=assert(io.open('LuaUI/Config/budget192.'..which,'w'));f:write(which);f:close()
end
local function profileSnapshot()
 local result={}
 for _,name in ipairs(Spring.GetProfilerRecordNames())do
  local total=Spring.GetProfilerTimeRecord(name,false)
  result[name]=total
 end
 return result
end
local profileBefore
local frameTimes,frameDraws,frameCount={},{},0
local frameTimer,frameDraw,frameStopped,frameOverflow=nil,nil,false,false
function widget:DrawScreen()
 if not started or frameStopped then return end
 local lo,hi=Spring.GetDrawFrame();local d=lo+65536*hi
 if d==frameDraw then return end
 if not cameraStillFixed() then
  frameStopped=true
  Spring.Echo('[MatchedGameplay] FAILED_CAMERA_DRIFT draw='..d..' simulation='..Spring.GetGameFrame())
  Spring.Quit()
  return
 end
 local now=Spring.GetTimer()
 if frameTimer then
  if frameCount>=30000 then frameOverflow=true else frameCount=frameCount+1;frameTimes[frameCount]=Spring.DiffTimers(now,frameTimer);frameDraws[frameCount]=d end
 end
 frameTimer=now;frameDraw=d
end

local function draw()local lo,hi=Spring.GetDrawFrame();return lo+65536*hi end
local function camera()
 local s=Spring.GetCameraState();local keys,out={},{}
 for k in pairs(s)do keys[#keys+1]=k end;table.sort(keys)
 for _,k in ipairs(keys)do local v=s[k];if type(v)=='number'then out[#out+1]=k..'='..string.format('%.9g',v)elseif type(v)=='string'then out[#out+1]=k..'='..v end end
 local w,h,x,y=Spring.GetViewGeometry();out[#out+1]=string.format('view=%d,%d,%d,%d',w,h,x,y)
 return table.concat(out,';')
end

--208 read-only route witness. Both snapshots and all formatting are outside
--the official40s window. Never change feature visibility, IDs or materials.
local route208={}
local function routeSnapshot208(boundary)
 local id=0;local def=Spring.GetFeatureDefID(id)
 local x,y,z=Spring.GetFeaturePosition(id)
 local flag=Spring.GetFeatureDrawFlag(id)
 local mask=Spring.GetFeatureEngineDrawMask(id)
 local name=def and FeatureDefs[def] and FeatureDefs[def].name or 'absent'
 local hex=tostring(name):gsub('.',function(c)return string.format('%02x',string.byte(c))end)
 route208[#route208+1]=string.format('[Route208] boundary=%s draw=%d simulation=%d feature_id=0 definition_known=%d definition=%d name_hex=%s x=%.9g y=%.9g z=%.9g draw_flag=%d engine_mask=%d writes=0',boundary,draw(),Spring.GetGameFrame(),def and 1 or 0,def or -1,hex,x or 0,y or 0,z or 0,flag or -1,mask or -1)
end

local fixedTrack='music/original/peace/ryan krause - sanctum.ogg'
local anchors={}
local sites={{5184,736},{5224,616},{5048,616},{5040,720},{5120,528}}
local views={}
local function recordSites(f)
 local w,h=Spring.GetViewGeometry()
 for i,p in ipairs(sites) do
  local y=Spring.GetGroundHeight(p[1],p[2])
  local x,z=Spring.WorldToScreenCoords(p[1],y,p[2])
  local visible=Spring.IsSphereInView(p[1],y,p[2],32) and x>=0 and x<w and z>=0 and z<h
  views[#views+1]=string.format('[Commander213SiteView] frame=%d site=%d visible=%d x=%.3f y=%.3f',f,i,visible and 1 or 0,x,z)
 end
end
-- Window ownership stays at GameFrame. No camera corrections once the window opens.
local function beginMeasurement(game)
 local now=Spring.GetTimer()
 assert(warm,'[MatchedGameplay] FAILED_NO_POSITIVE_SIMULATION')
 Spring.Echo(string.format('[V2FunctionalWarmup] observed_seconds=%.9f gate=observational',Spring.DiffTimers(now,warm)))
 assert(not started,'[MatchedGameplay] FAILED_DUPLICATE_START')
  assert(cameraRestored and camera()==expectedCamera,'[MatchedGameplay] FAILED_CAMERA_AT_OPEN')
  fixedX,fixedY,fixedZ=Spring.GetCameraPosition()
  fixedDX,fixedDY,fixedDZ=Spring.GetCameraDirection()
  routeSnapshot208("before")
  profileBefore=profileSnapshot();budgetMark("start")
  started=Spring.GetTimer();now=started;last=now;firstDraw=draw();lastDraw=firstDraw;lastSim=game
  Spring.Echo('[NativeShortRun] positive gameplay began at frame '..game)
  Spring.Echo(string.format('[MatchedAnchor] boundary=start seconds=0 draw=%d simulation=%d callback=GameFrame warmup_seconds=%.9f',firstDraw,game,Spring.DiffTimers(now,warm)))
end
local function sampleMeasurement(now,game)
 local seconds=Spring.DiffTimers(now,started)
   local d=draw();local elapsed=Spring.DiffTimers(now,last)
   local a,b,paused=Spring.GetGameSpeed();local units=Spring.GetAllUnits()
   rows[#rows+1]={seconds,elapsed,lastDraw,d,lastSim,game,Spring.GetFPS(),a,b,paused and 1 or 0,#units,camera()}
   last=now;lastDraw=d;lastSim=game;nextSample=math.floor(seconds)+1
 return seconds
end
local function endMeasurement(game)
 assert(started and not frameStopped,'[MatchedGameplay] FAILED_END_WITHOUT_START')
 local now=Spring.GetTimer()
 local seconds=sampleMeasurement(now,game)
   frameStopped=true
   budgetMark("end")
   routeSnapshot208("after")
   local profileAfter=profileSnapshot()
   for name,total in pairs(profileAfter)do
    if profileBefore[name]then
     local hex=name:gsub(".",function(c)return string.format("%02x",string.byte(c))end)
     Spring.Echo(string.format("[Budget192CPU] name_hex=%s total_ms=%.9g",hex,total-profileBefore[name]))
    end
   end
   Spring.Echo(string.format('[MatchedAnchor] boundary=end seconds=%.9f draw=%d simulation=%d callback=GameFrame',seconds,draw(),game))
   for _,r in ipairs(rows)do Spring.Echo(string.format('[MatchedFPS] seconds=%.9f interval_seconds=%.9f start_draw=%d end_draw=%d start_simulation=%d end_simulation=%d draws=%d interval_fps=%.9f get_fps=%d speed_a=%.9f speed_b=%.9f paused=%d units=%d camera=%s',r[1],r[2],r[3],r[4],r[5],r[6],r[4]-r[3],(r[4]-r[3])/r[2],r[7],r[8],r[9],r[10],r[11],r[12]))end
   Spring.Echo(string.format('[NativeShortRun] total draws=%d seconds=%.9f',draw()-firstDraw,seconds))
   for _,entry in ipairs(route208)do Spring.Echo(entry)end
   local frameFile=assert(io.open('LuaUI/Config/TERRAIN211-FRAMES.csv','w'))
   frameFile:write('draw,seconds\n')
   for i=1,frameCount do frameFile:write(string.format('%d,%.9f\n',frameDraws[i],frameTimes[i]))end
   frameFile:close()
   Spring.Echo(string.format('[Terrain211FrameObserver] samples=%d overflow=%d phase=after_measurement source=DrawScreen_timer',frameCount,frameOverflow and 1 or 0))
   return
end
function widget:GameFrame(f)
 if f==1 then warm=Spring.GetTimer();budgetMark("simulation");Spring.Echo("[NativeMatchedWarmup] excluded_seconds=10");Spring.Echo("[Direct50Lifecycle] simulation_start=1") end
 if f==120 then
  assert(WG.music and WG.music.playTrack and VFS.FileExists(fixedTrack),'benchmark music control unavailable')
  WG.music.playTrack(fixedTrack)
 end
 if f>=480 and f<=511 then
  anchors[#anchors+1]={f,camera()}
 end
 if f==480 or f==1500 then recordSites(f) end
 if f==300 then beginMeasurement(f) end
 if externalProfiles and started and not frameStopped and f==600 then budgetMark("time-profiler") end
 if externalProfiles and started and not frameStopped and f==1500 then budgetMark("metal-trace") end
 if f==1500 then endMeasurement(f);Spring.SendCommands('pause 1') end
end
function widget:Initialize()for i=1,30000 do frameTimes[i]=0;frameDraws[i]=0 end;opened=Spring.GetTimer();Spring.Echo('[MatchedGameplay] control_initialized warmup=10 measured=40 positive_total=50 idle_limiter_disabled=1 window_mode=simulation start_simulation=300 end_simulation=1500')end
function widget:Update()
 local now=Spring.GetTimer();local game=Spring.GetGameFrame()
 if game<=0 and not warm and Spring.DiffTimers(now,opened)>2 and requests<3 then
  if not last or Spring.DiffTimers(now,last)>=5 then last=now;requests=requests+1;Spring.SendCommands('forcestart')end
 end

 if warm and not cameraRestored and game>=30 then
  Spring.SetConfigInt('WindowedEdgeMove',0);Spring.SetConfigInt('FullscreenEdgeMove',0)
  assert(Spring.SetCameraState({dist=5000.0,dx=2.6e-08,dy=-0.835686207,dz=-0.54920727,fov=45.0,mode=2.0,name='spring',px=5149.0,py=204.18006897,pz=596.0,rx=2.560177326,ry=0.471238941,rz=0.0},0));cameraRestored=true
 end
 if started and not frameStopped then
  local seconds=Spring.DiffTimers(now,started)
  if seconds>=nextSample then sampleMeasurement(now,game) end
 end
 if frameStopped and game>=1500 then
  if not WG.v2FunctionalDone then
   if Spring.DiffTimers(now,opened)>=180 then Spring.Echo('[V2Functional] FAILED_CONTROLLER_TIMEOUT');Spring.Quit() end
   return
  end
  local done=Spring.GetGameRulesParam('macro213_done')
  local err=Spring.GetGameRulesParam('macro213_error')
  if done~=1500 or err then Spring.Echo('[Commander213Fail] '..tostring(err or 'missing synced completion'))
  else
   local n=assert(Spring.GetGameRulesParam('macro213_rows'))
   for i=1,n do Spring.Echo('[Commander213Event] '..assert(Spring.GetGameRulesParam('macro213_row_'..i))) end
   for _,v in ipairs(anchors) do Spring.Echo('[Commander213Anchor] '..v[1]..' '..v[2]) end
   for _,v in ipairs(views) do Spring.Echo(v) end
   Spring.Echo('[Commander213Complete] sim=1500')
  end
  Spring.Quit();widgetHandler:RemoveWidget(self);return
 end
 if Spring.DiffTimers(now,opened)>=240 then Spring.Echo('[MatchedGameplay] FAILED_HARD_DEADLINE');Spring.Quit();widgetHandler:RemoveWidget(self)end
end
