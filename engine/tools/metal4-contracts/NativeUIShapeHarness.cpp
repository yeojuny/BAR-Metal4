/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Generated sections are mechanically supplied by run_native_shape_test.py.
// The reference executes the pinned upstream parser and captures GL commands;
// it does not call the native parser or native matrix/triangulation helpers.
#include "Rendering/Backend/NativeUIRecorder.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

void LuaCreateMutex(lua_State*) {}
void LuaDestroyMutex(lua_State*) {}
void LuaLinkMutex(lua_State*,lua_State*) {}
void LuaMutexLock(lua_State*) {}
void LuaMutexUnlock(lua_State*) {}
int spring_lua_unsynced_rand(lua_State* L) { return luaL_error(L,"fixture excludes RNG"); }
int spring_lua_unsynced_srand(lua_State* L) { return luaL_error(L,"fixture excludes RNG"); }
void spring_lua_ftoa(float,char*,int) { throw std::runtime_error("fixture excludes formatting"); }
void spring_lua_format(float,const char*,char*) { throw std::runtime_error("fixture excludes formatting"); }

using namespace rendering;
using namespace rendering::ui;
using GLuint=unsigned;
enum { GL_POINTS=0,GL_LINES=1,GL_LINE_LOOP=2,GL_LINE_STRIP=3,GL_TRIANGLES=4,GL_TRIANGLE_STRIP=5,GL_TRIANGLE_FAN=6,GL_QUADS=7,GL_QUAD_STRIP=8,GL_POLYGON=9 };
constexpr int GL_MODELVIEW=0x1700,GL_PROJECTION=0x1701,GL_TEXTURE=0x1702;
constexpr unsigned GL_CURRENT_BIT=1,GL_COLOR_BUFFER_BIT=0x4000,GL_SCISSOR_BIT=0x80000,GL_TEXTURE_BIT=0x40000,GL_ENABLE_BIT=0x2000,GL_ALL_ATTRIB_BITS=0xfffff;
constexpr unsigned GL_DEPTH_BUFFER_BIT=0x100,GL_POLYGON_BIT=8,GL_STENCIL_BUFFER_BIT=0x400,GL_LINE_BIT=4,GL_POINT_BIT=2;
constexpr int L_ERROR=1,L_WARNING=2;
#define LOG_L(level,...) do { (void)(level); } while(false)
static bool lua_israwstring(lua_State* L,int n) { return lua_type(L,n)==LUA_TSTRING; }
static float lua_tofloat(lua_State* L,int n)
{
	const float value=lua_tonumber(L,n);
	if (!std::isfinite(value)) luaL_argerror(L,n,"non-finite original REPORT_LUANAN value");
	return value;
}
static int PosAbsLuaIndex(lua_State* L,int n) { return n<0 && n>LUA_REGISTRYINDEX?lua_gettop(L)+n+1:n; }
static float luaL_checkfloat(lua_State* L,int n) {return float(luaL_checknumber(L,n));}
struct LuaUtils { static int ParseFloatArray(lua_State*,int,float*,int); };
/*@@ORIGINAL_FLOAT_ARRAY@@*/

