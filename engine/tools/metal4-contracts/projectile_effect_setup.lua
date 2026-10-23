settings={gldebugannotations=1};logs={};frame=100;paused=false;visible=true;projectiles={};scanCount=0
Spring={GetConfigInt=function(k,d)return settings[k]or d end,Echo=function(...)logs[#logs+1]=table.concat({...},' ')end,GetSpectatingState=function()return false,false end,GetMyAllyTeamID=function()return 0 end,GetGameFrame=function()return frame end,GetGameSpeed=function()return 1,1,paused end,GetFrameTimeOffset=function()return .25 end,GetTimer=function()return frame end,DiffTimers=function(a,b)return a-b end,IsAABBInView=function()return visible end,GetCameraPosition=function()return 0,0,100 end,GetCameraDirection=function()return 0,0,-1 end,
GetProjectilesInRectangle=function()scanCount=scanCount+1;local t={}for _,p in ipairs(projectiles)do t[#t+1]=p.id end return t end,
GetVisibleProjectiles=function()local t={}for _,p in ipairs(projectiles)do t[#t+1]=p.id end return t end,
GetProjectileDefID=function(id)for _,p in ipairs(projectiles)do if p.id==id then return p.def end end end,
GetProjectilePosition=function(id)for _,p in ipairs(projectiles)do if p.id==id then return p.x,p.y,p.z end end end,
GetProjectileVelocity=function(id)for _,p in ipairs(projectiles)do if p.id==id then return p.vx,p.vy,p.vz end end end,
GetProjectileTeamID=function(id)for _,p in ipairs(projectiles)do if p.id==id then return p.team end end end,
GetProjectileOwnerID=function(id)for _,p in ipairs(projectiles)do if p.id==id then return p.owner or 0 end end end,
GetTeamAllyTeamID=function(id)return id end,IsPosInAirLos=function(x,y,z)return losMode~='none' and(losMode~='half' or x>=0)end,IsPosInLos=function()return true end,
GetUnitTransformMatrix=function()return 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 end}
setmetatable(Spring,{__index=function(_,name)return function()error('Unexpected Spring fixture call '..name)end end})
Game={mapSizeX=8192,mapSizeZ=8192};Platform={check=function()return true end,renderBackend='Metal4',isHeadless=false};GG={};UnitDefs={}
WeaponDefs={[1]={type='BeamLaser',thickness=2,corethickness=.3,laserflaresize=7,range=300,beamttl=3,beamtime=.1,visuals={colorR=.1,colorG=.35,colorB=.55}},[2]={type='Cannon',size=3,range=600,damageAreaOfEffect=66,visuals={colorR=.25,colorG=.5,colorB=.1}},[3]={type='BeamLaser',paralyzer=true,thickness=4,range=500,visuals={colorR=.3,colorG=.2,colorB=.1}}}
GL={ARRAY_BUFFER=34962,ELEMENT_ARRAY_BUFFER=34963,FLOAT=5126,TRIANGLES=4,ONE=1,ONE_MINUS_SRC_ALPHA=771,SRC_ALPHA=770}
gadgetHandler={IsSyncedCode=function()return false end,RemoveGadget=function()error('Original gadget unexpectedly removed')end}
tracy={ZoneBeginN=function()end,ZoneEnd=function()end}
FixtureSynced=false
LOG={ERROR=1,INFO=2}
Spring.Log=function(_,_,message)logs[#logs+1]=message end
VFS={ZIP_ONLY='Mmeb',CalculateHash=FixtureHash,LoadFile=function()error('Private native route must use exact ZIP loader')end}
math.randomseed(123123)
