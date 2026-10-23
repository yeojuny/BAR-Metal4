/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaNativeUI.h"
#include "LuaVBOImpl.h"
#include "Rendering/Backend/NativeCASProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include <cmath>
#include <cstring>
namespace LuaNativeCAS {
using namespace rendering;
inline void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer {std::shared_ptr<LuaVBOImpl> value;bool deleted=false;GLenum target;};
inline sol::table Create(lua_State* L,NativePostprocessRecorder& recorder)
{
 Require(!CLuaHandle::GetHandleSynced(L) && IsNativeCASConsumerAvailable(),"CAS requires the real compiled native consumer");
 sol::state_view lua(L);auto context=lua.create_table();
 auto type=context.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"CAS buffer was deleted");b.value->Define(count,layout);});
 Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values){Require(!b.deleted,"CAS buffer was deleted");return b.value->Upload(values,sol::nullopt,sol::nullopt,sol::nullopt,sol::nullopt);});
 Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("Buffer",sol::lua_nil);
 Bind(context,"GetVBO",[L](GLenum target,bool frequent){Require(!frequent && (target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER),"CAS original placeholder buffer target/usage changed");auto b=std::make_shared<Buffer>();b->target=target;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
 Bind(context,"ValidateProgram",[](const sol::table& params,const std::string& name){
  Require(IsNativeCASConsumerAvailable() && name=="Contrast Adaptive Sharpen","Unknown CAS paired program");
  unsigned count=0;for(const auto& item:params){const auto key=item.first.as<std::string>();Require(key=="vertex" || key=="fragment" || key=="uniformInt","CAS has an unexpected shader option/stage");++count;}Require(count==3,"CAS program source is incomplete");
  Require(casProgram::Hash(params["vertex"].get<std::string>())==casProgram::VertexHash,"CAS original vertex source changed");
  auto samplers=params["uniformInt"].get<sol::table>();count=0;for(const auto& item:samplers){Require(item.first.as<std::string>()=="screenCopyTex" && item.second.as<int>()==0,"CAS original sampler changed");++count;}Require(count==1,"CAS sampler declaration missing");
  const auto hash=casProgram::Hash(params["fragment"].get<std::string>());const auto found=std::find(casProgram::FragmentHashes.begin(),casProgram::FragmentHashes.end(),hash);
  Require(found!=casProgram::FragmentHashes.end(),"CAS exact original fragment/defines/engine declarations changed");return unsigned(found-casProgram::FragmentHashes.begin());
 });
 Bind(context,"Draw",[L,&recorder](unsigned variant,const sol::table& values,const std::string& texture,std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> index,std::shared_ptr<Buffer> instance){
  Require(IsNativeCASConsumerAvailable() && variant<8,"CAS native consumer/program missing");
  auto& ui=rendering::ui::GetRecorder();Require(ui.IsScreenStage() && !ui.IsWorldStage() && !ui.IsCompiling() && !ui.HasActiveTarget() && !ui.HasActiveTypedTarget(),"CAS requires its original root screen stage");
  // These real original VAO buffers are lifetime-checked although the pinned
  // vertex shader reads only gl_VertexID, never any of their attributes.
  Require(vertex && index && instance && !vertex->deleted && !index->deleted && !instance->deleted && vertex->target==GL_ARRAY_BUFFER && index->target==GL_ELEMENT_ARRAY_BUFFER && instance->target==GL_ARRAY_BUFFER,"CAS original VAO placeholders missing/deleted");
  const auto a=vertex->value->CaptureNativeSnapshot(),b=index->value->CaptureNativeSnapshot(),c=instance->value->CaptureNativeSnapshot();
  Require(a && b && c && a->layout.elements==3 && b->layout.elements==3 && c->layout.elements==1,"CAS original VAO buffer extents changed");
  const auto state=ui.CurrentState().draw;
  // Read the sampler's actual shared binding after the public Texture call.
  // A zero-unit request can inherit another active unit; never substitute the
  // name requested by Lua for the image the shader really samples at unit0.
  auto& shared=OriginalNativeTextureContext();Require(ui.TextureObserver()==&shared.UI(),"CAS actual sampler observer missing");
  const auto observed=shared.UI().BoundImage(0);
  Require(observed.image && !observed.image->incomplete && observed.image->postprocessVersion,"CAS actual sampler0 has no produced screen-copy version");
  NativeCASUniforms u;u.variant=variant;u.engine=OriginalCUSUniformCapture().Current();
  auto raster=std::make_shared<NativeCASRaster>();raster->surface=state;raster->world=ui.CurrentWorldState();u.raster=raster;
  u.strength=values["strength"].get<float>();u.groundY=values["groundY"].get<float>();u.debugFootprint=values["debugFootprint"].get<float>();
  auto rect=values["viewRect"].get<sol::table>();Require(rect.size()==4,"CAS viewRect changed");for(unsigned i=0;i<4;++i)u.viewRect[i]=rect[i+1].get<std::int32_t>();
  Require(globalRendering,"CAS needs actual view geometry");
  NativePostprocessRect viewport{globalRendering->viewPosX,globalRendering->viewPosY,std::uint32_t(globalRendering->viewSizeX),std::uint32_t(globalRendering->viewSizeY)};
  if(state.viewport){const auto& v=*state.viewport;viewport={v.x,v.y,v.width,v.height};}
  if(raster->surface.scissor && !raster->surface.scissor->windowCoordinates){raster->surface.scissor->x+=globalRendering->viewPosX;raster->surface.scissor->y+=globalRendering->viewPosY;raster->surface.scissor->windowCoordinates=true;}
  recorder.DrawCAS(observed.image->postprocessVersion,std::move(u),viewport,ui.RootCommandCount());
 });
 return context;
}
}
