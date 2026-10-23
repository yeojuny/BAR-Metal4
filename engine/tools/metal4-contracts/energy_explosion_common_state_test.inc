/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
namespace EnergyCommonStateTest {
using namespace rendering;
using namespace rendering::ui;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
/*@@HELPERS@@*/
int DispatchImpl(lua_State* L,const std::string& op){auto& r=GetRecorder();const int args=lua_gettop(L);
/*@@OPERATIONS@@*/
throw std::runtime_error("Excluded common energy fixture operation "+op);}
/*@@DISPATCH@@*/
void Install(lua_State* L){lua_getglobal(L,"gl");for(const char* op:{"DepthTest","DepthMask","Culling","AlphaTest","Color","ColorMask","Scissor","PolygonOffset","PolygonMode","StencilTest","Blending","BeginEnd","Vertex"}){lua_pushstring(L,op);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,op);}lua_pop(L,1);}
void Begin(){auto& r=GetRecorder();r.BeginFrame(64,64);GetWorldDrawRecorder().BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::World,IdentityMatrix(),IdentityMatrix(),IdentityMatrix(),false);}
void End(){auto& r=GetRecorder();r.EndWorldStage();GetWorldDrawRecorder().EndFrame();r.EndFrame();}
void Following(lua_State* L,std::uint64_t effectSequence){
	Run(L,"gl.BeginEnd(GL.TRIANGLES,function()gl.Vertex(-.5,-.5,0);gl.Vertex(.5,-.5,0);gl.Vertex(0,.5,0)end)","following actual generic world draw");
	auto& r=GetRecorder();r.EndWorldStage();auto frame=GetWorldDrawRecorder().EndFrame();r.EndFrame();
	CheckTest(frame->draws.size()==1,"Original EEP state must reach actual following generic triangle");const auto& d=frame->draws.front();CheckTest(d.sequence>effectSequence,"Original EEP and following generic geometry share actual monotonic world order");
	CheckTest(d.world.depthTest&&d.world.depthCompare==ScreenAlphaCompare::LessEqual&&d.world.depthWrite,"Following draw inherits original LEQUAL and restored depth write");
	CheckTest(!d.world.cullEnabled&&!d.world.stencilTest&&!d.world.polygonOffset,"Following draw inherits original disabled cull/stencil/offset");
	CheckTest(d.world.frontPolygonMode==NativePolygonMode::Fill&&d.world.backPolygonMode==NativePolygonMode::Fill,"Following draw inherits original fill");
	CheckTest(!d.surface.alphaTest.enabled&&!d.surface.scissor,"Following draw inherits disabled alpha/scissor");
	CheckTest(d.surface.blend.enabled&&d.surface.blend.sourceColor==ScreenBlendFactor::SourceAlpha&&d.surface.blend.destinationColor==ScreenBlendFactor::OneMinusSourceAlpha,"Following draw inherits original final alpha blend");
	CheckTest(d.world.colorWrite==std::array<bool,4>{true,true,true,true}&&d.vertices[0].color==ScreenColor{1,1,1,1},"Following draw inherits original color and color-mask");Begin();
}
}
