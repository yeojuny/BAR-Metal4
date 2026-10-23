/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
namespace LightningCommonStateTest {
using namespace rendering;
using namespace rendering::ui;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
/*@@HELPERS@@*/
int DispatchImpl(lua_State* L,const std::string& op){auto& r=GetRecorder();const int args=lua_gettop(L);
/*@@OPERATIONS@@*/
// Fixture-owned named-image transport; the actual common enable bit below is
// the production boolean behavior. The real VFS/DevIL loader is a separate gate.
if(op=="Texture") {
 int index=1;if(lua_isnumber(L,1)){Require(lua_tonumber(L,1)==0,"Only original texture unit0");index=2;}
 if(lua_isboolean(L,index)){r.SetTextureEnabled(lua_toboolean(L,index));lua_pushboolean(L,true);return 1;}
 const std::string name=luaL_checkstring(L,index);Require(name=="bitmaps/projectiletextures/flare2.tga","Unexpected named texture transport");CBitmap bitmap;Require(bitmap.Load(name,1,4,0),"Original named-image fixture missing");auto image=std::make_shared<ScreenTexture>();image->sourceName=name;image->width=bitmap.xsize;image->height=bitmap.ysize;image->bytesPerRow=image->width*4;image->bytes=bitmap.bytes;r.SetTexture(image,{ScreenFilter::Linear,ScreenWrap::Repeat,ScreenWrap::Repeat});lua_pushboolean(L,true);return 1;
}

throw std::runtime_error("Excluded common Lightning fixture operation "+op);}
/*@@DISPATCH@@*/
void Install(lua_State* L){lua_getglobal(L,"gl");for(const char* op:{"DepthTest","DepthMask","Culling","AlphaTest","Color","ColorMask","Scissor","PolygonOffset","PolygonMode","StencilTest","Blending","Texture","BeginEnd","Vertex"}){lua_pushstring(L,op);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,op);}lua_pop(L,1);}
void Begin(){auto& r=GetRecorder();r.BeginFrame(64,64);GetWorldDrawRecorder().BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::World,IdentityMatrix(),IdentityMatrix(),IdentityMatrix(),false);}
void End(){auto& r=GetRecorder();r.EndWorldStage();GetWorldDrawRecorder().EndFrame();r.EndFrame();}
void Following(lua_State* L,std::uint64_t effectSequence){
 Run(L,"gl.BeginEnd(GL.TRIANGLES,function()gl.Vertex(-.5,-.5,0);gl.Vertex(.5,-.5,0);gl.Vertex(0,.5,0)end)","following actual generic world draw");
 auto& r=GetRecorder();r.EndWorldStage();auto frame=GetWorldDrawRecorder().EndFrame();r.EndFrame();
 CheckTest(frame->draws.size()==1,"Original Lightning mutations reach actual following generic triangle");const auto& d=frame->draws.front();CheckTest(d.sequence>effectSequence,"Lightning and following generic geometry share actual monotonic world order");
 CheckTest(!d.world.depthTest&&d.world.depthCompare==ScreenAlphaCompare::Greater&&d.world.depthWrite,"Following draw inherits restored disabled depth/write and original Greater comparison");
 CheckTest(!d.world.cullEnabled,"Following draw inherits original disabled cull");
 CheckTest(d.surface.blend.enabled&&d.surface.blend.sourceColor==ScreenBlendFactor::SourceAlpha&&d.surface.blend.destinationColor==ScreenBlendFactor::OneMinusSourceAlpha,"Following draw inherits original final alpha blend");
 CheckTest(d.vertices[0].color==ScreenColor{.2f,.3f,.4f,.5f},"Lightning does not reset inherited current color");
 CheckTest(!r.CurrentState().draw.texture && r.CurrentState().draw.material==ScreenMaterial::Solid,"Original impact Texture(false) restores actual common disable bit");Begin();
}
}
