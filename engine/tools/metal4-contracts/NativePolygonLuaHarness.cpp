/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Production dispatch blocks are inserted verbatim by the adjacent runner.
// Real Lua 5.1-float executes the full original Build Placement widget. Only
// documented Spring queries/list IDs are fixture-owned; unknown calls error.
#include "Rendering/Backend/NativeUIRecorder.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <limits>
#include <stdexcept>
#include <string>
void LuaCreateMutex(lua_State*){} void LuaDestroyMutex(lua_State*){} void LuaLinkMutex(lua_State*,lua_State*){} void LuaMutexLock(lua_State*){} void LuaMutexUnlock(lua_State*){}
int spring_lua_unsynced_rand(lua_State* L){return luaL_error(L,"fixture excludes RNG");}
int spring_lua_unsynced_srand(lua_State* L){return luaL_error(L,"fixture excludes RNG");}
void spring_lua_ftoa(float,char*,int){throw std::runtime_error("fixture excludes formatting");}
void spring_lua_format(float,const char*,char*){throw std::runtime_error("fixture excludes formatting");}
using namespace rendering;using namespace rendering::ui;
constexpr int L_ERROR=1,L_WARNING=2;
#define LOG_L(level,...) do{(void)(level);std::fprintf(stderr,__VA_ARGS__);std::fputc('\n',stderr);}while(false)
struct LuaOpenGL{inline static bool enabled=true;static bool IsDrawingEnabled(lua_State*){return enabled;}static void SetDrawingEnabled(lua_State*,bool value){enabled=value;}};
struct Lists{
	unsigned next=0;std::map<unsigned,std::shared_ptr<const NativeUICommandList>> lists;
	unsigned NewNativeList(std::shared_ptr<const NativeUICommandList> list){lists[++next]=std::move(list);return next;}
	std::shared_ptr<const NativeUICommandList> GetNativeList(unsigned id){auto p=lists.find(id);return p==lists.end()?nullptr:p->second;}
	void FreeNativeList(unsigned id){lists.erase(id);}
} lists;
struct CLuaHandle{static Lists& GetActiveDisplayLists(lua_State*){return lists;}};
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

