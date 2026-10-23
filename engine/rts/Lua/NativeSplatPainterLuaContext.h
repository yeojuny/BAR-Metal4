/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeDeferredVisualizerLuaContext.h"
#include "Rendering/Backend/NativeSplatPainterProgram.h"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
#include <cstring>
namespace LuaNativeSplatPainter {
using namespace rendering;
using LuaNativeDeferredVisualizer::Require;
using LuaNativeDeferredVisualizer::Bind;
struct Owner;
struct Program {
 std::weak_ptr<Owner> owner;
 NativeSplatPainterProgram kind=NativeSplatPainterProgram::Paint;
 NativeSplatPainterUniforms uniforms;
 std::shared_ptr<const void> compiled;
 bool deleted=false;
};
struct Owner {std::shared_ptr<Program> active;};
inline void SetUniform(const std::shared_ptr<Owner>& owner,int location,sol::variadic_args values,bool integer){
 // OpenGL ignores a -1 location: texSize is declared but unused originally.
 if(location==-1)return;
 Require(owner->active&&!owner->active->deleted&&owner->active->kind==NativeSplatPainterProgram::Paint&&location>=1&&unsigned(location)<=NativeSplatPainterUniformFields.size(),"Splat Painter uniform requires its active original paint program");
 const auto& field=NativeSplatPainterUniformFields[location-1];
 Require(field.integer==integer&&field.components==values.size(),"Splat Painter uniform type/dimension changed");
 auto* out=reinterpret_cast<std::byte*>(&owner->active->uniforms)+(location-1)*16;
 for(unsigned i=0;i<field.components;++i){
  if(integer){const std::int32_t v=values[i].as<std::int32_t>();std::memcpy(out+i*4,&v,4);}
  else {const float v=values[i].as<float>();Require(std::isfinite(v),"Splat Painter uniform must be finite");std::memcpy(out+i*4,&v,4);}
 }
}
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Splat Painter must be unsynced");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 context.new_usertype<Program>("SplatPainterProgram",sol::no_constructor);context.set("SplatPainterProgram",sol::lua_nil);
 Bind(context,"CreateShader",[owner](const sol::table& source){
  unsigned count=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt","Splat Painter unknown shader field");++count;}Require(count==3,"Splat Painter original program field missing");
  Require(targetPriorityProgram::Hash(source["vertex"].get<std::string>())==NativeSplat_PAINT_VERT_SRC,"Splat Painter original vertex source changed");
  const auto hash=targetPriorityProgram::Hash(source["fragment"].get<std::string>());auto p=std::make_shared<Program>();p->owner=owner;
  if(hash==NativeSplat_PAINT_FRAG_SRC)p->kind=NativeSplatPainterProgram::Paint;
  else if(hash==NativeSplat_COPY_FRAG_SRC)p->kind=NativeSplatPainterProgram::Copy;
  else {Require(hash==NativeSplat_OVERLAY_FRAG_SRC,"Splat Painter original fragment source changed");p->kind=NativeSplatPainterProgram::Overlay;}
  count=0;for(const auto& entry:source["uniformInt"].get<sol::table>()){
   const auto key=entry.first.as<std::string>();const int value=entry.second.as<int>();
   Require((key=="tex0"&&value==0)||(p->kind==NativeSplatPainterProgram::Paint&&key=="heightMap"&&value==1),"Splat Painter sampler mapping changed");++count;
  }Require(count==(p->kind==NativeSplatPainterProgram::Paint?2u:1u),"Splat Painter sampler missing");
  p->compiled=p->kind==NativeSplatPainterProgram::Overlay?NativeSplatOverlayOwner().lock():NativeSplatPainterOwner().lock();
  Require(bool(p->compiled),"Splat Painter actual compiled consumer missing");return p;
 });
 Bind(context,"GetUniformLocation",[owner](const std::shared_ptr<Program>& p,const std::string& name){
  Require(p&&p->owner.lock()==owner&&!p->deleted,"Splat Painter deleted/foreign program");
  if(p->kind!=NativeSplatPainterProgram::Paint||name=="texSize")return -1;
  for(unsigned i=0;i<NativeSplatPainterUniformFields.size();++i)if(name==NativeSplatPainterUniformFields[i].name)return int(i+1);
  return -1;
 });
 Bind(context,"UseShader",[owner](sol::object value){
  if(value.is<int>()){Require(value.as<int>()==0,"Splat Painter has no numeric program names");owner->active.reset();return;}
  auto p=value.as<std::shared_ptr<Program>>();Require(p&&p->owner.lock()==owner&&!p->deleted&&p->compiled,"Splat Painter active program missing");owner->active=std::move(p);
 });
 Bind(context,"DeleteShader",[owner](const std::shared_ptr<Program>& p){Require(p&&p->owner.lock()==owner,"Splat Painter foreign program");if(owner->active==p)owner->active.reset();p->deleted=true;p->compiled.reset();});
 Bind(context,"Uniform",[owner](int location,sol::variadic_args values){SetUniform(owner,location,values,false);});
 Bind(context,"UniformInt",[owner](int location,sol::variadic_args values){SetUniform(owner,location,values,true);});
 Bind(context,"DrawTriangles",[owner](const sol::table& rows){
  Require(owner->active&&!owner->active->deleted&&rows.size()>0&&rows.size()<=4*1024*1024&&rows.size()%3==0,"Splat Painter original triangle stream missing");
  auto input=std::make_shared<NativeSplatPainterInput>();input->program=owner->active->kind;input->uniforms=owner->active->uniforms;input->compiled=owner->active->compiled;
  auto& recorder=ui::GetRecorder();Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"Splat Painter actual image owner missing");
  for(unsigned slot=0;slot<(input->program==NativeSplatPainterProgram::Paint?2u:1u);++slot){const auto bound=recorder.TextureObserver()->BoundImage(slot);input->images[slot]={bound.image,ScreenUVMode::Normalized,bound.sampler};}
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;vertices.reserve(rows.size());uv.reserve(rows.size());
  for(unsigned i=1;i<=rows.size();++i){const auto row=rows.get<sol::table>(i);Require(row.size()==5,"Splat Painter original vertex/UV changed");std::array<float,5> v{};for(unsigned j=0;j<5;++j){v[j]=row.get<float>(j+1);Require(std::isfinite(v[j]),"Splat Painter nonfinite vertex");}vertices.push_back({v[0],v[1],v[2],{1,1,1,1}});uv.push_back({v[3],v[4],0,0});}
  recorder.AppendSplatPainter(std::move(input),std::move(vertices),std::move(uv));
 });
 return context;
}
} // namespace LuaNativeSplatPainter
