/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDeferredVisualizerLuaContext.h"
#include "Rendering/Backend/NativeDecalCaptureProgram.h"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
namespace LuaNativeDecalCapture {
using namespace rendering;
using LuaNativeDeferredVisualizer::Require;
using LuaNativeDeferredVisualizer::Bind;
struct Owner;
struct Program {
 std::weak_ptr<Owner> owner;
 std::shared_ptr<const void> compiled;
 NativeDecalCaptureUniforms uniforms;
 bool deleted=false;
};
struct Owner {std::shared_ptr<Program> active;};
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Decal Capture must be unsynced");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 context.new_usertype<Program>("DecalCaptureProgram",sol::no_constructor);context.set("DecalCaptureProgram",sol::lua_nil);
 Bind(context,"CreateShader",[owner](const sol::table& source){
  unsigned count=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","Decal Capture unknown shader field");++count;}Require(count==4,"Decal Capture missing original program field");
  Require(targetPriorityProgram::Hash(source["vertex"].get<std::string>())==17707545575107498927ull && targetPriorityProgram::Hash(source["fragment"].get<std::string>())==13236906638078821457ull,"Decal Capture original shader source changed");
  const auto integers=source["uniformInt"].get<sol::table>();count=0;
  for(const auto& entry:integers){const auto key=entry.first.as<std::string>();const auto value=entry.second.as<int>();Require((key=="tex0"&&value==0)||(key=="heightMap"&&value==1),"Decal Capture sampler mapping changed");++count;}Require(count==2,"Decal Capture sampler missing");
  const auto floats=source["uniformFloat"].get<sol::table>();count=0;
  for(const auto& entry:floats){const auto key=entry.first.as<std::string>();++count;
   if(key=="featherStart"){Require(entry.second.as<float>()==.85f,"Decal Capture feather default changed");continue;}
   Require(key=="worldOrigin"||key=="worldExtent"||key=="mapSize","Decal Capture unknown original uniform");
   const auto values=entry.second.as<sol::table>();const float value=key=="worldOrigin"?0.f:1.f;
   Require(values.size()==2&&values.get<float>(1)==value&&values.get<float>(2)==value,"Decal Capture original vector default changed");
  }Require(count==4,"Decal Capture uniform defaults missing");
  auto program=std::make_shared<Program>();program->owner=owner;program->compiled=NativeDecalCaptureOwner().lock();Require(bool(program->compiled),"Decal Capture actual compiled pipeline missing");return program;
 });
 Bind(context,"GetUniformLocation",[owner](const std::shared_ptr<Program>& p,const std::string& name){
  Require(p&&p->owner.lock()==owner&&!p->deleted,"Decal Capture deleted/foreign program");
  const std::array<std::string_view,5> names{"viewProj","worldOrigin","worldExtent","mapSize","featherStart"};
  for(unsigned i=0;i<names.size();++i)if(name==names[i])return int(i+1);
  throw std::runtime_error("Decal Capture unknown original uniform");
 });
 Bind(context,"UseShader",[owner](sol::object value){
  if(value.is<int>()){Require(value.as<int>()==0,"Decal Capture has no numeric shader names");owner->active.reset();return;}
  auto p=value.as<std::shared_ptr<Program>>();Require(p&&p->owner.lock()==owner&&!p->deleted&&p->compiled,"Decal Capture active program missing");owner->active=std::move(p);
 });
 Bind(context,"DeleteShader",[owner](const std::shared_ptr<Program>& p){Require(p&&p->owner.lock()==owner,"Decal Capture foreign program");if(owner->active==p)owner->active.reset();p->deleted=true;p->compiled.reset();});
 Bind(context,"Uniform",[owner](int location,sol::variadic_args values){
  Require(owner->active&&!owner->active->deleted&&location>=2&&location<=5,"Decal Capture uniform requires its active original program");
  Require(values.size()==(location==5?1u:2u),"Decal Capture uniform dimension changed");
  auto& u=owner->active->uniforms;
  for(unsigned i=0;i<values.size();++i){const float v=values[i].as<float>();Require(std::isfinite(v),"Decal Capture uniform must be finite");
   if(location==2)u.originExtent[i]=v;else if(location==3)u.originExtent[2+i]=v;else if(location==4)u.mapFeather[i]=v;else u.mapFeather[2]=v;
  }
 });
 Bind(context,"UniformMatrix",[owner](int location,sol::variadic_args values){
  Require(owner->active&&!owner->active->deleted&&location==1&&values.size()==16,"Decal Capture original view projection missing");
  for(unsigned i=0;i<16;++i){const float v=values[i].as<float>();Require(std::isfinite(v),"Decal Capture matrix must be finite");owner->active->uniforms.viewProjection[i]=v;}
 });
 Bind(context,"DrawQuad",[owner](const sol::table& rows){
  Require(owner->active&&!owner->active->deleted&&rows.size()==4,"Decal Capture active original quad missing");
  auto input=std::make_shared<NativeDecalCaptureInput>();input->compiled=owner->active->compiled;input->uniforms=owner->active->uniforms;
  auto& recorder=ui::GetRecorder();Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"Decal Capture actual texture owner missing");
  for(unsigned unit=0;unit<2;++unit){const auto bound=recorder.TextureObserver()->BoundImage(unit);input->images[unit]={bound.image,ScreenUVMode::Normalized,bound.sampler};}
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;vertices.reserve(4);uv.reserve(4);
  for(unsigned i=1;i<=4;++i){const auto row=rows.get<sol::table>(i);Require(row.size()==5,"Decal Capture original vertex/UV dimension changed");std::array<float,5> v{};for(unsigned j=0;j<5;++j){v[j]=row.get<float>(j+1);Require(std::isfinite(v[j]),"Decal Capture quad must be finite");}vertices.push_back({v[0],v[1],v[2],{1,1,1,1}});uv.push_back({v[3],v[4],0,0});}
  recorder.AppendDecalCapture(std::move(input),std::move(vertices),std::move(uv));
 });
 return context;
}
} // namespace LuaNativeDecalCapture
