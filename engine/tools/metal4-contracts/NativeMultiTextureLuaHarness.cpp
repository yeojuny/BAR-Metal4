/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include <iostream>
#include <fstream>
#include <map>
#include <cmath>
#include <string_view>
using namespace rendering;using namespace rendering::ui;
void LuaCreateMutex(lua_State*){}void LuaDestroyMutex(lua_State*){}void LuaLinkMutex(lua_State*,lua_State*){}void LuaMutexLock(lua_State*){}void LuaMutexUnlock(lua_State*){}
int spring_lua_unsynced_rand(lua_State* L){return luaL_error(L,"fixture excludes RNG");}int spring_lua_unsynced_srand(lua_State* L){return luaL_error(L,"fixture excludes RNG");}void spring_lua_ftoa(float,char*,int){throw std::runtime_error("fixture excludes format");}void spring_lua_format(float,const char*,char*){throw std::runtime_error("fixture excludes format");}
unsigned checks=0;void Check(bool x,const char* m){++checks;if(!x)throw std::runtime_error(m);}void Require(bool x,const char* m){if(!x)throw std::runtime_error(m);}
float lua_tofloat(lua_State* L,int n){return lua_tonumber(L,n);}float luaL_checkfloat(lua_State* L,int n){return luaL_checknumber(L,n);}void CheckDrawingEnabled(lua_State*,const char*){}void CondWarnDeprecatedGL(lua_State*,const char*){}
using GLenum=unsigned;constexpr unsigned GL_TEXTURE0=0x84c0,GL_TEXTURE_2D=0xde1;
struct CGlobalRendering{static constexpr int MAX_TEXTURE_UNITS=16;};
struct Reference{unsigned active=0;std::array<std::array<float,4>,2> uv{{{0,0,0,1},{0,0,0,1}}};std::array<bool,2> enabled{};std::array<std::string,2> binding;};Reference ref;
void Coord(unsigned u,float x,float y=0,float z=0,float w=1){if(u<2)ref.uv[u]={x,y,z,w};}
void glTexCoord1f(float x){Coord(0,x);}void glTexCoord2f(float x,float y){Coord(0,x,y);}void glTexCoord3f(float x,float y,float z){Coord(0,x,y,z);}void glTexCoord4f(float x,float y,float z,float w){Coord(0,x,y,z,w);}
void glMultiTexCoord1f(unsigned u,float x){Coord(u-GL_TEXTURE0,x);}void glMultiTexCoord2f(unsigned u,float x,float y){Coord(u-GL_TEXTURE0,x,y);}void glMultiTexCoord3f(unsigned u,float x,float y,float z){Coord(u-GL_TEXTURE0,x,y,z);}void glMultiTexCoord4f(unsigned u,float x,float y,float z,float w){Coord(u-GL_TEXTURE0,x,y,z,w);}
void glActiveTexture(unsigned u){ref.active=u-GL_TEXTURE0;}void glEnable(unsigned){ref.enabled[ref.active]=true;}void glDisable(unsigned){ref.enabled[ref.active]=false;}
struct LuaMatTexture{std::string name;void Enable(bool v){ref.enabled[ref.active]=v;}void Bind(){ref.binding[ref.active]=name;}};struct LuaOpenGLUtils{static bool ParseTextureImage(lua_State*,LuaMatTexture& t,const std::string& n){if(n=="missing")return false;t.name=n;return true;}};
struct LuaOpenGL{static int TexCoord(lua_State*);static int MultiTexCoord(lua_State*);static int Texture(lua_State*);};
NativeUIRecorder recorder;
struct Slot{std::shared_ptr<NativeUITextureBinding> binding;};struct Registry{std::map<std::string,Slot> textures;}registry;
Registry& Textures(lua_State*){return registry;}
std::map<std::string,std::shared_ptr<ScreenTexture>> named;
std::shared_ptr<const ScreenTexture> NamedTexture(const std::string& name,ScreenSampler&){if(name=="missing")return {};auto& p=named[name];if(!p){p=std::make_shared<ScreenTexture>();p->sourceName=name;p->width=p->height=1;p->bytesPerRow=4;p->bytes={16,32,64,128};}return p;}
std::shared_ptr<NativeUITextureBinding> UnitBuildPicture(const std::string&){throw std::runtime_error("fixture excludes unitpic producer");}
std::string LuaPrimitiveOrigin(lua_State*){return "actual-original-callback";}
/*@@NATIVE_VALUES@@*/
int Native(lua_State* L){try{const std::string_view op=lua_tostring(L,lua_upvalueindex(1));const int args=lua_gettop(L);auto& r=recorder;
/*@@NATIVE_OPERATION_0@@*/
/*@@NATIVE_OPERATION_1@@*/
/*@@NATIVE_OPERATION_2@@*/
/*@@NATIVE_OPERATION_3@@*/
throw std::runtime_error("fixture unknown native call");}catch(const std::exception& e){return luaL_error(L,"%s",e.what());}}
std::map<unsigned,std::shared_ptr<const NativeUICommandList>> lists;unsigned nextList=0;
int CreateList(lua_State* L){try{luaL_checktype(L,1,LUA_TFUNCTION);recorder.BeginList();lua_pushvalue(L,1);if(lua_pcall(L,0,0,0)){recorder.AbortList();return lua_error(L);}lists[++nextList]=recorder.EndList();lua_pushnumber(L,nextList);return 1;}catch(const std::exception& e){return luaL_error(L,"%s",e.what());}}
int Blending(lua_State* L){auto s=recorder.CurrentState().draw.blend;s.enabled=lua_toboolean(L,1);recorder.SetBlend(s);return 0;}
void Init(lua_State* L,bool native){luaL_openlibs(L);lua_newtable(L);for(const char* n:{"TexCoord","MultiTexCoord","Texture","Color","TexRect"}){if(native){lua_pushstring(L,n);lua_pushcclosure(L,Native,1);}else{if(std::string_view(n)=="TexCoord")lua_pushcfunction(L,LuaOpenGL::TexCoord);else if(std::string_view(n)=="MultiTexCoord")lua_pushcfunction(L,LuaOpenGL::MultiTexCoord);else if(std::string_view(n)=="Texture")lua_pushcfunction(L,LuaOpenGL::Texture);else{lua_pushnil(L);}}lua_setfield(L,-2,n);}if(native){lua_pushcfunction(L,CreateList);lua_setfield(L,-2,"CreateList");lua_pushcfunction(L,Blending);lua_setfield(L,-2,"Blending");}lua_setglobal(L,"gl");}
bool Run(lua_State* L,const std::string& s){int r=luaL_loadbuffer(L,s.data(),s.size(),"original-call");if(!r)r=lua_pcall(L,0,0,0);if(r)lua_pop(L,1);return r==0;}
void Triangle(){recorder.Begin(ScreenPrimitive::Triangles);recorder.Vertex(1,1);recorder.Vertex(7,1);recorder.Vertex(1,7);recorder.End();}
int main(int argc,char** argv){auto* n=luaL_newstate();auto* o=luaL_newstate();try{Init(n,true);Init(o,false);recorder.BeginFrame(640,480);recorder.BeginScreenStage();
for(const std::string input:{".2","'0.3'",".4,.7","{.6}","{.8,.9}","{.1,nil,.9}","{.2,.4,0}","{.3,.6,0,1}","{.2,.5,0,1,7}"})for(unsigned u=0;u<2;++u){std::string call=u?"gl.MultiTexCoord(1,"+input+")":"gl.TexCoord("+input+")";Check(Run(o,call.c_str()),"Original parser rejected supported input");Check(Run(n,call.c_str()),"Actual native parser rejected common planar input");auto uv=recorder.CurrentState().texCoord;for(unsigned j=0;j<2;++j)for(unsigned k=0;k<2;++k)Check(uv[j*2+k]==ref.uv[j][k],"Actual planar coords differ from original GL command oracle");}
for(const std::string input:{"","{}","true","{false,.2}","{},1",".1,.2,0,1,9"}){Check(!Run(n,"gl.TexCoord("+input+")"),"Native accepted malformed coordinate input");Check(!Run(o,"gl.TexCoord("+input+")"),"Original accepted malformed coordinate input");}
for(const std::string input:{"gl.MultiTexCoord(-1,.2,.3)","gl.MultiTexCoord(math.huge,.2,.3)","gl.Texture(-math.huge,true)","gl.Texture(0/0,true)","gl.MultiTexCoord(2,.2,.3)","gl.MultiTexCoord(1,.2,.3,.4)","gl.TexCoord(.2,.3,0,.5)"})Check(!Run(n,input),"Unsupported unit/projective silently accepted");
for(const std::string call:{"gl.Texture('first')","gl.Texture(1,'second')","gl.Texture(1,false)","gl.Texture(1,true)","gl.Texture(false)","gl.Texture(true)","gl.Texture(1,'missing')","gl.Texture('third')"}){Check(Run(o,call),"Original binding parser failed");Check(Run(n,call),"Actual binding parser failed");Check(ref.active==0,"Original numeric Texture failed active-unit0 restoration");auto s=recorder.CurrentState().draw;Check(bool(s.texture)==ref.enabled[0]&&bool(s.secondary.texture)==ref.enabled[1],"Independent actual enables differ from original");if(s.texture)Check(s.texture->sourceName==ref.binding[0],"Primary name differs from original");if(s.secondary.texture)Check(s.secondary.texture->sourceName==ref.binding[1],"Secondary name differs from original");}
for(int a=1;a<argc;++a){std::ifstream f(argv[a]);std::string code{std::istreambuf_iterator<char>(f),{}};Check(Run(n,code),"Exact original game Initialize callback failed");Check(lists.size()==unsigned(a),"Exact callback did not produce real display list");auto before=recorder.CurrentState();recorder.MultiTexCoord(1,.25f,.75f);recorder.CallList(lists.rbegin()->second);Triangle();auto state=recorder.CurrentState();Check(state.draw.secondary.texture&&state.draw.secondary.texture->sourceName=="LuaUI/images/gradient_alpha_2.png","Exact callback lost declared unit1 name");Check(state.texCoord[2]==.25f&&state.texCoord[3]==.75f,"Original TexRect must not overwrite independent unit1 UV");}
auto frame=recorder.EndFrame();Check(!frame.drawList->Commands().empty(),"Actual original callbacks emitted no geometry");lua_close(n);lua_close(o);std::cout<<"PASS "<<checks<<" original parser/common-state/exact callback checks\n";return 0;}catch(const std::exception& e){recorder.AbortFrame();std::cerr<<"FAIL "<<e.what()<<'\n';lua_close(n);lua_close(o);return 1;}}

/*@@ORIGINAL_PARSERS@@*/
