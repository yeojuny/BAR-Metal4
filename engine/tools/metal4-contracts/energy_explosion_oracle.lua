-- CPU data oracle only: original Lua/helper, independent numeric table transport.
local source=gl.NativeEnergyExplosion.LoadArchiveSource('LuaRules/Gadgets/gfx_energy_explosion_particles_gl4.lua')
local helper=gl.NativeEnergyExplosion.LoadArchiveSource('modules/graphics/instancevbotable.lua')
local originalFrame=frame
frame=100;mode=1;visible=true;los=true;removal=false;health=1
local calls={};local attributes={};local nextId=1
local private={}
function private.GetVBO(target)
 local b={rows={},stride=1,id=nextId};nextId=nextId+1
 function b:Define(n,layout)self.capacity=n;self.layout=layout;self.stride=0;if layout then for _,a in ipairs(layout)do self.stride=self.stride+a.size end else self.stride=1 end end
 function b:Upload(input,attribute,offset,first,last)
  assert(attribute==nil,'oracle intentionally requires original full-record uploads')
  first=first or 1;last=last or #input;local destination=(offset or 0)*self.stride
  for i=first,last do self.rows[destination+i-first+1]=input[i]end
 end
 function b:GetID()return tostring(self.id)end
 function b:GetBufferSize()return self.capacity end
 function b:Delete()end
 return b
end
function private.GetVAO()
 local v={}
 function v:AttachVertexBuffer(b)self.vertex=b end
 function v:AttachIndexBuffer(b)self.index=b end
 function v:AttachInstanceBuffer(b)self.instance=b end
 function v:DrawElements(mode,count,first,instances)calls[#calls+1]={vao=self,count=count,instances=instances}end
 function v:DrawArrays(...)error('Actual original EEP NoGS must be indexed')end
 function v:Delete()end
 return v
end
private.ObjectLabel=function()end
for _,n in ipairs({'DepthTest','DepthMask','Culling','AlphaTest','Color','ColorMask','Scissor','PolygonOffset','PolygonMode','StencilTest','Blending'})do private[n]=function()end end
private.LuaShader={CheckShaderUpdates=function(cache)if cache.gsSrc then return nil end return{Activate=function()end,Deactivate=function()end}end}
local env=setmetatable({gl=private,gadget={},gadgetHandler={IsSyncedCode=function()return false end,RemoveGadget=function()error('Oracle unexpectedly removed')end}},{__index=_G})
local function loadOriginal(raw,name)local c=assert(loadstring(gl.NativeEnergyExplosion.PrepareSource(raw),name));setfenv(c,env);return c()end
private.InstanceVBOTable=loadOriginal(helper,'@original InstanceVBOTable CPU oracle')
loadOriginal(source,'@original energy producer CPU oracle')
math.randomseed(123123);env.gadget:Initialize();env.gadget:UnitDestroyed(10,1,0,nil,nil,nil,1);frame=101;env.gadget:GameFrame(frame);env.gadget:DrawWorld()
FixtureCheck(#calls==1,'Independent original producer emits one indexed draw')
local call=calls[1]
FixtureCheck(call.count==42,'Independent original cube/halo index count')
for record=1,call.instances do for scalar=1,16 do
 FixtureCheck(FixtureNativeSnapshot(record,scalar)==call.vao.instance.rows[(record-1)*16+scalar],'Independent original scalar at explicit record64/attribute address')
end end
frame=originalFrame
