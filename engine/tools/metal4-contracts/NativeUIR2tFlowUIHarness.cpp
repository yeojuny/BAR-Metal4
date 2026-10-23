/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Runs hash-verified, unmodified R2tHelper and FlowUI with the engine Lua 5.1
// runtime and the production CPU recorder. This is not a full widget emulator.
#include "Rendering/Backend/NativeUIRecorder.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "lib/tracy/profiler/src/stb_image.h"
#pragma GCC diagnostic pop
#include <cmath>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>

void LuaCreateMutex(lua_State*) {}
void LuaDestroyMutex(lua_State*) {}
void LuaLinkMutex(lua_State*,lua_State*) {}
void LuaMutexLock(lua_State*) {}
void LuaMutexUnlock(lua_State*) {}
int spring_lua_unsynced_rand(lua_State* L) { return luaL_error(L,"fixture excludes engine RNG"); }
int spring_lua_unsynced_srand(lua_State* L) { return luaL_error(L,"fixture excludes engine RNG"); }
void spring_lua_ftoa(float,char*,int) { throw std::runtime_error("fixture excludes engine formatting"); }
void spring_lua_format(float,const char*,char*) { throw std::runtime_error("fixture excludes engine formatting"); }

using namespace rendering;
using namespace rendering::ui;
#define LOG_L(level, ...) do { (void)(level);std::fprintf(stderr,__VA_ARGS__);std::fputc('\n',stderr); } while(false)
namespace {
constexpr int L_WARNING=3;
std::string lastFixtureError;
struct Environment {
	NativeUIRecorder recorder;
	std::unordered_map<unsigned,std::shared_ptr<const NativeUICommandList>> lists;
	struct Texture {std::shared_ptr<NativeUITextureBinding> binding;bool fbo=true;};
	std::unordered_map<std::string,Texture> textures;
	unsigned nextList=0,nextTexture=0,checks=0;
};
Environment& Env(lua_State* L) { return *static_cast<Environment*>(lua_touserdata(L,lua_upvalueindex(1))); }
void Require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
std::string LuaPrimitiveOrigin(lua_State* L)
{
	lua_Debug entry{};if(!lua_getstack(L,1,&entry)||!lua_getinfo(L,"Sl",&entry))return "Lua caller unavailable";
	return std::string(entry.short_src)+":"+std::to_string(entry.currentline);
}
ScreenPrimitive Primitive(int value) {Require(value==7,"fixture only uses original quads");return ScreenPrimitive::Quads;}
Environment& Textures(lua_State* L) {return Env(L);}
struct CLuaHandle {
	struct Lists {
		Environment& e;
		std::shared_ptr<const NativeUICommandList> GetNativeList(unsigned id) {const auto found=e.lists.find(id);return found==e.lists.end()?nullptr:found->second;}
	};
	static Lists GetActiveDisplayLists(lua_State* L) {return {Env(L)};}
};
void Check(Environment& e,bool value,const char* reason) { ++e.checks;if(!value)throw std::runtime_error(reason); }
void Run(lua_State* L,const char* text) { if(luaL_loadstring(L,text)||lua_pcall(L,0,0,0))throw std::runtime_error(lua_tostring(L,-1)); }
void File(lua_State* L,const char* path,int results=0)
{
	if(luaL_loadfile(L,path)||lua_pcall(L,0,results,0)) {
		const char* error=lua_tostring(L,-1);
		throw std::runtime_error(error?error:lastFixtureError.empty()?"Lua fixture failed with non-string error object":lastFixtureError);
	}
}
int Unsupported(lua_State* L) { return luaL_error(L,"unsupported fixture operation: %s",luaL_checkstring(L,2)); }
ScreenBlendFactor Factor(int value)
{
	switch(value) {
		case 0:return ScreenBlendFactor::Zero;case 1:return ScreenBlendFactor::One;
		case 0x302:return ScreenBlendFactor::SourceAlpha;case 0x303:return ScreenBlendFactor::OneMinusSourceAlpha;
		case 0x305:return ScreenBlendFactor::OneMinusDestinationAlpha;
		default:throw std::runtime_error("unsupported fixture blend factor");
	}
}
int DispatchImpl(lua_State* L,const std::string& op)
{
	auto& e=Env(L);auto& r=e.recorder;const int args=lua_gettop(L);
	if(op=="CreateList") {
		luaL_checktype(L,1,LUA_TFUNCTION);r.BeginList();
		if(lua_pcall(L,args-1,0,0)) { r.AbortList();lua_pushnumber(L,0);return 1; }
		e.lists[++e.nextList]=r.EndList();lua_pushnumber(L,e.nextList);return 1;
	}
	if(op=="CallList") { r.CallList(e.lists.at(luaL_checkint(L,1)));return 0; }
	if(op=="CreateTexture") {
		auto identity=std::make_shared<ScreenRenderTargetIdentity>();identity->resourceId=++e.nextTexture;
		identity->width=luaL_checkint(L,1);identity->height=luaL_checkint(L,2);
		auto image=std::make_shared<ScreenTexture>();image->renderTarget=identity;image->width=identity->width;image->height=identity->height;image->flipV=true;
		auto binding=std::make_shared<NativeUITextureBinding>();binding->current=image;
		const auto name="!"+std::to_string(e.nextTexture);e.textures[name]={binding};lua_pushstring(L,name.c_str());return 1;
	}
	if(op=="RenderToTexture") {
		auto binding=e.textures.at(luaL_checkstring(L,1)).binding;auto previous=binding->current;
		auto next=std::make_shared<ScreenTexture>(*previous);++next->generation;
		r.BeginTarget(next,previous->generation?previous:nullptr);
		if(lua_pcall(L,args-2,0,0)) { r.AbortTarget();return lua_error(L); }
		try { r.EndTarget(); } catch(...) { r.AbortTarget();throw; }
		binding->current=next;return 0;
	}
	if(op=="BeginEnd") {
		if(luaL_checkint(L,1)!=7)throw std::runtime_error("fixture only uses original quads");
		r.Begin(ScreenPrimitive::Quads);
		if(lua_pcall(L,args-2,0,0)) { r.AbortPrimitive();return lua_error(L); }
		try { r.End(); } catch(...) { r.AbortPrimitive();throw; }return 0;
	}
	if(op=="Vertex") { r.Vertex(luaL_checknumber(L,1),luaL_checknumber(L,2),luaL_optnumber(L,3,0));return 0; }
	if(op=="TexCoord") {r.TexCoord(luaL_checknumber(L,1),luaL_checknumber(L,2));return 0;}
	if(op=="Color") {
		ScreenColor color{1,1,1,1};
		for(int k=0;k<4;++k) {
			if(lua_istable(L,1)) { lua_rawgeti(L,1,k+1);color[k]=luaL_optnumber(L,-1,color[k]);lua_pop(L,1); }
			else color[k]=luaL_optnumber(L,k+1,color[k]);
		}
		r.Color(color);return 0;
	}
	if(op=="Translate"||op=="Scale") {
		const float x=luaL_checknumber(L,1),y=luaL_checknumber(L,2),z=luaL_checknumber(L,3);
		r.MultMatrix(op=="Scale"?ScaleMatrix(x,y,z):TranslationMatrix(x,y,z));return 0;
	}
	if(op=="PushMatrix") {r.PushMatrix();return 0;}
	if(op=="PopMatrix") {r.PopMatrix();return 0;}
	if(op=="Clear") {
		if(luaL_checkint(L,1)!=0x4000)throw std::runtime_error("fixture only clears original color attachment");
		r.ClearTarget(ScreenColor{float(luaL_checknumber(L,2)),float(luaL_checknumber(L,3)),float(luaL_checknumber(L,4)),float(luaL_checknumber(L,5))});return 0;
	}
	if(op=="Scissor") {
		if(args==1&&!lua_toboolean(L,1))r.SetScissor({});
		else r.SetScissor(ScreenScissor{luaL_checkint(L,1),luaL_checkint(L,2),unsigned(luaL_checkint(L,3)),unsigned(luaL_checkint(L,4))});return 0;
	}
	if(op=="Blending"||op=="BlendFuncSeparate") {
		auto blend=r.CurrentState().draw.blend;
		blend.sourceColor=Factor(luaL_checkint(L,1));blend.destinationColor=Factor(luaL_checkint(L,2));
		blend.sourceAlpha=op=="BlendFuncSeparate"?Factor(luaL_checkint(L,3)):blend.sourceColor;
		blend.destinationAlpha=op=="BlendFuncSeparate"?Factor(luaL_checkint(L,4)):blend.destinationColor;
		if(op=="Blending")blend.enabled=true;r.SetBlend(blend);return 0;
	}
	if(op=="Texture") {
		if(lua_isboolean(L,1)&&!lua_toboolean(L,1))r.SetTexture(nullptr);
		else r.SetTextureResource(e.textures.at(luaL_checkstring(L,1)).binding);lua_pushboolean(L,true);return 1;
	}
	if(op=="TexRect") {
		const float x1=luaL_checknumber(L,1),y1=luaL_checknumber(L,2),x2=luaL_checknumber(L,3),y2=luaL_checknumber(L,4);
		float s1=0,t1=1,s2=1,t2=0;if(lua_toboolean(L,5))std::swap(s1,s2);if(lua_toboolean(L,6))std::swap(t1,t2);
		r.Begin(ScreenPrimitive::Quads);r.TexCoord(s1,t1);r.Vertex(x1,y1);r.TexCoord(s2,t1);r.Vertex(x2,y1);r.TexCoord(s2,t2);r.Vertex(x2,y2);r.TexCoord(s1,t2);r.Vertex(x1,y2);r.End();return 0;
	}
	throw std::runtime_error("unsupported fixture operation");
}
int Dispatch(lua_State* L)
{
	char reason[1024];
	try {return DispatchImpl(L,lua_tostring(L,lua_upvalueindex(2)));}
	catch(const std::exception& error) {lastFixtureError=error.what();Env(L).recorder.AbortPrimitive();std::snprintf(reason,sizeof(reason),"%s",error.what());}
	return luaL_error(L,"%s",reason);
}
void Missing(lua_State* L) {lua_newtable(L);lua_pushcfunction(L,Unsupported);lua_setfield(L,-2,"__index");lua_setmetatable(L,-2);}
void Initialize(lua_State* L,Environment& e)
{
	luaL_openlibs(L);lua_newtable(L);lua_setglobal(L,"widget");lua_newtable(L);lua_setglobal(L,"WG");
	lua_newtable(L);
	for(const char* op:{"CreateList","CallList","CreateTexture","RenderToTexture","BeginEnd","Vertex","TexCoord","Color","Translate","Scale","PushMatrix","PopMatrix","Clear","Scissor","Blending","BlendFuncSeparate","Texture","TexRect"}) {
		lua_pushlightuserdata(L,&e);lua_pushstring(L,op);lua_pushcclosure(L,Dispatch,2);lua_setfield(L,-2,op);
	}
	Missing(L);lua_setglobal(L,"gl");lua_newtable(L);
	for(const auto& [key,value]:std::initializer_list<std::pair<const char*,int>>{{"QUADS",7},{"ONE",1},{"SRC_ALPHA",0x302},{"ONE_MINUS_SRC_ALPHA",0x303},{"ONE_MINUS_DST_ALPHA",0x305},{"COLOR_BUFFER_BIT",0x4000},{"RGBA",0x1908},{"TEXTURE_2D",0xDE1}}) {lua_pushnumber(L,value);lua_setfield(L,-2,key);}
	Missing(L);lua_setglobal(L,"GL");
	Run(L,"Spring={GetConfigFloat=function(_,default)return default end, GetViewGeometry=function()return 800,600 end}; setmetatable(Spring,{__index=function(_,key)error('unsupported Spring.'..key)end})");
}
bool Near(float a,float b) {return std::abs(a-b)<0.001f;}
float Area(const ScreenDrawList& geometry)
{
	float area=0;const auto vertices=geometry.Vertices();const auto indices=geometry.Indices();
	for(std::size_t n=0;n<indices.size();n+=3) {const auto& a=vertices[indices[n]];const auto& b=vertices[indices[n+1]];const auto& c=vertices[indices[n+2]];area+=std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5f;}return area;
}
bool Covered(const ScreenDrawList& geometry,float x,float y)
{
	const auto vertices=geometry.Vertices();const auto indices=geometry.Indices();
	for(std::size_t n=0;n<indices.size();n+=3) {
		const auto& a=vertices[indices[n]];const auto& b=vertices[indices[n+1]];const auto& c=vertices[indices[n+2]];
		const float ab=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x),bc=(c.x-b.x)*(y-b.y)-(c.y-b.y)*(x-b.x),ca=(a.x-c.x)*(y-c.y)-(a.y-c.y)*(x-c.x);
		const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
		if(std::abs(area)>0.00001f && ((ab>=0&&bc>=0&&ca>=0)||(ab<=0&&bc<=0&&ca<=0)))return true;
	}return false;
}
}
int main(int argc,char** argv)
{
	if(argc!=5) {std::cerr<<"Usage: native-r2t-flowui <real gui_flowui.lua> <real r2thelper.lua> <fixture.lua> <real backgroundtile.png>\n";return 1;}
	auto* L=luaL_newstate();Environment e;
	try {
		int width=0,height=0,channels=0;auto* pixels=stbi_load(argv[4],&width,&height,&channels,4);
		Check(e,pixels!=nullptr && width>0 && height>0,"actual FlowUI tile PNG decode failed");
		auto tile=std::make_shared<ScreenTexture>();tile->width=width;tile->height=height;tile->bytesPerRow=std::size_t(width)*4;
		tile->bytes.assign(pixels,pixels+std::size_t(width)*height*4);tile->sourceName="luaui/images/backgroundtile.png";stbi_image_free(pixels);
		auto tileBinding=std::make_shared<NativeUITextureBinding>();tileBinding->current=tile;e.textures[tile->sourceName]={tileBinding,false};
		Initialize(L,e);File(L,argv[1]);File(L,argv[2],1);lua_setglobal(L,"R2tHelper");
		e.recorder.BeginFrame(800,600);e.recorder.BeginScreenStage();File(L,argv[3]);auto frame=e.recorder.EndFrame();
		Check(e,frame.targetPasses.size()==5,"actual helper did not produce shape/update/short UiElement/collapsed-outline targets");
		Check(e,frame.drawList->Commands().size()==2,"actual helper compositions/list replay count differs");
		const auto& a=*frame.targetPasses[0].drawList;
		Check(e,a.Commands().size()==2 && a.Vertices().size()==32,"FlowUI RectRound target was not retained exactly");
		Check(e,Near(a.Vertices()[4].x,14)&&Near(a.Vertices()[4].y,10),"actual helper left/bottom translation differs");
		Check(e,Near(a.Vertices()[6].x,86)&&Near(a.Vertices()[6].y,50),"actual helper target scale differs");
		Check(e,a.Commands()[1].state.blend.sourceAlpha==ScreenBlendFactor::OneMinusDestinationAlpha && a.Commands()[1].state.blend.destinationAlpha==ScreenBlendFactor::One,"actual R2t separate-alpha draw state differs");
		Check(e,frame.targetPasses[1].drawList->Commands().size()==2,"retained FlowUI callback replay missing in RTT");
		Check(e,frame.targetPasses[2].previous==frame.targetPasses[0].destination,"actual scissored update lost previous contents");
		Check(e,frame.targetPasses[2].drawList->Commands()[0].state.scissor==ScreenScissor{5,6,20,10},"actual helper scissor clear differs");
		Check(e,frame.drawList->Commands()[0].state.texture==frame.targetPasses[0].destination && frame.drawList->Commands()[1].state.texture==frame.targetPasses[2].destination,"old/new cached immutable compositions collapsed");
		Check(e,frame.drawList->TexCoords()[0][1]==0 && frame.drawList->TexCoords()[2][1]==1,"actual GL FBO TexRect flip differs");
		Check(e,frame.drawList->Commands()[0].state.blend.sourceColor==ScreenBlendFactor::One,"actual helper premultiplied composition differs");
		const auto& element=*frame.targetPasses[3].drawList;
		Check(e,element.Commands().size()>=4 && element.Vertices().size()>200,"actual GameInfo-style UiElement RTT callback was not recorded");
		bool hasRealTile=false;
		for(const auto& command:element.Commands())hasRealTile|=command.state.texture==tile;
		Check(e,hasRealTile,"actual UiElement tile was omitted/substituted");
		const auto& outline=*frame.targetPasses[4].drawList;
		Check(e,outline.Commands().size()==2 && outline.Vertices().size()==32 && outline.Indices().size()==48,"collapsed outline did not preserve exact seven-quads plus clear");
		// Clear fills100x22; original solid RectRound excludes four half-square
		// corners (2*4*4). Independently compare total triangle area and pixels.
		Check(e,Near(Area(outline),2200+2168),"collapsed outline area differs from source solid rectangle");
		Check(e,Near(outline.Vertices()[4].x,4)&&Near(outline.Vertices()[4].y,0)&&Near(outline.Vertices()[6].x,96)&&Near(outline.Vertices()[6].y,22),"nested Begin changed active quad mode/vertex positions");
		for(unsigned n=4;n<32;++n)Check(e,Near(outline.Vertices()[n].color[3],.37f),"collapsed outline lost continuous outer-color alpha");
		// Exclude clear when testing actual outline-only coverage.
		ScreenDrawList solid;std::vector<std::uint32_t> indices(outline.Indices().begin()+6,outline.Indices().end());for(auto& n:indices)n-=4;
		solid.AppendTriangles(outline.Vertices().subspan(4),outline.TexCoords().subspan(4),indices,outline.Commands()[1].state);
		Check(e,Covered(solid,50.5f,10.5f)&&!Covered(solid,.5f,.5f)&&!Covered(solid,99.5f,21.5f),"collapsed outline independent center/cut-corner pixels differ");
		Check(e,e.recorder.TakeLegacyInvalidOperation() && !e.recorder.TakeLegacyInvalidOperation(),"original nested Begin/extra End error flag missing/not consumable");
		Check(e,OrderScreenRenderTargetPasses(frame).size()==5,"actual helper dependency order invalid");
		e.recorder.RetireCompletedFrame();lua_close(L);
		std::cout<<"NativeUIR2tFlowUI: "<<e.checks<<" checks passed; actual Lua "<<LUA_VERSION<<" number bytes="<<sizeof(lua_Number)<<'\n';return 0;
	} catch(const std::exception& error) {e.recorder.AbortFrame();std::cerr<<error.what()<<'\n';lua_close(L);return 1;}
}
