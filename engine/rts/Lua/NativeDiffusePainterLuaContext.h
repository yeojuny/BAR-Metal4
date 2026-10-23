/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDeferredVisualizerLuaContext.h"
#include "Rendering/Backend/NativeDiffusePainterProgram.h"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
#include <cstring>
#include <algorithm>
namespace LuaNativeDiffusePainter {
using namespace rendering;
using LuaNativeDeferredVisualizer::Require;
using LuaNativeDeferredVisualizer::Bind;
struct Owner;
struct Program {
 std::weak_ptr<Owner> owner;
 NativeDiffusePainterProgram kind=NativeDiffusePainterProgram::Composite;
 NativeDiffusePainterUniforms uniforms;
 std::shared_ptr<const void> compiled;
 bool deleted=false;
};
struct Owner {std::shared_ptr<Program> active;};
inline void SetUniform(const std::shared_ptr<Owner>& owner,int location,sol::variadic_args values,bool integer){
 // OpenGL ignores a -1 location for uniforms inactive in this program.
 if(location==-1)return;
 Require(owner->active&&!owner->active->deleted&&location>=1&&unsigned(location)<=NativeDiffusePainterUniformFields.size(),"Diffuse Painter uniform requires its active original paint program");
 const auto& field=NativeDiffusePainterUniformFields[location-1];
 Require((field.programs&(1u<<unsigned(owner->active->kind)))&&field.integer==integer&&field.components==values.size(),"Diffuse Painter uniform type/dimension changed");
 auto* out=reinterpret_cast<std::byte*>(&owner->active->uniforms)+(location-1)*16;
 for(unsigned i=0;i<field.components;++i){
  if(integer){const std::int32_t v=values[i].as<std::int32_t>();std::memcpy(out+i*4,&v,4);}
  else {const float v=values[i].as<float>();Require(std::isfinite(v),"Diffuse Painter uniform must be finite");std::memcpy(out+i*4,&v,4);}
 }
}
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Diffuse Painter must be unsynced");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 context.new_usertype<Program>("DiffusePainterProgram",sol::no_constructor);context.set("DiffusePainterProgram",sol::lua_nil);
 Bind(context,"CreateShader",[owner](const sol::table& source){
  unsigned count=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt","Diffuse Painter unknown shader field");++count;}Require(count==3,"Diffuse Painter original program field missing");
  Require(targetPriorityProgram::Hash(source["vertex"].get<std::string>())==NativeDiffuseVertexHash,"Diffuse Painter original vertex source changed");
  const auto hash=targetPriorityProgram::Hash(source["fragment"].get<std::string>());auto p=std::make_shared<Program>();p->owner=owner;
  const auto found=std::find(NativeDiffuseFragmentHashes.begin(),NativeDiffuseFragmentHashes.end(),hash);
  Require(found!=NativeDiffuseFragmentHashes.end(),"Diffuse Painter original injected fragment source changed");
  p->kind=NativeDiffusePainterProgram(unsigned(found-NativeDiffuseFragmentHashes.begin()));
  const std::array<std::array<std::string_view,4>,4> samplers{{
   {"srcTex","maskTex","heightMap","layerTex"},{"srcMask","layerTex","normalTex","roughTex"},
   {"dstTex","layerTex","normalTex","roughTex"},{"tex0","","",""}}};
  const unsigned needed=p->kind==NativeDiffusePainterProgram::Copy?1:4;count=0;
  for(const auto& entry:source["uniformInt"].get<sol::table>()){
   const auto key=entry.first.as<std::string>();const int value=entry.second.as<int>();
   Require(value>=0&&unsigned(value)<needed&&key==samplers[unsigned(p->kind)][value],"Diffuse Painter sampler mapping changed");++count;
  }Require(count==needed,"Diffuse Painter sampler missing");
  p->compiled=NativeDiffusePainterOwner().lock();Require(bool(p->compiled),"Diffuse Painter actual compiled consumer missing");return p;
 });
 Bind(context,"GetUniformLocation",[owner](const std::shared_ptr<Program>& p,const std::string& name){
  Require(p&&p->owner.lock()==owner&&!p->deleted,"Diffuse Painter deleted/foreign program");
  for(unsigned i=0;i<NativeDiffusePainterUniformFields.size();++i){const auto& field=NativeDiffusePainterUniformFields[i];
   if(name==field.name&&(field.programs&(1u<<unsigned(p->kind))))return int(i+1);
  }
  return -1;
 });
 Bind(context,"UseShader",[owner](sol::object value){
  if(value.is<int>()){Require(value.as<int>()==0,"Diffuse Painter has no numeric program names");owner->active.reset();return;}
  auto p=value.as<std::shared_ptr<Program>>();Require(p&&p->owner.lock()==owner&&!p->deleted&&p->compiled,"Diffuse Painter active program missing");owner->active=std::move(p);
 });
 Bind(context,"DeleteShader",[owner](const std::shared_ptr<Program>& p){Require(p&&p->owner.lock()==owner,"Diffuse Painter foreign program");if(owner->active==p)owner->active.reset();p->deleted=true;p->compiled.reset();});
 Bind(context,"Uniform",[owner](int location,sol::variadic_args values){SetUniform(owner,location,values,false);});
 Bind(context,"UniformInt",[owner](int location,sol::variadic_args values){SetUniform(owner,location,values,true);});
 Bind(context,"DrawTriangles",[owner](const sol::table& rows){
  Require(owner->active&&!owner->active->deleted&&rows.size()==6,"Diffuse Painter original triangle stream missing");
  auto input=std::make_shared<NativeDiffusePainterInput>();input->program=owner->active->kind;input->uniforms=owner->active->uniforms;input->compiled=owner->active->compiled;
  auto& recorder=ui::GetRecorder();Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"Diffuse Painter actual image owner missing");
  for(unsigned slot=0;slot<(input->program==NativeDiffusePainterProgram::Copy?1u:4u);++slot){const auto bound=recorder.TextureObserver()->BoundImage(slot);input->images[slot]={bound.image,ScreenUVMode::Normalized,bound.sampler};}
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;vertices.reserve(rows.size());uv.reserve(rows.size());
  for(unsigned i=1;i<=rows.size();++i){const auto row=rows.get<sol::table>(i);Require(row.size()==5,"Diffuse Painter original vertex/UV changed");std::array<float,5> v{};for(unsigned j=0;j<5;++j){v[j]=row.get<float>(j+1);Require(std::isfinite(v[j]),"Diffuse Painter nonfinite vertex");}vertices.push_back({v[0],v[1],v[2],{1,1,1,1}});uv.push_back({v[3],v[4],0,0});}
  recorder.AppendDiffusePainter(std::move(input),std::move(vertices),std::move(uv));
 });
 return context;
}
} // namespace LuaNativeDiffusePainter
