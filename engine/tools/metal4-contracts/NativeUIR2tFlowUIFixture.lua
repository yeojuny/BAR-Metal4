-- Unmodified actual FlowUI and R2tHelper are loaded by the C++ harness first.
local a=gl.CreateTexture(100,60,{target=GL.TEXTURE_2D,format=GL.RGBA,fbo=true})
local b=gl.CreateTexture(200,120,{target=GL.TEXTURE_2D,format=GL.RGBA,fbo=true})
local function shape()
	WG.FlowUI.Draw.RectRound(110,210,190,250,4,1,1,1,1,{.2,.4,.6,.3},{.8,.6,.4,.7})
end
local cached=gl.CreateList(shape)
assert(cached~=0)
R2tHelper.RenderInRect(a,100,200,200,260,shape,true)
R2tHelper.RenderInRect(b,100,200,200,260,function()gl.CallList(cached)end,true)
local composed=gl.CreateList(function() R2tHelper.BlendTexRect(a,100,200,200,260,true) end)
assert(composed~=0)
gl.CallList(composed)
R2tHelper.RenderInRect(a,100,200,200,260,shape,true,{5,6,20,10})
gl.CallList(composed)
-- Same original UiElement call as actual gui_advplayerslist_gameinfo.lua's
-- drawBackground. Includes the real background PNG and all gradient layers.
WG.FlowUI.Callin.ViewResize1(800,600)
-- Actual HUD panels are short:16px outline width collapses the inner rect.
local c=gl.CreateTexture(100,22,{target=GL.TEXTURE_2D,format=GL.RGBA,fbo=true})
R2tHelper.RenderInRect(c,100,200,200,222,function()
	WG.FlowUI.Draw.Element(100,200,200,222,1,0,0,1,1,1,0,1,nil,nil,nil,nil)
end,true)
local d=gl.CreateTexture(100,22,{target=GL.TEXTURE_2D,format=GL.RGBA,fbo=true})
R2tHelper.RenderInRect(d,10,20,110,42,function()
	WG.FlowUI.Draw.RectRoundOutline(10,20,110,42,4,16,1,1,1,1,{.4,.6,.8,.37},{1,1,1,0})
end,true)
