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
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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
/*@@HELPERS@@*/
int DispatchImpl(lua_State* L,const std::string& op){auto& r=GetRecorder();const int args=lua_gettop(L);
/*@@OPERATIONS@@*/
throw std::runtime_error("fixture excludes gl."+op);}
/*@@DISPATCH@@*/
unsigned checks=0;
void Check(bool value,const char* message){++checks;Require(value,message);}
void Run(lua_State* L,const std::string& code){if(luaL_loadbuffer(L,code.data(),code.size(),"@native-world-fixture")||lua_pcall(L,0,0,0))throw std::runtime_error(lua_tostring(L,-1));}
std::string Read(const char* path){std::ifstream f(path);Require(bool(f),"fixture source missing");return {std::istreambuf_iterator<char>(f),{}};}
void Write(const char* path,const NativeWorldDrawFrame& frame)
{
	std::ofstream f(path);Require(bool(f),"cannot save source-produced frame");f<<std::setprecision(9)<<"{\"draws\":[";
	bool first=true;for(const auto& d:frame.draws){if(!first)f<<',';first=false;f<<"{\"sourcePrimitive\":"<<int(d.sourcePrimitive)<<",\"depthTest\":"<<d.world.depthTest<<",\"depthWrite\":"<<d.world.depthWrite<<",\"lineWidth\":"<<d.world.lineWidth<<",\"vertices\":[";
		for(size_t i=0;i<d.vertices.size();++i){if(i)f<<',';const auto& v=d.vertices[i];f<<'[';for(size_t q=0;q<4;++q){if(q)f<<',';f<<v.position[q];}for(float c:v.color)f<<','<<c;for(float uv:v.uv)f<<','<<uv;f<<']';}f<<"]}";}f<<"]}\n";
}
int main(int argc,char** argv)
{
	try{Require(argc==4,"usage: fixture original-build-widget original-attack-widget output-frame");lua_State* L=luaL_newstate();Require(L!=nullptr,"Lua state failed");luaL_openlibs(L);
		lua_newtable(L);for(const char* op:{"CreateList","CallList","DeleteList","BeginEnd","Vertex","Color","LineWidth","PointSize","DepthMask","DepthTest","PushMatrix","PopMatrix","Translate","Scale","Rotate"}){lua_pushstring(L,op);lua_pushcclosure(L,Dispatch,1);lua_setfield(L,-2,op);}lua_setglobal(L,"gl");
		Run(L,R"LUA(
local function unsupported(_,key)error('unsupported fixture call '..key)end
setmetatable(gl,{__index=unsupported})
widget={};WG={};Game={mapSizeX=128,mapSizeZ=128,gameSpeed=30,gravity=130};UnitDefs={[1]={xsize=4,zsize=4}}
Spring={GetActiveCommand=function()return 1,-1 end,GetMouseState=function()return 23,34 end,TraceScreenRay=function()return 'ground',{32,0,32}end,
 Pos2BuildPos=function(_,x,y,z)return x,y,z end,TestBuildOrder=function()return 0 end,GetBuildFacing=function()return 0 end,
 GetGroundHeight=function(x,z)return x/8+z/16 end,GetTimer=function()return 0 end,DiffTimers=function(a,b)return a-b end}
setmetatable(Spring,{__index=unsupported});GL={QUADS=7,TRIANGLES=4,TRIANGLE_FAN=6,TRIANGLE_STRIP=5,LINES=1,LINE_STRIP=3,LINE_LOOP=2,POINTS=0}
)LUA");
		Run(L,Read(argv[1]));Run(L,"widget:Update()");auto& r=GetRecorder();auto& world=GetWorldDrawRecorder();
		const NativeUIMatrix view{1,0,0,0, 0,0,1,0, 0,-1,0,0, -32,32,0,1};const auto projection=OrthoMatrix(-32,32,-32,32,-32,32);
		r.BeginFrame(64,64);world.BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::PreUnit,view,projection,IdentityMatrix(),true);Run(L,"widget:DrawWorldPreUnit()");r.EndWorldStage();auto first=world.EndFrame();r.EndFrame();
		Check(first->draws.size()==8,"original blocked2x2footprint did not produce4fills+4outlines");Check(lists.lists.size()==1,"original cache list missing");
		for(size_t i=0;i<8;++i){const auto& d=first->draws[i];Check(!d.world.depthTest&&!d.world.depthWrite,"original overlay depth state differs");Check(d.sourcePrimitive==(i<4?NativeDrawPrimitive::TriangleFan:NativeDrawPrimitive::LineLoop),"original fill/outline order differs");Check(d.vertices.size()==(i<4?10:8),"original octagon topology differs");Check(d.indices.size()==(i<4?24:16),"original octagon index count differs");Check(std::abs(d.vertices[0].color[3]-(i<4?.33f:.165f))<.00001,"original continuous alpha differs");}
		Check(std::abs(first->draws[0].vertices[0].position[0]+.25f)<.00001,"original first cell centerx differs");Check(std::abs(first->draws[0].vertices[0].position[1]-.25f)<.00001,"original worldz projection differs");
		r.BeginFrame(64,64);world.BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::PreUnit,view,projection,IdentityMatrix(),true);Run(L,"widget:DrawWorldPreUnit()");r.EndWorldStage();auto cached=world.EndFrame();r.EndFrame();Check(lists.next==1,"unchanged original cache was recompiled");Check(cached->draws.size()==8,"original cached list replay lost geometry");Check(first->draws[0].vertices[0].position==cached->draws[0].vertices[0].position,"same original cached geometry changed");
		Write(argv[3],*first);Run(L,"widget:Shutdown();widget:Shutdown()");Check(lists.lists.empty(),"original Shutdown failed to delete list idempotently");
		// Full AttackAoE chunk with fixture-only private helper export. The
		// original function body/source is unchanged; no product Lua is edited.
		Run(L,R"LUA(
widget={};UnitDefs={};CMD={ATTACK=20,MANUALFIRE=105};GameCMD={UNIT_SET_TARGET=34923,UNIT_SET_TARGET_NO_GROUND=34924,MANUAL_LAUNCH=34569}
math.tau=2*math.pi;math.mix=function(a,b,t)return a+(b-a)*t end;math.diag=function(x,z)return math.sqrt(x*x+z*z)end
math.normalize=function(x,y,z)local n=math.sqrt(x*x+y*y+z*z);return x/n,y/n,z/n end
setmetatable(Spring,{__index=function(_,key)return function()error('unsupported Spring.'..key)end end})
gl.DrawGroundCircle=function()error('unreached ground producer in focused Attack fixture')end;gl.LuaShader={}
)LUA");
		Run(L,Read(argv[2])+"\n_nativeAttack={DrawNoExplode=DrawNoExplode}\n");
		r.BeginFrame(64,64);world.BeginFrame();r.BeginWorldStage(NativeWorldDrawStage::PreUnit,view,projection,IdentityMatrix(),true);
		Run(L,"_nativeAttack.DrawNoExplode({source={x=32,y=0,z=16},target={x=32,y=0,z=48},weaponInfo={aoe=8,range=32},distanceFromCamera=2048,colors={base={1,.25,.5,.8}}})");
		r.EndWorldStage();auto attack=world.EndFrame();r.EndFrame();Check(attack->draws.size()==1,"original Attack scatter missing");const auto& scatter=attack->draws[0];Check(scatter.primitive==NativeDrawPrimitive::Lines&&scatter.vertices.size()==4,"original Attack two-lines topology differs");Check(scatter.world.lineWidth==1.5f&&!scatter.world.depthTest,"original Attack width/depth differs");Check(std::abs(scatter.vertices[0].color[3]-.4f)<.00001,"original Attack pulse alpha differs");
		lua_close(L);std::cout<<checks<<" actual-original Lua world checks passed\n";return 0;
	}catch(const std::exception& error){GetRecorder().AbortFrame();GetWorldDrawRecorder().AbortFrame();std::cerr<<error.what()<<'\n';return 1;}
}