namespace {
unsigned checks=0;
void Check(bool yes,const std::string& why) { ++checks;if(!yes)throw std::runtime_error(why); }
void Require(bool yes,const char* why) { if(!yes)throw std::runtime_error(why); }
bool Near(float a,float b) { return std::abs(a-b)<=0.0002f; }
using Matrix=std::array<float,16>;
Matrix Unit() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
Matrix Ortho(float l,float r,float b,float t,float n,float f)
{
	return {2/(r-l),0,0,0,0,2/(t-b),0,0,0,0,-2/(f-n),0,-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1};
}
Matrix Product(const Matrix& a,const Matrix& b)
{
	Matrix c{};for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)for(unsigned k=0;k<4;++k)c[col*4+row]+=a[k*4+row]*b[col*4+k];return c;
}
struct Vertex { std::array<float,3> position;ScreenColor color;ScreenTexCoord uv;std::array<float,3> normal; };
struct Fan { int mode;std::string target;std::vector<Vertex> vertices; };
struct Reference {
	ScreenColor color{1,1,1,1};ScreenTexCoord uv{0,0};std::array<float,3> normal{0,0,1};
	std::array<std::vector<Matrix>,3> matrices;unsigned matrixMode=0,width=256,height=192;
	struct Current {ScreenColor color;ScreenTexCoord uv;std::array<float,3> normal;};std::vector<Current> attrs;
	std::vector<Fan> fans;std::optional<Fan> active;std::string target;
	std::vector<std::function<void()>> commands;bool compiling=false,replaying=false;
	std::map<unsigned,std::vector<std::function<void()>>> lists;unsigned lastList=0,lastTexture=0;
	std::map<std::string,std::pair<unsigned,unsigned>> textures;
	void Reset() { *this=Reference{};matrices={{{Unit()},{Ortho(0,256,0,192,-1,1)},{Unit()}}}; }
};
struct Lists {
	unsigned last=0;std::unordered_map<unsigned,std::shared_ptr<const NativeUICommandList>> values;
	unsigned NewNativeList(std::shared_ptr<const NativeUICommandList> list) { values[++last]=std::move(list);return last; }
	std::shared_ptr<const NativeUICommandList> GetNativeList(unsigned n) { const auto x=values.find(n);return x==values.end()?nullptr:x->second; }
};
struct Environment {
	Reference ref;bool reference=false,drawing=true;Lists lists;
	struct Texture {std::shared_ptr<NativeUITextureBinding> binding;bool fbo=true;};
	std::unordered_map<std::string,Texture> textures;unsigned lastTexture=0;
} env;
void Command(std::function<void()> command)
{
	if(env.ref.compiling&&!env.ref.replaying)env.ref.commands.push_back(std::move(command));else command();
}
void glBegin(GLuint mode) { Command([mode] {if(!env.ref.active)env.ref.active=Fan{int(mode),env.ref.target,{}};}); }
void glEnd() { Command([] {if(env.ref.active){env.ref.fans.push_back(std::move(*env.ref.active));env.ref.active.reset();}}); }
void glColor4fv(const float* p) {ScreenColor a{p[0],p[1],p[2],p[3]};Command([a]{env.ref.color=a;});}
void glTexCoord2fv(const float* p) {ScreenTexCoord a{p[0],p[1]};Command([a]{env.ref.uv=a;});}
void glNormal3fv(const float* p) {std::array<float,3> a{p[0],p[1],p[2]};Command([a]{env.ref.normal=a;});}
void glNormal3f(float x,float y,float z) {const float a[]{x,y,z};glNormal3fv(a);}
void glVertex3fv(const float* p)
{
	const std::array<float,4> v{p[0],p[1],p[2],1};Command([v] {
		if(!env.ref.active)return;
		const auto m=Product(env.ref.matrices[1].back(),env.ref.matrices[0].back());std::array<float,4> clip{};
		for(unsigned row=0;row<4;++row)for(unsigned k=0;k<4;++k)clip[row]+=m[k*4+row]*v[k];
		env.ref.active->vertices.push_back({{(clip[0]/clip[3]+1)*env.ref.width*.5f,(clip[1]/clip[3]+1)*env.ref.height*.5f,clip[2]/clip[3]},env.ref.color,env.ref.uv,env.ref.normal});
	});
}
struct LuaOpenGL {
	static int Shape(lua_State* L);
	static int Normal(lua_State* L);
	static bool IsDrawingEnabled(lua_State*) {return env.drawing;}
	static void SetDrawingEnabled(lua_State*,bool value) {env.drawing=value;}
};
void CheckDrawingEnabled(lua_State* L,const char*) {if(!env.drawing)luaL_error(L,"drawing disabled");}
void CondWarnDeprecatedGL(lua_State*,const char*) {}
/*@@ORIGINAL_VERTEX_DATA@@*/
/*@@ORIGINAL_SHAPE@@*/
/*@@ORIGINAL_NORMAL@@*/

