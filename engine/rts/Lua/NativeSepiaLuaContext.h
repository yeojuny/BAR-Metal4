/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaNativeUI.h"
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
 auto type=context.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Sepia buffer was deleted");b.value->Define(count,layout);});
 Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values){Require(!b.deleted,"Sepia buffer was deleted");return b.value->Upload(values,sol::nullopt,sol::nullopt,sol::nullopt,sol::nullopt);});
 Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("Buffer",sol::lua_nil);
 Bind(context,"GetVBO",[L](GLenum target,bool frequent){Require(!frequent && (target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER),"Sepia original placeholder buffer target/usage changed");auto b=std::make_shared<Buffer>();b->target=target;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
 Bind(context,"ValidateProgram",[](const sol::table& params,const std::string& name){
  Require(IsNativeSepiaConsumerAvailable() && name==": Sepia","Unknown Sepia paired program");
  unsigned count=0;for(const auto& item:params){const auto key=item.first.as<std::string>();Require(key=="vertex" || key=="fragment" || key=="uniformInt","Sepia has an unexpected shader option/stage");++count;}Require(count==3,"Sepia program source is incomplete");
  Require(casProgram::Hash(params["vertex"].get<std::string>())==sepiaProgram::VertexHash,"Sepia original vertex source changed");
  auto samplers=params["uniformInt"].get<sol::table>();count=0;for(const auto& item:samplers){Require(item.first.as<std::string>()=="screenCopyTex" && item.second.as<int>()==0,"Sepia original sampler changed");++count;}Require(count==1,"Sepia sampler declaration missing");
  Require(casProgram::Hash(params["fragment"].get<std::string>())==sepiaProgram::FragmentHash,"Sepia original fragment source changed");return true;
 });
 Bind(context,"Draw",[L,&recorder](const sol::table& values,const std::string& texture,std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> index,std::shared_ptr<Buffer> instance){
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
  NativeSepiaUniforms u;u.engine=OriginalCUSUniformCapture().Current();
  auto raster=std::make_shared<NativeCASRaster>();raster->surface=state;raster->world=ui.CurrentWorldState();u.raster=raster;
  u.gamma=values["gamma"].get<float>();
  auto pos=values["viewPos"].get<sol::table>();Require(pos.size()==2,"Sepia viewPos differs");for(unsigned i=0;i<2;++i)u.viewPos[i]=pos[i+1].get<std::int32_t>();
  for(unsigned i=0;i<3;++i){auto row=values[std::array{"colorR","colorG","colorB"}[i]].get<sol::table>();Require(row.size()==3,"Sepia color row differs");for(unsigned j=0;j<3;++j)u.rows[i][j]=row[j+1].get<float>();}
  Require(globalRendering,"Sepia needs actual view geometry");
  NativePostprocessRect viewport{globalRendering->viewPosX,globalRendering->viewPosY,std::uint32_t(globalRendering->viewSizeX),std::uint32_t(globalRendering->viewSizeY)};
  if(state.viewport){const auto& v=*state.viewport;viewport={v.x,v.y,v.width,v.height};}
  if(raster->surface.scissor && !raster->surface.scissor->windowCoordinates){raster->surface.scissor->x+=globalRendering->viewPosX;raster->surface.scissor->y+=globalRendering->viewPosY;raster->surface.scissor->windowCoordinates=true;}
  recorder.DrawSepia(observed.image->postprocessVersion,std::move(u),viewport,ui.RootCommandCount());
 });
 return context;
}
}
