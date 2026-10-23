-- SPDX-License-Identifier: GPL-2.0-or-later
-- Execute the exact 31657 selected-unit widget with its paired native program family.
local path="LuaUI/Widgets/gui_selectedunits_gl4.lua"
local original=getfenv and getfenv(1) or _ENV
local native=Platform and Platform.renderBackend=="Metal4"
local family=native and gl.NativeUnitPrimitive
if native and (not family or not family.SelectedAvailable() or not family.CreateSelectedContext31657 or not family.LoadArchiveSource31657 or not family.PrepareSource31657) then error("Original 31657 unit primitive consumer is unavailable") end
local identities={
 ["LuaUI/Widgets/gui_selectedunits_gl4.lua"]="900ec2805920eaed5d45316ae01fdfd4a05eba6a03afa460188a50f1380598de10d10090ad299c789db6a22fa3ce0e052930598de0f192a6ba6005bcff93ee89",
 ["LuaUI/Include/DrawPrimitiveAtUnit.lua"]="3b4913f5e3ff6ca3eaa76ec87a0a67d756ce446fc2e11d9b53be78b1cdd64a2de8ee496133b0e256bac304e0bea08256b988ee655f99b6cc7603f85087f0f9fd",
 ["modules/graphics/instancevbotable.lua"]="bf128f904541628d4f89c1699c94461f604dcbbc9fa48164bc4ac6febdeba29c4c92786e9d1d0c054e3fd8fc55104766e0b0d0e70f2a6fbd3bd3149b0ea39256",
 ["modules/graphics/luashader.lua"]="81858ab150aceb7d84f324708f3b3c4a5c8101df03c2a3a0597cc64413fea8ff7db558b711b18e930aa577bf7b0c6b96bfde83c342ca94508f72021742372436",
 ["LuaUI/Shaders/DrawPrimitiveAtUnit.vert.glsl"]="3d7af88c69422993239dd37d02f95ee4c6a6efc0404e78c7d2fa20f6636e51a7b2a050b04319cc75d48bc2ecd95538aab2e1ea8834804dec05eec1b02b13ee13",
 ["LuaUI/Shaders/DrawPrimitiveAtUnit.geom.glsl"]="843714d4c67b4f7d70117793d5070212cf9646417cb2c18077e383d61ed2221499997e372ed482736c742d702e5d75a19e053bfb58fcdd484610828a4e3b56b3",
 ["LuaUI/Shaders/DrawPrimitiveAtUnit.frag.glsl"]="76831ae32e50431b69769ca0e39461ecb7ab6528e76bc735cb84deae540b35c21e1ed32a58d1fb1234044c5e1d1a4198113256b4030fe707db12beec1360ef38",
}
local function archive(name,raw)
 local source=native and family.LoadArchiveSource31657(name) or VFS.LoadFile(name,VFS.ZIP_ONLY)
 if type(source)~="string" then error("Original unit primitive source missing: "..name) end
 if native and VFS.CalculateHash(source,1)~=identities[name] then error("Original unit primitive source identity changed: "..name) end
 return native and not raw and family.PrepareSource31657(source) or source
end
local function execute(source,name,environment)
 local chunk,message=(loadstring or load)(source,"@"..name.." [native unit primitive scope]")
 if not chunk then error(message) end
 if setfenv then setfenv(chunk,environment) end
 return chunk()
end
local source=archive(path)
if not native then return execute(source,path,original) end
local privateGL=family.CreateSelectedContext31657()
setmetatable(privateGL,{__index=gl})
local environment=setmetatable({gl=privateGL},{__index=original})
local privateVFS=setmetatable({},{__index=VFS})
environment.VFS=privateVFS
privateVFS.Include=function(name,...)
 if identities[name] then return execute(archive(name),name,environment) end
 return VFS.Include(name,...)
end
-- The selected game init.lua require shim is a fresh Include on every call,
-- with no module cache. Preserve the atlas flip and the caller environment.
environment.require=function(name,requestedEnvironment,mode)
 local fullPath=name..".lua"
 if identities[fullPath] then return execute(archive(fullPath),fullPath,requestedEnvironment or environment) end
 local result=original.require(name,requestedEnvironment or environment,mode)
 return result
end
-- Current USETEXTURE=0 source performs no texture lookup or bind.
local helper="modules/graphics/instancevbotable.lua"
privateGL.InstanceVBOTable=execute(archive(helper),helper,environment)
local shaderPath="modules/graphics/luashader.lua"
local originalShader=execute(archive(shaderPath),shaderPath,environment)
local shader=setmetatable({isGeometryShaderSupported=true},{__index=originalShader})
-- This local capability is backed by the original GS strip consumer. The
-- global GL4, GLSL and geometry-shader capabilities are unchanged.
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
 local quaternion=originalShader.GetQuaternionDefs()
 local defines=originalShader.CreateShaderDefinesString(cache.shaderConfig)
 local function complete(text)
  return text:gsub("//__ENGINEUNIFORMBUFFERDEFS__",engine):gsub("//__DEFINES__",defines):gsub("//__QUATERNIONDEFS__",quaternion)
 end
 cache.vsSrcComplete,cache.gsSrcComplete,cache.fsSrcComplete=complete(vertex),complete(geometry),complete(fragment)
 local program=privateGL.CreateProgram(cache.shaderName,vertex,geometry,fragment,engine,quaternion,defines,cache.uniformFloat or {},cache.uniformInt or {})
 -- 31657 records dpatUsesGeometryShader on the returned Lua table and its
 -- Shutdown finalizes only tables. All GPU state remains in this same owner.
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
local originalInfo=widget.GetInfo
widget.GetInfo=function(self,...)
 local info=originalInfo(self,...)
 if not family.SelectedAvailable() then error("Original unit primitive consumer became unavailable") end
 if info.depends then
  if #info.depends~=1 or info.depends[1]~="gl4" then error("Original unit primitive dependency contract changed") end
  info.depends=nil
 end
 return info
end
Spring.Echo("[BAR Metal4] Source-verified 31657 selected units: "..path)
return result
