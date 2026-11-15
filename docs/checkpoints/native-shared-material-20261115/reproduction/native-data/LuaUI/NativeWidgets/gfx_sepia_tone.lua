-- SPDX-License-Identifier: GPL-2.0-or-later
-- Original widget selects/controls Sepia; this private scope supplies its exact native pair.
local original = getfenv and getfenv(1) or _ENV
local native = Platform and Platform.renderBackend == "Metal4"
local path = "LuaUI/Widgets/gfx_sepia_tone.lua"
local text = native and gl.NativeSepiaLoadSource() or VFS.LoadFile(path, VFS.ZIP_ONLY)
if type(text) ~= "string" then error("Original Sepia archive source missing") end
local pair = native and gl.NativeSepiaContext() or nil
if native and not pair.SharedPath and (VFS.CalculateHash(text, 1) ~= "d6d310eac2f86fed0c4e9108067577a2c337e8b4494ce2667e45d9b810cd2da34ae3b6dd83f2a64102ee78f0c85db71d91f5352030ed8d54d799e19b893a3be8" and VFS.CalculateHash(text, 1) ~= "66ae2c235ef17f2da1e371d357996925c630631487b0dabf12caeb1704a5aeb023f924a3be20d8deca8fe99108026df642544294ec964b6fc73d0d5765074e52") then error("Original Sepia archive source identity changed") end
local environment = original
if native then
 -- GLSL is the offline compiler input; its generated Lua copy and linked native
 -- admission must match exactly. Old mode continues to consume archive GLSL.
 local generated = pair.SharedPath and VFS.Include("LuaUI/NativeWidgets/sepia_pilot_program.lua", nil, VFS.RAW_FIRST) or nil
 local active, texture, blend = nil, nil, false
 local scope = setmetatable({}, {__index = gl})
 local function live(program)
  if program.deleted then error("Original Sepia program was deleted") end
 end
 scope.LuaShader = setmetatable({GetEngineUniformBufferDefs = gl.LuaShader.GetEngineUniformBufferDefs}, {
  __call = function(_, params, name)
   if generated then params.vertex=generated.vertex; params.fragment=generated.fragment; params.uniformInt=generated.uniformInt end
   local p={linked=pair.LinkProgram(params,name)}
   function p:Initialize() live(self); return true end -- exact pair was compiled before this context became available
   function p:Activate() live(self); active=self end
   function p:Deactivate() if active==self then active=nil end end
   function p:Finalize() self:Deactivate(); pair.DeleteProgram(self.linked); self.deleted=true end
   function p:ActivateWith(fn)
    self:Activate(); local ok, failure=pcall(fn); self:Deactivate(); if not ok then error(failure) end
   end
   function p:SetUniform(name,...)
    live(self); pair.SetUniform(self.linked,name,{...},false)
   end
   function p:SetUniformInt(name,...)
    live(self); pair.SetUniform(self.linked,name,{...},true)
   end
   -- Optional ordinary Lua values support additional generated uniforms without
   -- new native branches. Values are applied at link, never reflected per draw.
   if generated and generated.uniforms then
    for name,value in pairs(generated.uniforms) do
     pair.SetUniform(p.linked,name,value.values,value.integer==true)
    end
   end
   return p
  end,
 })
 scope.CreateShader = pair.ValidateProgram -- private capability, backed by the compiled Sepia consumer
 scope.GetVBO = pair.GetVBO
 scope.GetVAO = function()
  local vao={}
  function vao:AttachVertexBuffer(b) if self.deleted or self.vertex then error("Sepia vertex attachment changed") end; self.vertex=b end
  function vao:AttachIndexBuffer(b) if self.deleted or self.index then error("Sepia index attachment changed") end; self.index=b end
  function vao:AttachInstanceBuffer(b) if self.deleted or self.instance then error("Sepia instance attachment changed") end; self.instance=b end
  function vao:Delete() self.deleted=true;self.vertex=nil;self.index=nil;self.instance=nil end
  function vao:DrawArrays(mode,count,...)
   if self.deleted or mode~=GL.TRIANGLES or count~=3 or select("#",...)~=0 or not active or not texture then error("Original Sepia triangle/program/input changed") end
   live(active)
   if blend then error("Original Sepia requires blending disabled") end
   pair.Draw(active.linked,texture,self.vertex,self.index,self.instance)
  end
  return vao
 end
 scope.Texture = function(unit,value)
  if unit~=0 then error("Original Sepia sampler unit changed") end
  local result=gl.Texture(unit,value)
  if value~=false then texture=value end
  return result
 end
 scope.Blending=function(value,...)
  if select("#",...)~=0 then error("Original Sepia blending argument changed") end
  local result=gl.Blending(value)
  blend=value~=false
  return result
 end
 environment=setmetatable({gl=scope},{__index=original})
end
local chunk,failure=(loadstring or load)(text,"@"..path.." [native Sepia scope]")
if not chunk then error(failure) end
if setfenv then setfenv(chunk,environment) end
return chunk()
