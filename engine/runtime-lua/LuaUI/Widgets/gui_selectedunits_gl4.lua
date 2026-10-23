-- SPDX-License-Identifier: GPL-2.0-or-later
-- Execute the full exact30770 selection widget with its paired native program family.
local path="LuaUI/Widgets/gui_selectedunits_gl4.lua"
local original=getfenv and getfenv(1) or _ENV
local native=Platform and Platform.renderBackend=="Metal4"
local family=native and gl.NativeUnitPrimitive
if native and (not family or not family.SelectedAvailable()) then error("Original unit primitive consumer is unavailable") end
local identities={
 ["LuaUI/Widgets/gui_selectedunits_gl4.lua"]="1e5ca9d347ac083d152a5a8ba3d0769fc289e263e17de4303362587cbb46339cef6fea367ba8994d51ddd099f1c4a719422f03ac2062a20b4ee155cdc759e580",
 ["LuaUI/Include/DrawPrimitiveAtUnit.lua"]="403de85b552602cc129ed14f8bd2758ae8a65d4cf7d0b0c3678df166be51990aa809796bd52a30e26e6b4f26481214fa1b8fb631850ddbd826e959874d869552",
 ["modules/graphics/instancevbotable.lua"]="8cbe685932529758aac4907c541205d43ebbd532072e67aa7f0901126dee2ee8d2d4d650b1f9f2b368c36cd6d305a390b33c1fe5c0e4274d1c46906d0568ba28",
 ["modules/graphics/luashader.lua"]="577e7ebebe099c7a66b49a8c35613270e7116fba27fa1f5b772a8dd5d325eb645be41a81ac0ae515f673b0011d55e5fda58fba28fc499dfe8d56761285ba5179",
}
local function archive(name)
 local source=native and family.LoadArchiveSource(name) or VFS.LoadFile(name,VFS.ZIP_ONLY)
 if type(source)~="string" then error("Original unit primitive source missing: "..name) end
 if native and VFS.CalculateHash(source,1)~=identities[name] then error("Original unit primitive source identity changed: "..name) end
 return native and family.PrepareSource(source) or source
end
local function execute(source,name,environment)
 local chunk,message=(loadstring or load)(source,"@"..name.." [native unit primitive scope]")
 if not chunk then error(message) end
 if setfenv then setfenv(chunk,environment) end
 return chunk()
end
local source=archive(path)
if not native then return execute(source,path,original) end
local privateGL=family.CreateSelectedContext()
setmetatable(privateGL,{__index=gl})
local environment=setmetatable({gl=privateGL},{__index=original})
local privateVFS=setmetatable({},{__index=VFS})
environment.VFS=privateVFS
privateVFS.Include=function(name,...)
 if identities[name] then return execute(archive(name),name,environment) end
 return VFS.Include(name,...)
end
local texture=privateGL.Texture
privateGL.Texture=function(unit,value)
 -- This is the sole original lookup/bind/enable/disable event owner.
 local result=gl.Texture(unit,value)
 if result then texture(unit,value) end -- retain the actual shared binding only
 return result
end
local helper="modules/graphics/instancevbotable.lua"
privateGL.InstanceVBOTable=execute(archive(helper),helper,environment)
local shaderPath="modules/graphics/luashader.lua"
local originalShader=execute(archive(shaderPath),shaderPath,environment)
local shader=setmetatable({isGeometryShaderSupported=true},{__index=originalShader})
-- This private DPAT path is backed by all three actual selected native programs. The
-- global GL4, GLSL and geometry-shader capabilities are unchanged.
-- The helper reuses one mutable source-cache table for three program names.
-- Retain actual native programs by their source name/config; a cached real
-- program is returned rather than incorrectly selecting the NoGS fallback.
local programCaches={}
shader.CheckShaderUpdates=function(cache,delay)
 local defines=originalShader.CreateShaderDefinesString(cache.shaderConfig)
 local previous=programCaches[cache.shaderName]
 if previous and previous.defines==defines and not cache.forceupdate and Spring.DiffTimers(Spring.GetTimer(),previous.updated)<=(delay or 0.5) then return previous.program end
 local vertex=(cache.vssrcpath and VFS.LoadFile(cache.vssrcpath)) or cache.vsSrc
 local fragment=(cache.fssrcpath and VFS.LoadFile(cache.fssrcpath)) or cache.fsSrc
 local geometry=(cache.gssrcpath and VFS.LoadFile(cache.gssrcpath)) or cache.gsSrc
 if previous and previous.defines==defines and vertex==previous.vertex and fragment==previous.fragment and geometry==previous.geometry and not cache.forceupdate then
  previous.updated=Spring.GetTimer()
  return previous.program
 end
 local engine=originalShader.GetEngineUniformBufferDefs()
 local quaternion=originalShader.GetQuaternionDefs()
 local program=privateGL.CreateProgram(cache.shaderName,vertex,geometry,fragment,engine,quaternion,defines,cache.uniformFloat or {},cache.uniformInt or {})
 cache.vsSrc,cache.fsSrc,cache.gsSrc=vertex,fragment,geometry
 cache.forceupdate=nil;cache.updateFlag=true;cache.lastshaderupdate=Spring.GetTimer()
 local function complete(text)
  return text:gsub("//__ENGINEUNIFORMBUFFERDEFS__",engine):gsub("//__DEFINES__",defines):gsub("//__QUATERNIONDEFS__",quaternion)
 end
 cache.vsSrcComplete,cache.gsSrcComplete,cache.fsSrcComplete=complete(vertex),complete(geometry),complete(fragment)
 programCaches[cache.shaderName]={program=program,defines=defines,vertex=vertex,fragment=fragment,geometry=geometry,updated=cache.lastshaderupdate}
 return program
end
privateGL.LuaShader=shader
local result=execute(source,path,environment)
return result
