-- Pure-host Lua adapter wiring test; no engine or GPU is created.
local root=assert(arg[1])
local generated=assert(loadfile(arg[2] or root..'/data/LuaUI/NativeWidgets/sepia_pilot_program.lua'))()
local calls={}
getfenv=function() return _G end
setfenv=function(fn,env)
 for i=1,100 do local name=debug.getupvalue(fn,i);if not name then break end;if name=='_ENV' then debug.upvaluejoin(fn,i,function() return env end,1);break end end
 return fn
end
Platform={renderBackend='Metal4'};GL={TRIANGLES=4}
local original=[[
local p=gl.LuaShader({vertex='archive vertex',fragment='archive fragment',uniformInt={screenCopyTex=0}},': Sepia')
assert(p:Initialize());p:SetUniform('gamma',1.25);p:SetUniform('exposure',2.0);p:SetUniform('bias',0.1,0.2,0.3);p:SetUniformInt('viewPos',5,6)
p:Activate();gl.Texture(0,'screen-version');gl.Blending(false)
local vao=gl.GetVAO();vao:AttachVertexBuffer({});vao:AttachIndexBuffer({});vao:AttachInstanceBuffer({});vao:DrawArrays(GL.TRIANGLES,3)
p:Finalize();assert(not pcall(function()p:SetUniform('gamma',2)end))
]]
local pair={SharedPath=true}
pair.LinkProgram=function(params)assert(params.vertex==generated.vertex and params.fragment==generated.fragment);calls.link={};return calls.link end
pair.SetUniform=function(p,name,values,integer)assert(p==calls.link);calls[name]={values=values,integer=integer} end
pair.DeleteProgram=function(p)assert(p==calls.link);calls.deleted=true end
pair.Draw=function(p,texture)assert(p==calls.link and texture=='screen-version');calls.draw=true end
pair.ValidateProgram=function()return true end
VFS={ZIP_ONLY=1,RAW_FIRST=2,LoadFile=function()return original end,Include=function()return generated end,CalculateHash=function()return 'edited-supported-widget-no-fixed-hash' end}
gl={LuaShader={},NativeSepiaContext=function()return pair end,NativeSepiaLoadSource=function()return original end,Texture=function()return true end,Blending=function()return true end}
assert(loadfile(root..'/data/LuaUI/NativeWidgets/gfx_sepia_tone.lua'))()
assert(calls.exposure.values[1]==2 and #calls.bias.values==3 and calls.viewPos.integer)
assert(calls.deleted and calls.draw)
pair.SharedPath=false
local ok,failure=pcall(assert(loadfile(root..'/data/LuaUI/NativeWidgets/gfx_sepia_tone.lua')))
assert(not ok and failure:find('Original Sepia archive source identity changed',1,true))
print('Lua shared loader accepts edited widget and generated stages; old loader retains strict widget identity; generic uniforms, linked draw and deletion passed')
