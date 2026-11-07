-- SPDX-License-Identifier: GPL-2.0-or-later
-- Execute original game31657 Healthbars CPU/event/config/lifetime code with
-- a private, exact-source full GS equivalent. Global GL4 stays unchanged.
local path="LuaUI/Widgets/gui_healthbars_gl4.lua"
local original=getfenv and getfenv(1) or _ENV
local native=Platform and Platform.renderBackend=="Metal4"
local family=native and gl.NativeHealthBars
if native and (not family or not family.Available()) then error("Original Healthbars consumer is unavailable") end
local identities={
 ["LuaUI/Widgets/gui_healthbars_gl4.lua"]="940f386d2b20545f36e0b86a515b6d0a696225d60f85b7e744f84892767697aeb6a282552a365b572d1bc78e42362c785f5db80b865d33d9569495c377bf71d7",
 ["modules/graphics/instancevbotable.lua"]="bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256",
 ["modules/graphics/luashader.lua"]="81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436",
 ["LuaUI/Shaders/HealthbarsGL4.vert.glsl"]="77dde305e3841ada03bc8a3cb9080a49b011abd5cb9625a2a86d655581bcfce1cc9bf541efd882767257e11c6dfb4f3e1c723783e16edf00a1c2f376cc7a68f2",
 ["LuaUI/Shaders/HealthbarsGL4.geom.glsl"]="82526f994f49ea781bf860a63a4edb2f9c5a9e06b40914cc5524ea75ad1f803c82e121e443208917c3736172e2abdc443e688294b61453f7c8a0222679d667ae",
 ["LuaUI/Shaders/HealthbarsGL4.frag.glsl"]="575714a85a96a618e6823c248b3896751a352df11b82eed7a2ce0306d90c5d351c4231d6392727db4468e3e5465993b4c3338cabedbd1c1e0e8aa1d3466b469a",
}
local function archive(name,raw)
 local source=native and family.LoadArchiveSource(name) or VFS.LoadFile(name,VFS.ZIP_ONLY)
 if type(source)~="string" then error("Original Healthbars source missing: "..name) end
 if native and VFS.CalculateHash(source,1)~=identities[name] then error("Original Healthbars source identity changed: "..name) end
 return native and not raw and family.PrepareSource(source) or source
end
local function execute(source,name,environment)
 local chunk,message=(loadstring or load)(source,"@"..name.." [native Healthbars scope]")
 if not chunk then error(message) end
 if setfenv then setfenv(chunk,environment) end
 return chunk()
end
local source=archive(path)
if not native then return execute(source,path,original) end
local function replaceDefaultOnce(text, before, after)
 local first,last=string.find(text,before,1,true)
 if not first or string.find(text,before,last+1,true) then error("Original Healthbars default anchor changed") end
 return string.sub(text,1,first-1)..after..string.sub(text,last+1)
end
source=replaceDefaultOnce(source,"local barScale = 1 -- Option 'healthbarsscale'","local barScale = 2 -- Option 'healthbarsscale'")
source=replaceDefaultOnce(source,"local barHeight = 0.9","local barHeight = 2")
local privateGL=family.CreateContext()
setmetatable(privateGL,{__index=gl})
local environment=setmetatable({gl=privateGL},{__index=original})
local helper="modules/graphics/instancevbotable.lua"
privateGL.InstanceVBOTable=execute(archive(helper),helper,environment)
local shaderPath="modules/graphics/luashader.lua"
local originalShader=execute(archive(shaderPath),shaderPath,environment)
local shader=setmetatable({isGeometryShaderSupported=true},{__index=originalShader})
shader.CheckShaderUpdates=function(cache,delay)
 if not cache.forceupdate and cache.lastshaderupdate and Spring.DiffTimers(Spring.GetTimer(),cache.lastshaderupdate)<=(delay or 0.5) then return nil end
 cache.lastshaderupdate=Spring.GetTimer()
 local vertex=(cache.vssrcpath and archive(cache.vssrcpath,true)) or cache.vsSrc
 local fragment=(cache.fssrcpath and archive(cache.fssrcpath,true)) or cache.fsSrc
 local geometry=(cache.gssrcpath and archive(cache.gssrcpath,true)) or cache.gsSrc
 if vertex==cache.vsSrc and fragment==cache.fsSrc and geometry==cache.gsSrc and not cache.forceupdate then return nil end
 cache.vsSrc,cache.fsSrc,cache.gsSrc=vertex,fragment,geometry
 cache.forceupdate=nil;cache.updateFlag=true
 local engine=originalShader.GetEngineUniformBufferDefs()
 local defines=originalShader.CreateShaderDefinesString(cache.shaderConfig)
 local function complete(text)
  return text:gsub("//__ENGINEUNIFORMBUFFERDEFS__",engine):gsub("//__DEFINES__",defines)
 end
 cache.vsSrcComplete,cache.gsSrcComplete,cache.fsSrcComplete=complete(vertex),complete(geometry),complete(fragment)
 local program=privateGL.CreateProgram(cache.shaderName,vertex,geometry,fragment,engine,defines,cache.uniformFloat or {},cache.uniformInt or {})
 -- Preserve the original table-based Shutdown:Finalize contract.
 local result={}
 function result:Activate() return program:Activate() end
 function result:Deactivate() return program:Deactivate() end
 function result:SetUniform(name,...) return program:SetUniform(name,...) end
 function result:Delete() return program:Delete() end
 function result:Finalize() return program:Finalize() end
 return result
end
privateGL.LuaShader=shader
local result=execute(source,path,environment)
-- Match the retained reference once-only default migration. Exact legacy
-- serialized values only; later explicit choices remain authoritative.
local originalGetConfigData=widget.GetConfigData
local originalSetConfigData=widget.SetConfigData
widget.GetConfigData=function(self,...)
 local data=originalGetConfigData(self,...)
 data.run91ReferenceHealthBars=1
 return data
end
widget.SetConfigData=function(self,data,...)
 if data.run91ReferenceHealthBars~=1 then
  if data.barScale==nil or data.barScale==1 then data.barScale=2 end
  if data.barHeight==nil or data.barHeight==0.9 or data.barHeight==0.89999998 then data.barHeight=2 end
  data.run91ReferenceHealthBars=1
 end
 return originalSetConfigData(self,data,...)
end
local originalInfo=widget.GetInfo
widget.GetInfo=function(self,...)
 local info=originalInfo(self,...)
 if not family.Available() then error("Original Healthbars consumer became unavailable") end
 if info.depends then
  if #info.depends~=1 or info.depends[1]~="gl4" then error("Original Healthbars dependency contract changed") end
  info.depends=nil
 end
 return info
end
Spring.Echo("[BAR Metal4] Source-verified full original Healthbars connected")
return result
