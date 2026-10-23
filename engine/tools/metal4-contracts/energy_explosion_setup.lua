settings={gldebugannotations=1,NanoParticleMode=1};logs={};frame=100;visible=true;los=true;mode=1;health=1;removal=false
Spring={GetConfigInt=function(k,d)if k=='NanoParticleMode'then return mode end return settings[k]or d end,Echo=function(...)local x={}for i,v in ipairs({...})do x[i]=tostring(v)end logs[#logs+1]=table.concat(x,' ')end,
GetSpectatingState=function()return false,false end,GetMyAllyTeamID=function()return 0 end,GetGameFrame=function()return frame end,GetAllUnits=function()return {10}end,GetUnitDefID=function()return 1 end,GetUnitHealth=function()return 100,100,0,0,health end,
GetUnitPosition=function()return 0,0,0 end,GetUnitRadius=function()return 32 end,GetUnitRulesParam=function()return removal and 1 or 0 end,IsPosInLos=function()return los end,IsSphereInView=function()return visible end,GetTeamColor=function(team)if team==1 then return .8,.1,.2 end return .1,.5,.9 end}
setmetatable(Spring,{__index=function(_,name)return function()error('Unexpected Spring fixture call '..name)end end})
Game={envDamageTypes={Reclaimed=-3,KilledByLua=-4}};Platform={check=function()return true end,renderBackend='Metal4',gpuVendor='Apple',osFamily='MacOS'};GG={}
UnitDefs={[1]={name='fixture_fusion',energyMake=1000,energyStorage=500,metalCost=1000,radius=32,deathExplosion='fusionboom',customParams={}},[2]={name='fixture_solar',energyUpkeep=-20,energyMake=0,metalCost=150,radius=24,customParams={}},[3]={name='fixture_raptor',energyMake=1000,metalCost=1,category='RAPTOR',customParams={}},[4]={name='fixture_metal_maker',energyMake=0,metalCost=100,radius=10,customParams={energyconv_capacity='1000'}}}
WeaponDefNames={fusionboom={damageAreaOfEffect=100,damages={[0]=200}}};WeaponDefs={}
GL={ARRAY_BUFFER=34962,ELEMENT_ARRAY_BUFFER=34963,FLOAT=5126,TRIANGLES=4,POINTS=0,ONE=1,ONE_MINUS_SRC_ALPHA=771,SRC_ALPHA=770,LEQUAL=515,FRONT_AND_BACK=1032,FILL=6914}
gadgetHandler={IsSyncedCode=function()return false end,RemoveGadget=function()error('Original energy gadget unexpectedly removed')end}
tracy={ZoneBeginN=function()end,ZoneEnd=function()end};FixtureSynced=false;LOG={ERROR=1,INFO=2};Spring.Log=function(_,_,s)logs[#logs+1]=s end
VFS={ZIP_ONLY='Mmeb',CalculateHash=FixtureHash,LoadFile=function()error('Private source must use actual archive transport')end}
math.randomseed(123123)
