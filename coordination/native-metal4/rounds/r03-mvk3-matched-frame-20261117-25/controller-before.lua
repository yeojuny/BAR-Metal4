function widget:GetInfo()return{name='Explicit Matched Product Gameplay',desc='Start only, fixed excluded-warmup view, observe, normal exit',author='Codex',layer=100000,enabled=true}end
local opened,warm,started,last,firstDraw,lastDraw,lastSim,nextSample= nil,nil,nil,nil,nil,nil,nil,1
local rows,requests,cameraRestored={},0,false
local externalProfiles=false
local combatWarm
local expectedCamera=nil
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
 assert(warm and Spring.DiffTimers(now,warm)>=9.8,'[MatchedGameplay] FAILED_WARMUP_BEFORE_SIM300')
 assert(not started,'[MatchedGameplay] FAILED_DUPLICATE_START')
  assert(cameraRestored,'[MatchedGameplay] FAILED_CAMERA_AT_OPEN')
  assert(combatWarm and Spring.DiffTimers(now,combatWarm)>=9.8,'[BattleBenchmark] FAILED_COMBAT_WARMUP')
  Spring.Echo(string.format('[BattleBenchmark] combat_warmup_wall_seconds=%.9f',Spring.DiffTimers(now,combatWarm)))
  expectedCamera=camera();Spring.Echo('[BattleCameraOpen] '..expectedCamera)
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

local function battleInitialState()
 local records={}
 for _,id in ipairs(Spring.GetAllUnits())do
  local team=Spring.GetUnitTeam(id);local def=Spring.GetUnitDefID(id)
  local x,y,z=Spring.GetUnitPosition(id)
  local name=def and UnitDefs[def] and UnitDefs[def].name or 'unknown'
  records[#records+1]=string.format('%d:%s:%.3f:%.3f:%.3f',team or -1,name,x or 0,y or 0,z or 0)
 end
 table.sort(records)
 return string.format('frame=240 map=%s mapx=%d mapz=%d units=%d details=%s',
  Game.mapName,Game.mapSizeX,Game.mapSizeZ,#records,table.concat(records,'|'))
end
local battleStopped=false
function widget:GameFrame(f)
 if f==30 then Spring.SendCommands('cheat 1');Spring.SendCommands('specfullview 1') end
 if f==240 then
  local vw,vh=Spring.GetViewGeometry()
  assert(vw==1512 and vh==950,'[UltraQuality] FAILED_VIEW_AT_COMBAT')
  local spec,fullView,fullSelect=Spring.GetSpectatingState()
  assert(spec and fullView,'[BattleVisibility] FAILED_SPECTATOR_FULLVIEW')
  local visible=(Spring.GetVisibleUnits and Spring.GetVisibleUnits()) or {}
  Spring.Echo(string.format('[BattleVisibility] frame=240 spectating=%s fullview=%s fullselect=%s visible_units=%d',tostring(spec),tostring(fullView),tostring(fullSelect),#visible))
  combatWarm=Spring.GetTimer()
  assert(Game.mapName=='Seths Ravine Remake 1.3.1','[BattleBenchmark] wrong_map')
  assert(UnitDefNames.armbull,'[BattleBenchmark] armbull_absent')
  Spring.Echo('[BattleInitialState] '..battleInitialState())
  Spring.Echo('[BattleBenchmark] command_frame=240 seed=7654321 command=luarules_benchmark_armbull_armbull_80_8_1400')
  Spring.SendCommands('luarules benchmark armbull armbull 80 8 1400')
 end
 if f==1 then warm=Spring.GetTimer();budgetMark("simulation");Spring.Echo("[NativeMatchedWarmup] excluded_seconds=18");Spring.Echo("[Direct50Lifecycle] simulation_start=1") end
 if f>=540 and f<=571 then
  anchors[#anchors+1]={f,camera()}
 end
 if f==540 then
  local spec,fullView,fullSelect=Spring.GetSpectatingState()
  local visible=(Spring.GetVisibleUnits and Spring.GetVisibleUnits()) or {}
  Spring.Echo(string.format('[BattleVisibility] frame=540 spectating=%s fullview=%s fullselect=%s visible_units=%d',tostring(spec),tostring(fullView),tostring(fullSelect),#visible))
  beginMeasurement(f)
 end
 if externalProfiles and started and not frameStopped and f==600 then budgetMark("time-profiler") end
 if externalProfiles and started and not frameStopped and f==1740 then budgetMark("metal-trace") end
 if f==1740 then endMeasurement(f) end
end
function widget:Initialize()for i=1,30000 do frameTimes[i]=0;frameDraws[i]=0 end;opened=Spring.GetTimer();Spring.Echo('[MatchedGameplay] control_initialized warmup=10 measured=40 positive_total=58 combat_total=50 idle_limiter_disabled=1 window_mode=simulation start_simulation=540 end_simulation=1740 benchmark_command_frame=240')end
function widget:Update()
 local now=Spring.GetTimer();local game=Spring.GetGameFrame()
 if game<=0 and not warm and Spring.DiffTimers(now,opened)>2 and requests<3 then
  if not last or Spring.DiffTimers(now,last)>=5 then last=now;requests=requests+1;Spring.SendCommands('forcestart')end
 end

 if warm and not cameraRestored and game>=30 then
  Spring.SetConfigInt('WindowedEdgeMove',0);Spring.SetConfigInt('FullscreenEdgeMove',0)
  local x,z=Game.mapSizeX/2,Game.mapSizeZ/2
  local y=math.max(0,Spring.GetGroundHeight(x,z))
  assert(Spring.SetCameraState({px=x,py=y,pz=z,height=2400,zscale=0.6},0));cameraRestored=true
 end
 if started and not frameStopped then
  local seconds=Spring.DiffTimers(now,started)
  if seconds>=nextSample then sampleMeasurement(now,game) end
 end
 if frameStopped and game>=1740 then
  if not battleStopped then
   battleStopped=true
   Spring.SendCommands('luarules benchmark')
   Spring.Echo('[BattleBenchmark] stop_after_count=1 frame='..game)
   return
  end
  Spring.Quit();widgetHandler:RemoveWidget(self);return
 end
 if Spring.DiffTimers(now,opened)>=240 then Spring.Echo('[MatchedGameplay] FAILED_HARD_DEADLINE');Spring.Quit();widgetHandler:RemoveWidget(self)end
end
