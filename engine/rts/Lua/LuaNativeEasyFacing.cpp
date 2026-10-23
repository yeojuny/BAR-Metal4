/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeEasyFacing.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "Rendering/GL/myGL.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeEasyFacingProgram.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <vector>
namespace LuaNativeEasyFacing {namespace {
using namespace rendering;
void Check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program {NativeEasyFacingInput uniforms;bool deleted=false;};
struct Context {std::shared_ptr<Program> active;bool collecting=false;std::vector<std::array<float,4>> vertices;};
int Location(std::string_view name){constexpr std::array<std::string_view,9> names{"worldScale","cornerRadius","outlineWidth","chevronDepth","chevronWidth","chevronBaseGap","alphaMultiplier","fillColor","outlineColor"};auto i=std::find(names.begin(),names.end(),name);return i==names.end()?-1:int(i-names.begin());}
void Uniform(Program& p,int location,const std::vector<float>& values){
 if(location==-1)return;Check(location>=0&&location<9&&values.size()==(location>=7?4u:1u),"Original Easy Facing uniform shape differs");
 for(float v:values)Check(std::isfinite(v),"Original Easy Facing uniform is not finite");
 if(location<4)p.uniforms.shape[location]=values[0];else if(location<7)p.uniforms.detail[location-4]=values[0];else std::copy(values.begin(),values.end(),(location==7?p.uniforms.fill:p.uniforms.outline).begin());
}
void Active(const std::shared_ptr<Program>& p){Check(p&&!p->deleted&&p->uniforms.compiled&&p->uniforms.compiled==NativeEasyFacingConsumer(),"Original Easy Facing shader is unavailable/deleted");}
sol::table CreateContext(lua_State* L){
 Check(!CLuaHandle::GetHandleSynced(L)&&bool(NativeEasyFacingConsumer()),"Original Easy Facing compiled consumer missing");sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();
 Bind(table,"CreateShader",[](sol::table source){
  for(const auto& item:source){const auto name=item.first.as<std::string>();Check(name=="vertex"||name=="fragment"||name=="uniformFloat","Original Easy Facing shader option differs");}
  Check(EasyFacingHash(source["vertex"].get<std::string>())==easyfacing::VertexHash&&EasyFacingHash(source["fragment"].get<std::string>())==easyfacing::FragmentHash,"Original Easy Facing shader source differs");
  auto p=std::make_shared<Program>();p->uniforms.compiled=NativeEasyFacingConsumer();
  auto values=source["uniformFloat"].get<sol::table>();for(const auto& item:values){std::vector<float> v;if(item.second.is<sol::table>()){auto t=item.second.as<sol::table>();for(unsigned n=1;n<=t.size();++n)v.push_back(t[n].get<float>());}else v.push_back(item.second.as<float>());Uniform(*p,Location(item.first.as<std::string>()),v);}ValidateNativeEasyFacing(p->uniforms);return p;
 });
 Bind(table,"UseShader",[context](sol::object value){Check(!context->collecting,"Easy Facing program changed inside BeginEnd");if(value.is<int>()&&value.as<int>()==0){context->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Active(p);context->active=p;return true;});
 Bind(table,"DeleteShader",[context](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(context->active==p)context->active.reset();return true;});
 Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Active(p);return Location(name);});
 Bind(table,"Uniform",[context](int location,sol::variadic_args args){Active(context->active);Check(!context->collecting,"Easy Facing uniform changed inside BeginEnd");std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());Uniform(*context->active,location,v);});
 Bind(table,"Vertex",[context](float x,float y,float z){Check(context->collecting&&context->vertices.size()<4,"Original Easy Facing vertex outside original quad");context->vertices.push_back({x,y,z,1});});
 Bind(table,"BeginEnd",[context](GLenum mode,sol::protected_function function){
  Active(context->active);Check(mode==GL_QUADS&&!context->collecting,"Original Easy Facing primitive differs");context->vertices.clear();context->collecting=true;
  auto result=function();context->collecting=false;if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}
  Check(context->vertices.size()==4,"Original Easy Facing quad is incomplete");ui::GetRecorder().AppendEasyFacing(context->vertices,std::make_shared<const NativeEasyFacingInput>(context->active->uniforms));
 });return table;
}
}
bool PushEntries(lua_State* L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();family.new_usertype<Program>("Program",sol::no_constructor);family.set("Program",sol::lua_nil);
 Bind(family,"Available",[](){return bool(NativeEasyFacingConsumer());});Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](){Check(!CLuaHandle::GetHandleSynced(L),"Easy Facing source is unsynced-only");CFileHandler file("LuaUI/Widgets/gui_easyfacing.lua",SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&EasyFacingHash(source)==easyfacing::WidgetHash,"Original Easy Facing widget identity differs");return source;});
 Bind(family,"PrepareSource",[](std::string source){Check(EasyFacingHash(source)==easyfacing::WidgetHash,"Original Easy Facing preprocessing requires pinned source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl["NativeEasyFacing"]=family;return true;
}
}
#else
bool LuaNativeEasyFacing::PushEntries(lua_State*){return false;}
#endif
