-- Independent exact Environmental producer/helper over numeric Lua tables.
local savedFrame=frame
local oracle=FixtureEnvironmentOracle
local env,private,calls=oracle.env,oracle.private,oracle.calls
for n=#calls,1,-1 do calls[n]=nil end
env.gadget={};env.gadgetHandler.AddSyncAction=function(_,name,callback)FixtureCheck(name=='envLightningSpawn','Original Environmental sync registration');oracle.action=callback end
env.gadgetHandler.RemoveSyncAction=function()end
oracle.original('LuaRules/Gadgets/gfx_environmental_lightning_gl4.lua')
frame=108;math.randomseed(321321);env.gadget:Initialize();oracle.action('envLightningSpawn','scavradiation',0,0,0,1,1,0)
for n=109,114 do frame=n;env.gadget:DrawWorld()end
FixtureCheck(#calls>=2 and calls[1].program=='EnvLightningGlowGL4' and calls[2].program=='EnvLightningBoltGL4','Independent original Environmental order')
local call=calls[1];FixtureCheck(call.count==FixtureEnvCount(),'Independent original Environmental branch count')
for row=1,call.count do for scalar=1,24 do FixtureCheck(call.data[(row-1)*24+scalar]==FixtureEnvSnapshot(row,scalar),'Independent original Environmental bytes at record96 and attribute16')end end
env.gadget:Shutdown();frame=savedFrame
