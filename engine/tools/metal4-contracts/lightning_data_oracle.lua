-- CPU oracle: exact original producer/helper over independent numeric tables,
-- compared at declared record96/attribute16 addresses, never native Download.
local savedFrame,savedProjectiles=frame,projectiles
local calls,activeProgram={},nil
local private={}
function private.GetVBO(target)
 local b={rows={},stride=1}
 function b:Define(n,layout)self.capacity=n;self.stride=0;if layout then for _,a in ipairs(layout)do self.stride=self.stride+a.size end else self.stride=1 end end
 function b:Upload(input,attribute,offset,first,last)
  assert(attribute==nil,'oracle requires original full-record uploads')
  first=first or 1;last=last or #input;local base=(offset or 0)*self.stride
  for n=first,last do self.rows[base+n-first+1]=input[n]end
 end
 function b:GetID()return 'oracle' end
 function b:GetBufferSize()return self.capacity end
 function b:Delete()end
 return b
end
function private.GetVAO()
 local v={}
 function v:AttachVertexBuffer(b)self.vertices=b end
 function v:AttachInstanceBuffer(b)self.instances=b end
 function v:AttachIndexBuffer(b)self.indices=b end
 function v:Delete()end
 function v:DrawArrays(primitive,count,first,instances)local data={}for n=1,#self.instances.rows do data[n]=self.instances.rows[n]end;calls[#calls+1]={program=activeProgram,count=instances,data=data}end
 function v:DrawElements()error('Original Lightning helper must retain DrawArrays')end
 return v
end
for _,name in ipairs({'ObjectLabel','DepthTest','DepthMask','Culling','Blending','Texture'})do private[name]=function()end end
private.LuaShader={CheckShaderUpdates=function(cache)return{Activate=function()activeProgram=cache.shaderName end,Deactivate=function()activeProgram=nil end,Finalize=function()end}end}
local env=setmetatable({gl=private,gadget={},gadgetHandler={IsSyncedCode=function()return false end,RemoveGadget=function()error('Original oracle unexpectedly removed')end}},{__index=_G})
local function original(path)
 local source=gl.NativeLightning.LoadArchiveSource(path)
 local chunk=assert(loadstring(gl.NativeLightning.PrepareSource(source),'@original independent Lightning data'))
 setfenv(chunk,env);return chunk()
end
private.InstanceVBOTable=original('modules/graphics/instancevbotable.lua')
original('LuaRules/Gadgets/gfx_lightning_cannon_gl4.lua')
math.randomseed(123123);env.gadget:Initialize();frame=100;projectiles={};env.gadget:DrawWorld()
projectiles={{id=11,def=1,x=-20,y=5,z=0,vx=40,vy=8,vz=2,team=0}}
for n=101,108 do frame=n;env.gadget:GameFrame(n);env.gadget:DrawWorld()end
FixtureCheck(#calls>=3,'Independent original table transport emits actual Cannon passes')
FixtureCheck(calls[1].program=='LightningCannonGlowGL4' and calls[2].program=='LightningCannonBoltGL4' and calls[3].program=='LightningCannonImpactGL4','Independent original pass order')
local call=calls[1]
FixtureCheck(call.count==FixtureNativeCount(),'Independent original instance count')
for row=1,call.count do for scalar=1,24 do
 FixtureCheck(FixtureNativeSnapshot(row,scalar)==call.data[(row-1)*24+scalar],'Independent original scalar at record96/attribute16 byte address')
end end
FixtureEnvironmentOracle={private=private,env=env,original=original,calls=calls};frame=savedFrame;projectiles=savedProjectiles