struct CLuaHandle { static Lists& GetActiveDisplayLists(lua_State*) {return env.lists;} };
Environment& Textures(lua_State*) {return env;}
std::string LuaPrimitiveOrigin(lua_State*) {return "independent Shape fixture";}
/*@@NATIVE_HELPERS@@*/

int NativeDispatchImpl(lua_State* L,std::string_view op)
{
	auto& r=GetRecorder();const int args=lua_gettop(L);
	/*@@NATIVE_CREATE_LIST@@*/
	Require(LuaOpenGL::IsDrawingEnabled(L),"drawing disabled");
	/*@@NATIVE_OPERATIONS@@*/
	if(op=="CreateTexture") {
		auto identity=std::make_shared<ScreenRenderTargetIdentity>();identity->resourceId=++env.lastTexture;
		identity->width=luaL_checkint(L,1);identity->height=luaL_checkint(L,2);
		auto image=std::make_shared<ScreenTexture>();image->renderTarget=identity;image->width=identity->width;image->height=identity->height;image->sourceName="!"+std::to_string(env.lastTexture);
		auto cell=std::make_shared<NativeUITextureBinding>();cell->current=image;env.textures[image->sourceName]={cell};lua_pushstring(L,image->sourceName.c_str());return 1;
	}
	throw std::runtime_error("unimplemented CPU fixture operation");
}
/*@@NATIVE_DISPATCH@@*/

