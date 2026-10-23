/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeDeferredVisualizerProgram.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
namespace LuaNativeDeferredVisualizer {
using namespace rendering;
inline void Require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program{std::shared_ptr<const void> compiled;bool deleted=false;};
struct Owner{std::shared_ptr<Program> active;};
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Deferred visualizer must be unsynced");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 context.new_usertype<Program>("DeferredVisualizerProgram",sol::no_constructor);context.set("DeferredVisualizerProgram",sol::lua_nil);
 Bind(context,"CreateShader",[](const sol::table& source){
  unsigned count=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt","Deferred visualizer unexpected shader field");++count;}Require(count==3,"Deferred visualizer shader fields missing");
  Require(HashNativeDeferredVisualizer(source["vertex"].get<std::string>())==NativeDeferredVisualizerVS&&HashNativeDeferredVisualizer(source["fragment"].get<std::string>())==NativeDeferredVisualizerFS,"Deferred visualizer original shader source changed");
  const std::array<std::string,13> names{"map_gbuffer_normtex","map_gbuffer_difftex","map_gbuffer_spectex","map_gbuffer_emittex","map_gbuffer_misctex","map_gbuffer_zvaltex","model_gbuffer_normtex","model_gbuffer_difftex","model_gbuffer_spectex","model_gbuffer_emittex","model_gbuffer_misctex","model_gbuffer_zvaltex","depthcopy"};
  const auto samplers=source["uniformInt"].get<sol::table>();count=0;for(const auto& entry:samplers){const auto unit=entry.second.as<int>();Require(unit>=0&&unit<13&&entry.first.as<std::string>()==names[unit],"Deferred visualizer original sampler mapping differs");++count;}Require(count==13,"Deferred visualizer sampler missing");
  auto program=std::make_shared<Program>();program->compiled=NativeDeferredVisualizerOwner().lock();Require(bool(program->compiled),"Deferred visualizer actual compiled pipeline missing");return program;
 });
 Bind(context,"GetUniformLocation",[](const std::shared_ptr<Program>& program,const std::string& name){Require(program&&!program->deleted&&name=="debugDraw","Deferred visualizer unexpected uniform");return -1;}); // Original unused uniform is inactive.
 Bind(context,"DeleteShader",[owner](const std::shared_ptr<Program>& program){Require(bool(program),"Deferred visualizer program missing");if(owner->active==program)owner->active.reset();program->deleted=true;program->compiled.reset();});
 Bind(context,"UseShader",[owner](sol::object value){if(value.is<int>()){Require(value.as<int>()==0,"Deferred visualizer has no numeric shader names");owner->active.reset();return;}auto program=value.as<std::shared_ptr<Program>>();Require(program&&!program->deleted&&program->compiled,"Deferred visualizer program deleted");owner->active=std::move(program);});
 Bind(context,"TexRect",[owner](float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1){
  Require(owner->active&&!owner->active->deleted&&x0==0&&y0==-1&&x1==1&&y1==1&&s0==.5f&&t0==0&&s1==1&&t1==1,"Deferred visualizer original TexRect differs");
  auto input=std::make_shared<NativeDeferredVisualizerInput>();input->compiled=owner->active->compiled;auto& recorder=ui::GetRecorder();
  Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"Deferred visualizer texture observer missing");
  for(unsigned unit=0;unit<13;++unit){const auto bound=recorder.TextureObserver()->BoundImage(unit);input->images[unit]={bound.image,ScreenUVMode::Normalized,bound.sampler};}
  ValidateNativeDeferredVisualizer(*input);recorder.AppendDeferredVisualizer(std::move(input));
 });
 return context;
}
} // namespace LuaNativeDeferredVisualizer
