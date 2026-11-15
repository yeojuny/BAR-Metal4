/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaNativeUI.h"
#include "Rendering/Backend/NativeSepiaPilot.h"
#include <cstdlib>
#include "LuaVBOImpl.h"
#include "Rendering/Backend/NativeSepiaProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include <cmath>
#include <cstring>
namespace LuaNativeSepia {
using namespace rendering;
inline void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer {std::shared_ptr<LuaVBOImpl> value;bool deleted=false;GLenum target;};
inline sol::table Create(lua_State* L,NativePostprocessRecorder& recorder)
{
 Require(!CLuaHandle::GetHandleSynced(L) && IsNativeSepiaConsumerAvailable(),"Sepia requires the real compiled native consumer");
 sol::state_view lua(L);auto context=lua.create_table();
 const char* selected=std::getenv("BAR_METAL4_SEPIA_PATH");
 Require(!selected || std::strcmp(selected,"old")==0 || std::strcmp(selected,"shared")==0,"BAR_METAL4_SEPIA_PATH must be old or shared");
 const bool sharedPath=selected && std::strcmp(selected,"shared")==0;
 context.set("SharedPath",sharedPath);
 context.new_usertype<sepiaPilot::Program>("LinkedProgram",sol::no_constructor);context.set("LinkedProgram",sol::lua_nil);
 auto type=context.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Sepia buffer was deleted");b.value->Define(count,layout);});
 Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values){Require(!b.deleted,"Sepia buffer was deleted");return b.value->Upload(values,sol::nullopt,sol::nullopt,sol::nullopt,sol::nullopt);});
 Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("Buffer",sol::lua_nil);
 Bind(context,"GetVBO",[L](GLenum target,bool frequent){Require(!frequent && (target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER),"Sepia original placeholder buffer target/usage changed");auto b=std::make_shared<Buffer>();b->target=target;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
 auto validate=[sharedPath](const sol::table& params,const std::string& name){
  Require(name==": Sepia","Original Sepia program name changed");
  Require(IsNativeSepiaConsumerAvailable(),"Sepia native linked consumer unavailable");
  unsigned count=0;for(const auto& item:params){const auto key=item.first.as<std::string>();Require(key=="vertex" || key=="fragment" || key=="uniformInt","Unsupported generated shader option/stage");++count;}Require(count==3,"Generated linked program source is incomplete");
  const auto vertex=params["vertex"].get<std::string>(),fragment=params["fragment"].get<std::string>();
  if(sharedPath)Require(vertex==sepiaGenerated::VertexSource && fragment==sepiaGenerated::FragmentSource,"GLSL differs from offline generated linked program; regenerate first");
  else Require(casProgram::Hash(vertex)==sepiaProgram::VertexHash && casProgram::Hash(fragment)==sepiaProgram::FragmentHash,"Old Sepia comparator requires unchanged original GLSL");
  const auto expected=sepiaGenerated::Link();auto samplers=params["uniformInt"].get<sol::table>();count=0;
  for(const auto& item:samplers){Require(expected->resources.size()==1 && item.first.as<std::string>()==expected->resources[0].name && item.second.as<int>()==0,"Generated linked logical sampler differs");++count;}
  Require(count==1,"Generated linked sampler declaration missing");return true;
 };
 Bind(context,"ValidateProgram",validate);
 Bind(context,"LinkProgram",[validate,sharedPath](const sol::table& params,const std::string& name){validate(params,name);return sepiaPilot::Link(sharedPath);});
 Bind(context,"SetUniform",[](std::shared_ptr<sepiaPilot::Program> p,const std::string& name,const sol::table& values,bool integer){
  Require(bool(p),"Linked uniform program missing");std::vector<double> typed;typed.reserve(values.size());for(size_t i=1;i<=values.size();++i)typed.push_back(values[i].get<double>());sepiaPilot::SetUniform(*p,name,typed,integer);
 });
 Bind(context,"DeleteProgram",[](std::shared_ptr<sepiaPilot::Program> p){Require(bool(p),"Linked program missing");p->deleted=true;});
 Bind(context,"Draw",[L,&recorder](std::shared_ptr<sepiaPilot::Program> program,const std::string& texture,std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> index,std::shared_ptr<Buffer> instance){
  Require(IsNativeSepiaConsumerAvailable(),"Sepia native consumer/program missing");
  auto& ui=rendering::ui::GetRecorder();Require(ui.IsScreenStage() && !ui.IsWorldStage() && !ui.IsCompiling() && !ui.HasActiveTarget() && !ui.HasActiveTypedTarget(),"Sepia requires its original root screen stage");
  // These real original VAO buffers are lifetime-checked although the pinned
  // vertex shader reads only gl_VertexID, never any of their attributes.
  Require(vertex && index && instance && !vertex->deleted && !index->deleted && !instance->deleted && vertex->target==GL_ARRAY_BUFFER && index->target==GL_ELEMENT_ARRAY_BUFFER && instance->target==GL_ARRAY_BUFFER,"Sepia original VAO placeholders missing/deleted");
  const auto a=vertex->value->CaptureNativeSnapshot(),b=index->value->CaptureNativeSnapshot(),c=instance->value->CaptureNativeSnapshot();
  Require(a && b && c && a->layout.elements==3 && b->layout.elements==3 && c->layout.elements==1,"Sepia original VAO buffer extents changed");
  const auto state=ui.CurrentState().draw;
  // Read the sampler's actual shared binding after the public Texture call.
  // A zero-unit request can inherit another active unit; never substitute the
  // name requested by Lua for the image the shader really samples at unit0.
  auto& shared=OriginalNativeTextureContext();Require(ui.TextureObserver()==&shared.UI(),"Sepia actual sampler observer missing");
  const auto observed=shared.UI().BoundImage(0);
  Require(observed.image && !observed.image->incomplete && observed.image->postprocessVersion,"Sepia actual sampler0 has no produced screen-copy version");
  Require(bool(program),"Linked draw program missing");sepiaPilot::Live(*program);
  NativeSepiaUniforms u;u.sharedPath=program->shared;u.engine=OriginalCUSUniformCapture().Current();
  auto raster=std::make_shared<NativeCASRaster>();raster->surface=state;raster->world=ui.CurrentWorldState();u.raster=raster;
  auto material=std::make_shared<sharedpilot::MaterialContent>();material->program=program->interface;material->uniformBytes=program->uniforms;
  sharedpilot::LogicalBinding binding;binding.logicalSlot=0;binding.versionOwner=observed.image->postprocessVersion;material->bindings.push_back(std::move(binding));u.material=std::move(material);
  // The old comparator alone keeps its original handwritten marshaling.
  if(!u.sharedPath){
   const auto& bytes=*program->uniforms;
   for(const auto& field:program->interface->fields){
    if(field.name=="gamma")std::memcpy(&u.gamma,bytes.data()+field.offset,4);
    else if(field.name=="viewPos")std::memcpy(u.viewPos.data(),bytes.data()+field.offset,8);
    else for(unsigned row=0;row<3;++row)if(field.name==std::array{"colorR","colorG","colorB"}[row])std::memcpy(u.rows[row].data(),bytes.data()+field.offset,12);
   }
  }
  Require(globalRendering,"Sepia needs actual view geometry");
  NativePostprocessRect viewport{globalRendering->viewPosX,globalRendering->viewPosY,std::uint32_t(globalRendering->viewSizeX),std::uint32_t(globalRendering->viewSizeY)};
  if(state.viewport){const auto& v=*state.viewport;viewport={v.x,v.y,v.width,v.height};}
  if(raster->surface.scissor && !raster->surface.scissor->windowCoordinates){raster->surface.scissor->x+=globalRendering->viewPosX;raster->surface.scissor->y+=globalRendering->viewPosY;raster->surface.scissor->windowCoordinates=true;}
  recorder.DrawSepia(observed.image->postprocessVersion,std::move(u),viewport,ui.RootCommandCount());
 });
 return context;
}
}