int ReferenceDispatch(lua_State* L,const std::string& op)
{
	auto& r=env.ref;const int args=lua_gettop(L);
	if(op=="Shape")return LuaOpenGL::Shape(L);
	if(op=="Color") {
		ScreenColor color{1,1,1,1};for(int k=0;k<4;++k)color[k]=luaL_optnumber(L,k+1,color[k]);glColor4fv(color.data());return 0;
	}
	if(op=="TexCoord") {const float uv[]{float(luaL_checknumber(L,1)),float(luaL_checknumber(L,2))};glTexCoord2fv(uv);return 0;}
	if(op=="Normal") return LuaOpenGL::Normal(L);
	if(op=="Vertex") {const float v[]{float(luaL_checknumber(L,1)),float(luaL_checknumber(L,2)),float(luaL_optnumber(L,3,0))};glVertex3fv(v);return 0;}
	if(op=="BeginEnd") {glBegin(luaL_checkint(L,1));const int error=lua_pcall(L,args-2,0,0);glEnd();if(error)return lua_error(L);return 0;}
	if(op=="Translate"||op=="Scale") {
		Matrix m=Unit();for(unsigned k=0;k<3;++k)m[(op=="Scale")?k*5:12+k]=luaL_checknumber(L,k+1);
		Command([m]{auto& matrix=env.ref.matrices[env.ref.matrixMode].back();matrix=Product(matrix,m);});return 0;
	}
	if(op=="MatrixMode") {const int mode=luaL_checkint(L,1);Command([mode]{env.ref.matrixMode=mode==GL_MODELVIEW?0:mode==GL_PROJECTION?1:2;});return 0;}
	if(op=="LoadIdentity") {Command([]{env.ref.matrices[env.ref.matrixMode].back()=Unit();});return 0;}
	if(op=="Ortho") {
		const auto m=Ortho(luaL_checknumber(L,1),luaL_checknumber(L,2),luaL_checknumber(L,3),luaL_checknumber(L,4),luaL_checknumber(L,5),luaL_checknumber(L,6));
		Command([m]{auto& matrix=env.ref.matrices[env.ref.matrixMode].back();matrix=Product(matrix,m);});return 0;
	}
	if(op=="PushMatrix") {Command([]{auto& m=env.ref.matrices[env.ref.matrixMode];m.push_back(m.back());});return 0;}
	if(op=="PopMatrix") {Command([]{env.ref.matrices[env.ref.matrixMode].pop_back();});return 0;}
	if(op=="PushAttrib") {Command([]{env.ref.attrs.push_back({env.ref.color,env.ref.uv,env.ref.normal});});return 0;}
	if(op=="PopAttrib") {Command([]{const auto a=env.ref.attrs.back();env.ref.attrs.pop_back();env.ref.color=a.color;env.ref.uv=a.uv;env.ref.normal=a.normal;});return 0;}
	if(op=="CreateList") {
		Check(!r.compiling,"reference test excludes nested list compilation");r.compiling=true;r.commands.clear();
		const int error=lua_pcall(L,args-1,0,0);r.compiling=false;
		if(error){r.commands.clear();lua_pushnumber(L,0);return 1;}
		r.lists[++r.lastList]=std::move(r.commands);lua_pushnumber(L,r.lastList);return 1;
	}
	if(op=="CallList") {for(const auto& c:r.lists.at(luaL_checkint(L,1)))c();return 0;}
	if(op=="CreateTexture") {const auto name="!"+std::to_string(++r.lastTexture);r.textures[name]={unsigned(luaL_checkint(L,1)),unsigned(luaL_checkint(L,2))};lua_pushstring(L,name.c_str());return 1;}
	if(op=="RenderToTexture") {
		const std::string name=luaL_checkstring(L,1);const auto oldTarget=r.target;const auto oldWidth=r.width,oldHeight=r.height;
		const auto oldM=r.matrices[0],oldP=r.matrices[1];r.matrices[0].push_back(Unit());r.matrices[1].push_back(Unit());r.matrixMode=0;r.target=name;
		const auto extent=r.textures.at(name);r.width=extent.first;r.height=extent.second;const int error=lua_pcall(L,args-2,0,0);
		r.matrices[0]=oldM;r.matrices[1]=oldP;r.matrixMode=0;r.target=oldTarget;r.width=oldWidth;r.height=oldHeight;if(error)return lua_error(L);return 0;
	}
	return luaL_error(L,"unsupported reference fixture operation %s",op.c_str());
}
int Routing(lua_State* L)
{
	if(!env.reference)return Dispatch(L);
	try {return ReferenceDispatch(L,lua_tostring(L,lua_upvalueindex(1)));}
	catch(const std::exception& e){return luaL_error(L,"reference fixture %s",e.what());}
}
void Run(lua_State* L,const std::string& script)
{
	if(luaL_loadstring(L,script.c_str())||lua_pcall(L,0,0,0)) {
		const char* message=lua_tostring(L,-1);
		const std::string why=message?message:std::string("Lua non-string error object: ")+lua_typename(L,lua_type(L,-1));
		lua_pop(L,1);throw std::runtime_error(why);
	}
}
void Initialize(lua_State* L)
{
	luaL_openlibs(L);lua_newtable(L);lua_setglobal(L,"widget");lua_newtable(L);lua_setglobal(L,"WG");lua_newtable(L);
	for(const char* op:{"Shape","Normal","Color","TexCoord","Vertex","CreateList","CallList","CreateTexture","RenderToTexture","Translate","Scale","MatrixMode","LoadIdentity","Ortho","PushMatrix","PopMatrix","PushAttrib","PopAttrib","BeginEnd"}) {
		lua_pushstring(L,op);lua_pushcclosure(L,Routing,1);lua_setfield(L,-2,op);
	}
	lua_setglobal(L,"gl");lua_newtable(L);
	for(const auto& [name,value]:std::initializer_list<std::pair<const char*,int>>{{"TRIANGLES",4},{"TRIANGLE_STRIP",5},{"TRIANGLE_FAN",6},{"QUADS",7},{"CURRENT_BIT",1},{"MODELVIEW",GL_MODELVIEW},{"PROJECTION",GL_PROJECTION}}){lua_pushnumber(L,value);lua_setfield(L,-2,name);}
	lua_setglobal(L,"GL");Run(L,"Spring={GetConfigFloat=function(_,v)return v end,GetViewGeometry=function()return 256,192 end}");
}
struct Geometry {std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<unsigned> indices;std::vector<ScreenDrawCommand> commands;};
Geometry Copy(const ScreenDrawList& s) {return {{s.Vertices().begin(),s.Vertices().end()},{s.TexCoords().begin(),s.TexCoords().end()},{s.Indices().begin(),s.Indices().end()},{s.Commands().begin(),s.Commands().end()}};}
struct Snapshot {std::map<std::string,Geometry> geometry;std::vector<Fan> fans;ScreenColor color;ScreenTexCoord uv;std::array<float,3> normal;};
Snapshot Scenario(lua_State* L,const std::string& script,bool reference)
{
	auto& r=GetRecorder();r.AbortFrame();env.reference=reference;env.drawing=true;env.lists={};env.textures.clear();env.lastTexture=0;env.ref.Reset();
	if(!reference){r.BeginFrame(256,192);r.BeginScreenStage();}
	Run(L,script);Snapshot result;
	if(reference){Check(!env.ref.active,"reference Shape remained in Begin");result.fans=env.ref.fans;result.color=env.ref.color;result.uv=env.ref.uv;result.normal=env.ref.normal;}
	else {const auto state=r.CurrentState();result.color=state.color;result.uv=state.texCoord;result.normal=r.CurrentNormal();auto frame=r.EndFrame();result.geometry[""]=Copy(*frame.drawList);for(const auto& t:frame.targetPasses)result.geometry[t.destination->sourceName]=Copy(*t.drawList);r.RetireCompletedFrame();}
	return result;
}
std::vector<unsigned> Triangles(const Fan& f)
{
	std::vector<unsigned> out;const auto n=unsigned(f.vertices.size());auto tri=[&](unsigned a,unsigned b,unsigned c){out.insert(out.end(),{a,b,c});};
	if(f.mode==GL_TRIANGLE_FAN)for(unsigned k=2;k<n;++k)tri(0,k-1,k);
	else if(f.mode==GL_TRIANGLES)for(unsigned k=2;k<n;k+=3)tri(k-2,k-1,k);
	else if(f.mode==GL_QUADS)for(unsigned k=3;k<n;k+=4){tri(k-3,k-2,k-1);tri(k-3,k-1,k);}
	else if(f.mode==GL_TRIANGLE_STRIP)for(unsigned k=2;k<n;++k){if(k%2)tri(k-1,k-2,k);else tri(k-2,k-1,k);}
	else throw std::runtime_error("reference expansion only compares implemented native modes");return out;
}
void Compare(const Snapshot& reference,const Snapshot& native,const std::string& label)
{
	for(unsigned k=0;k<4;++k)Check(Near(reference.color[k],native.color[k]),label+" current color differs");
	for(unsigned k=0;k<2;++k)Check(Near(reference.uv[k],native.uv[k]),label+" current UV differs");
	for(unsigned k=0;k<3;++k)Check(Near(reference.normal[k],native.normal[k]),label+" current normal differs");
	std::map<std::string,std::vector<Vertex>> expanded;
	for(const auto& fan:reference.fans)for(const auto index:Triangles(fan))expanded[fan.target].push_back(fan.vertices[index]);
	for(const auto& [target,g]:native.geometry){const auto& expected=expanded[target];Check(g.indices.size()==expected.size(),label+" triangle index count differs");for(std::size_t k=0;k<expected.size();++k){const auto& a=expected[k];const auto& b=g.vertices.at(g.indices[k]);const auto uv=g.uv.at(g.indices[k]);for(unsigned c=0;c<3;++c)Check(Near(a.position[c],c==0?b.x:c==1?b.y:b.z),label+" reference/native triangle position differs");for(unsigned c=0;c<4;++c)Check(Near(a.color[c],b.color[c]),label+" reference/native triangle color differs");for(unsigned c=0;c<2;++c)Check(Near(a.uv[c],uv[c]),label+" reference/native triangle UV differs");}expanded.erase(target);}
	for(const auto& [name,data]:expanded)Check(data.empty(),label+" missing native target "+name);
}
void JsonArray(std::ostream& o,const auto& a) {o<<'[';bool first=true;for(auto n:a){if(!first)o<<',';o<<n;first=false;}o<<']';}
void JsonVertex(std::ostream& o,const Vertex& v) {o<<"{\"position\":";JsonArray(o,v.position);o<<",\"color\":";JsonArray(o,v.color);o<<",\"uv\":";JsonArray(o,v.uv);o<<",\"normal\":";JsonArray(o,v.normal);o<<'}';}
unsigned GLFactor(ScreenBlendFactor f)
{
	switch(f){case ScreenBlendFactor::Zero:return 0;case ScreenBlendFactor::One:return 1;case ScreenBlendFactor::SourceAlpha:return 0x302;case ScreenBlendFactor::OneMinusSourceAlpha:return 0x303;default:throw std::runtime_error("oracle unexpected blend factor");}
}
void OracleCase(std::ostream& o,const std::string& label,float progress,bool flip,const Snapshot& a,const Snapshot& b)
{
	o<<"{\"label\":\""<<label<<"\",\"progress\":"<<progress<<",\"outerFlipX\":"<<(flip?"true":"false")<<",\"referenceFans\":[";bool first=true;
	for(const auto& fan:a.fans){if(!first)o<<',';first=false;o<<"{\"primitive\":"<<fan.mode<<",\"vertices\":[";for(std::size_t n=0;n<fan.vertices.size();++n){if(n)o<<',';JsonVertex(o,fan.vertices[n]);}o<<"]}";}
	const auto& g=b.geometry.at("");o<<"],\"native\":{\"vertices\":[";for(std::size_t n=0;n<g.vertices.size();++n){if(n)o<<',';const auto& v=g.vertices[n];JsonVertex(o,{{v.x,v.y,v.z},v.color,g.uv[n],{0,0,1}});}o<<"],\"indices\":";JsonArray(o,g.indices);o<<",\"commands\":[";
	for(std::size_t n=0;n<g.commands.size();++n){if(n)o<<',';const auto& c=g.commands[n];const auto& s=c.state.blend;o<<"{\"firstIndex\":"<<c.firstIndex<<",\"indexCount\":"<<c.indexCount<<",\"blend\":{\"enabled\":"<<(s.enabled?"true":"false")<<",\"sourceColor\":"<<GLFactor(s.sourceColor)<<",\"destinationColor\":"<<GLFactor(s.destinationColor)<<",\"sourceAlpha\":"<<GLFactor(s.sourceAlpha)<<",\"destinationAlpha\":"<<GLFactor(s.destinationAlpha)<<"}}";}o<<"]}}";
}
}