struct CommandColors{int pattern=0xf0f0,factor=3;int StipplePattern()const{return pattern;}int StippleFactor()const{return factor;}} cmdColors;
int appliedFactor=0;std::uint16_t appliedPattern=0;
void ReferenceStipple(int factor,std::uint16_t pattern){appliedFactor=factor;appliedPattern=pattern;}
#define glLineStipple ReferenceStipple
struct CLineDrawer{bool lineStipple=false;float stippleTimer=.2f;void SetupLineStipple();std::pair<int,std::uint16_t> GetNativeStippleSettings()const;} lineDrawer;
/*@@LINE_METHODS@@*/
/*@@HELPERS@@*/
int DispatchImpl(lua_State* L,const std::string& op){auto& r=GetRecorder();const int args=lua_gettop(L);
/*@@OPERATIONS@@*/
throw std::runtime_error("fixture excludes gl."+op);}
/*@@DISPATCH@@*/
unsigned checks=0;
void Check(bool v){++checks;Require(v,("original polygon Lua check "+std::to_string(checks)).c_str());}
void Run(lua_State* L,const std::string& code){if(luaL_loadbuffer(L,code.data(),code.size(),"@native-polygon-fixture")||lua_pcall(L,0,0,0))throw std::runtime_error(lua_tostring(L,-1));}
std::string Read(const char* path){std::ifstream f(path);Require(bool(f),"source missing");return {std::istreambuf_iterator<char>(f),{}};}
int TextureFixture(lua_State* L){auto& r=GetRecorder();if(lua_isboolean(L,1)){r.SetTextureEnabled(lua_toboolean(L,1));}else{Require(std::string(luaL_checkstring(L,1))==":n:LuaUI/Images/vignette.dds","fixture excludes unknown image");auto t=std::make_shared<ScreenTexture>();t->width=t->height=1;t->bytesPerRow=4;t->bytes={255,255,255,255};r.SetTexture(t,{ScreenFilter::Nearest,ScreenWrap::Repeat,ScreenWrap::Repeat});}lua_pushboolean(L,true);return 1;}
int main(int argc,char** argv){try{Require(argc==3,"usage: original selectionbox original formation-source");auto* L=luaL_newstate();luaL_openlibs(L);
 lua_newtable(L);for(const char* op:{"CreateList","CallList","DeleteList","BeginEnd","Vertex","Color","Rect","TexRect","PushMatrix","PopMatrix","PolygonMode","LineStipple","LineWidth","PushAttrib","PopAttrib"}){lua_pushstring(L,op);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,op);}lua_pushcfunction(L,TextureFixture);lua_setfield(L,-2,"Texture");lua_setglobal(L,"gl");
 Run(L,R"LUA(
local function unsupported(_,key)error('unsupported fixture '..key)end
setmetatable(gl,{__index=unsupported});widget={};WG={};Game={mapSizeX=128,mapSizeZ=128};LUAUI_DIRNAME='LuaUI/'
GL={QUADS=7,TRIANGLES=4,TRIANGLE_FAN=6,TRIANGLE_STRIP=5,LINES=1,LINE_STRIP=3,LINE_LOOP=2,POINTS=0,FRONT=1028,BACK=1029,FRONT_AND_BACK=1032,POINT=6912,LINE=6913,FILL=6914,ALL_ATTRIB_BITS=1048575,LINE_BIT=4,POLYGON_BIT=8}
configs={};mouseX=5;mouseY=5
Spring={GetViewGeometry=function()return 32,32 end,LoadCmdColorsConfig=function(s)configs[#configs+1]=s end,
 GetSelectionBox=function()return 4.5,4.5,27.5,27.5 end,GetModKeyState=function()return false,false,false,false end,
 GetMiniMapGeometry=function()return 0,0,32,32,false,false end,GetMouseState=function()return mouseX,mouseY,true end}
setmetatable(Spring,{__index=unsupported})
)LUA");
 Run(L,Read(argv[1]));Run(L,"widget:Initialize()");Check(lua_gettop(L)==0);
 auto& r=GetRecorder();lineDrawer.SetupLineStipple();r.BeginFrame(32,32);r.BeginScreenStage();Check(r.CurrentWorldState().stipplePattern==0x1e1e&&r.CurrentWorldState().stippleFactor==3);Run(L,"gl.LineStipple(2,21845);gl.LineStipple(false);widget:DrawScreen()");
 Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Fill);Check(r.CurrentWorldState().backPolygonMode==NativePolygonMode::Fill);Check(!r.CurrentWorldState().lineStipple);Check(r.CurrentWorldState().lineWidth==1.5f);
 r.EndScreenStage();auto frame=r.EndFrame();Check(frame.drawList->Commands().size()==4);
 const auto& black=frame.drawList->Commands()[2];const auto& white=frame.drawList->Commands()[3];Check(black.indexCount>6);Check(white.indexCount>6);Check(white.indexCount<black.indexCount);
 Check(frame.drawList->Commands()[0].indexCount==6);Check(frame.drawList->Commands()[1].indexCount==6);
 // Original minimap selection uses explicit LINE_LOOP, with no polygon mode.
 Run(L,"widget:MousePress(5,5,1);widget:MouseMove(25,25,20,20,1)");r.BeginFrame(32,32);r.BeginScreenStage();Run(L,"widget:DrawInMiniMap(32,32)");r.EndScreenStage();auto mini=r.EndFrame();Check(mini.drawList->Commands().size()==2);Check(mini.drawList->Commands()[1].indexCount>6);
 r.BeginFrame(32,32);r.BeginScreenStage();Run(L,"gl.BeginEnd(GL.QUADS,function()gl.PolygonMode(GL.FRONT_AND_BACK,GL.LINE);gl.LineStipple(1,1);gl.Vertex(4,4);gl.Vertex(27,4);gl.Vertex(27,27);gl.Vertex(4,27)end)");Check(r.TakeLegacyInvalidOperation());Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Fill&&!r.CurrentWorldState().lineStipple);r.EndScreenStage();Check(r.EndFrame().drawList->Commands().size()==1);
 // Exact production string path applies original queue side effects and phase.
 r.BeginFrame(32,32);r.BeginScreenStage();Run(L,"gl.LineStipple('springdefault')");Check(lineDrawer.lineStipple);Check(appliedFactor==3&&appliedPattern==0x1e1e);Check(r.CurrentWorldState().stippleFactor==3&&r.CurrentWorldState().stipplePattern==0x1e1e);
 Run(L,"gl.PushAttrib(GL.LINE_BIT+GL.POLYGON_BIT);gl.PolygonMode(GL.BACK,GL.POINT);gl.LineStipple(0,1,-1);gl.PopAttrib()");Check(r.CurrentWorldState().backPolygonMode==NativePolygonMode::Fill);Check(r.CurrentWorldState().stippleFactor==3);Check(r.CurrentWorldState().stipplePattern==0x1e1e);
 Run(L,"held=gl.CreateList(function()gl.PolygonMode(GL.FRONT,GL.LINE);gl.LineStipple('springdefault')end)");lineDrawer.stippleTimer=.4f;cmdColors.pattern=0xaaaa;Run(L,"gl.CallList(held)");Check(r.CurrentWorldState().stipplePattern==0x1e1e);Check(r.CurrentWorldState().backPolygonMode==NativePolygonMode::Fill);Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Line);
 Run(L,"gl.LineStipple(0,1,-1)");Check(r.CurrentWorldState().stippleFactor==1&&r.CurrentWorldState().stipplePattern==2);Run(L,"gl.LineStipple(999,65537,17)");Check(r.CurrentWorldState().stippleFactor==256&&r.CurrentWorldState().stipplePattern==0x8000);
 Run(L,"assert(not pcall(gl.PolygonMode,123,GL.LINE));assert(not pcall(gl.PolygonMode,GL.FRONT,123));assert(not pcall(gl.LineStipple,0/0,1));assert(not pcall(gl.LineStipple,1,math.huge));assert(not pcall(gl.LineStipple,1,1,math.huge))");r.EndScreenStage();r.EndFrame();
 // Original Formation helper is exported unchanged in the adjacent runner.
 Run(L,Read(argv[2]));auto& world=GetWorldDrawRecorder();r.BeginFrame(32,32);world.BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::PreUnit,IdentityMatrix(),IdentityMatrix(),IdentityMatrix(),true);Run(L,"DrawFormationLines(function(nodes)for _,p in ipairs(nodes)do gl.Vertex(p[1],p[2],p[3])end end,2)");r.EndWorldStage();auto commands=world.EndFrame();r.EndFrame();Check(commands->draws.size()==2);for(const auto& d:commands->draws){Check(d.sourcePrimitive==NativeDrawPrimitive::LineStrip);Check(d.world.lineStipple&&d.world.stippleFactor==2&&d.world.stipplePattern==4369);Check(d.world.lineWidth==2);}
 Run(L,"widget:Shutdown();assert(configs[1]=='mouseBoxLineWidth 0' and configs[2]=='mouseBoxLineWidth 1.5')");lua_close(L);std::cout<<checks<<" original selectionbox/formation Lua checks passed\n";
}catch(const std::exception& e){GetRecorder().AbortFrame();GetWorldDrawRecorder().AbortFrame();std::cerr<<e.what()<<'\n';return 1;}}