int main(int argc,char** argv)
{
	if(argc!=3){std::cerr<<"usage: shape-test <original gui_flowui.lua> <flowui-oracle.json>\n";return 2;}
	auto* L=luaL_newstate();
	try {
		Initialize(L);if(luaL_loadfile(L,argv[1])||lua_pcall(L,0,0,0))throw std::runtime_error(lua_tostring(L,-1));
		const std::vector<std::pair<std::string,std::string>> cases={
			{"aliases-3D", "gl.Shape(4,{{vertex={1,2,.25},color={.2,.3,.4,.5},texcoord={.25,.75},normal={2,3,4}},{v={5,6,-.25}},{v={8,9,0}}})"},
			{"short-color-empty", "gl.Color(.2,.3,.4,.5);gl.Shape(4,{{v={1,2},c={}},{v={4,5},c={.6}},{v={7,8},c={.7,.8}}})"},
			{"short-color-three-stop", "gl.Shape(4,{{v={1,2},c={.1,.2,.3}},{v={4,5},c={.4,false,.9}},{v={7,8},c={false}}})"},
			{"numeric-strings-extra", "gl.Shape(4,{{v={'1','2','0',false},c={'.1','.2','.3','.4',false},t={'.2','.8',false},n={'4','5','6',false}},{v={3,4,'oops'}},{v={5,6,nil}}})"},
			{"inherited-state", "gl.Color(.2,.3,.4,.5);gl.TexCoord(.7,.8);gl.Normal(3,4,5);gl.Shape(4,{{v={1,2}},{v={3,4},c={.8,.7,.6,.5}},{v={5,6},n={0,0,0}}});gl.Shape(4,{{v={7,8}},{v={8,9}},{v={9,10}}})"},
			{"standalone-normal-table", "gl.Normal({'3','4','5',false});gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})"},
			{"unknown-table-key", "gl.Shape(4,{{v={1,2},ignored={notUsed=true}},{v={3,4}},{v={5,6}}})"},
			{"gap-terminates", "gl.Shape(4,{[1]={v={1,2}},[2]={v={3,4}},[3]={v={5,6}},[5]=false,metadata='ignored'})"},
			{"duplicate-aliases", "gl.Shape(4,{{v={1,2},vertex={7,8},c={.1},color={.9},t={.1,.2},texcoord={.8,.9},n={1,2,3},normal={4,5,6}},{v={3,4}},{v={5,6}}})"},
			{"triangle-strip", "gl.Shape(5,{{v={1,2}},{v={1,8}},{v={8,2}},{v={8,8}},{v={12,8}}})"},
			{"quads", "gl.Shape(7,{{v={1,2}},{v={8,2}},{v={8,9}},{v={1,9}}})"},
			{"nested-original-invalid-operation", "gl.BeginEnd(7,function()gl.Vertex(0,0);gl.Shape(6,{{v={4,0}},{v={4,4}},{v={0,4}}});gl.Vertex(99,99)end)"},
			{"numeric-string-mode", "gl.Shape('4',{{v={1,2}},{v={3,4}},{v={5,6}}})"},
			{"ignored-extra-arguments", "gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}},'unused',false)"},
			{"raw-metadata-no-metamethod", "gl.Shape(4,setmetatable({},{__index=function()error('must not call __index')end}))"},
			{"empty-and-short", "gl.Shape(6,{});gl.Shape(6,{{v={1,2},n={3,4,5}}});gl.Shape(6,{{v={1,2}},{v={2,3}}})"},
			{"matrix-and-current-attrib", "gl.Normal(3,4,5);gl.PushAttrib(1);gl.PushMatrix();gl.Translate(10,20,0);gl.Scale(-2,3,1);gl.Shape(4,{{v={1,2},n={6,7,8},c={.2,.3,.4,.5}},{v={3,4}},{v={5,6}}});gl.PopMatrix();gl.PopAttrib();gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})"},
			{"display-list-relative-state", "gl.Normal(3,4,5);local h=gl.CreateList(function()gl.PushMatrix();gl.Translate(1,2,0);gl.Shape(4,{{v={1,2},n={6,7,8}},{v={3,4}},{v={5,6}}});gl.PopMatrix()end);gl.Translate(10,20,0);gl.CallList(h);gl.Translate(20,-3,0);gl.CallList(h)"},
			{"display-list-compilation-no-state-change", "gl.Normal(3,4,5);gl.CreateList(function()gl.Shape(4,{{v={1,2},n={6,7,8}},{v={3,4}},{v={5,6}}})end)"},
			{"RTT-matrix-restore-current-persist", "gl.Translate(10,20,0);local t=gl.CreateTexture(32,16);gl.RenderToTexture(t,function()gl.Ortho(0,32,0,16,-1,1);gl.Shape(4,{{v={1,2},n={6,7,8},c={.2,.3,.4,.5}},{v={3,4}},{v={5,6}}})end);gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})"},
		};
		for(const auto& [label,code]:cases){const auto a=Scenario(L,code,true),b=Scenario(L,code,false);Compare(a,b,label);}
		const std::vector<std::string> invalid={"gl.Shape(4,false)","gl.Shape({}, {})","gl.Shape(4,{false})","gl.Shape(4,{{v={1}}})","gl.Shape(4,{{v={1,false}}})","gl.Shape(4,{{c={1,1,1}}})","gl.Shape(4,{{v={1,2},n={1,2}}})","gl.Shape(4,{{v={1,2},t={1}}})","gl.Shape(4,{{v={1,2},unknown=false}})","gl.Shape(4,{{v={1,2},[1]={}}})"};
		for(const auto& code:invalid){for(bool ref:{true,false}){bool failed=false;try{Scenario(L,code,ref);}catch(const std::exception&){failed=true;}Check(failed,"invalid Shape accepted: "+code);GetRecorder().AbortFrame();}}
		for(int mode:{8,9}){bool failed=false;try{Scenario(L,"gl.Shape("+std::to_string(mode)+",{})",false);}catch(const std::exception&){failed=true;}Check(failed,"unsupported native primitive silently succeeded");}
		const auto recovery=Scenario(L,"local ok=pcall(gl.Shape,4,{{v={1,2},n={2,3,4}},{v={1}}});assert(not ok);gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})",false);Check(recovery.geometry.at("").indices.size()==3,"failed Shape left poisoned primitive");
		const auto failedList=Scenario(L,"local h=gl.CreateList(function()gl.Shape(4,{{v={1,2}},{v={1}}})end);assert(h==0);gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})",false);Check(failedList.geometry.at("").indices.size()==3,"failed Shape published partial list");
		const auto invalidNormal=Scenario(L,"gl.Normal(3,4,5);for _,n in ipairs({0/0,1/0,-1/0}) do local ok=pcall(gl.Normal,n,0,1);assert(not ok)end;gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})",false);Check(invalidNormal.normal==std::array<float,3>{3,4,5},"invalid nonfinite normal changed current state");
		const auto abortedRTT=Scenario(L,"gl.Normal(3,4,5);local t=gl.CreateTexture(32,16);local ok=pcall(gl.RenderToTexture,t,function()gl.Normal(6,7,8);error('expected RTT failure')end);assert(not ok);gl.Shape(4,{{v={1,2}},{v={3,4}},{v={5,6}}})",false);Check(abortedRTT.normal==std::array<float,3>{3,4,5}&&abortedRTT.geometry.size()==1,"aborted target leaked normal/partial target");
		std::ofstream json(argv[2]);json<<std::setprecision(9)<<"{\"schemaVersion\":1,\"width\":256,\"height\":192,\"clearRGBA\":[0.08,0.12,0.18,0.4],\"cases\":[";bool first=true;
		for(float progress:{0.f,.0001f,.125f,.25f,.375f,.5f,.625f,.75f,.875f,1.f}) {
		for(bool flip:{false,true}){
			const auto label="progress-"+std::to_string(progress)+(flip?"-outerFlipX":"-ordinary");
			const std::string code="gl.TexCoord(.25,.75);"+std::string(flip?"gl.Translate(256,0,0);gl.Scale(-1,1,1);":"")+"WG.FlowUI.Draw.RectRoundProgress(32,24,224,168,8,"+std::to_string(progress)+",{.2,.7,.4,.65})";
			const auto a=Scenario(L,code,true),b=Scenario(L,code,false);Compare(a,b,label);Check(a.fans.size()==1&&a.fans[0].mode==GL_TRIANGLE_FAN,"original FlowUI callback changed primitive");if(!first)json<<',';first=false;OracleCase(json,label,progress,flip,a,b);
		}
		}
		json<<"]}\n";json.close();
		GetRecorder().AbortFrame();lua_close(L);std::cout<<"NativeShape17: "<<checks<<" checks passed; "<<cases.size()<<" contract cases + invalid/recovery/list/RTT +20 exact original FlowUI cases\n";return 0;
	}catch(const std::exception& error){GetRecorder().AbortFrame();std::cerr<<error.what()<<'\n';lua_close(L);return 1;}
}
