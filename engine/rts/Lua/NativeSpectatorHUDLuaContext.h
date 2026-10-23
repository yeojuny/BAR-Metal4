/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeSpectatorHUDProgram.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <cstring>
#include <stdexcept>
#include <limits>

namespace LuaNativeSpectatorHUD {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner { bool validated=false,active=false; };
struct Buffer {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;};

inline sol::table Create(lua_State* L)
{
 Require(!CLuaHandle::GetHandleSynced(L)&&targetPriorityProgram::Available(),"Spectator HUD requires its actual compiled screen program");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 auto type=context.new_usertype<Buffer>("SpectatorHUDBuffer",sol::no_constructor);
 Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Spectator HUD buffer deleted");b.value->Define(count,layout);});
 Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"Spectator HUD buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
 Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("SpectatorHUDBuffer",sol::lua_nil);
 Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER,"Spectator HUD original buffer target differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
 Bind(context,"ValidateProgram",[owner](const sol::table& params,const std::string& name,const std::string& engine){
  Require(targetPriorityProgram::Available()&&name=="spectator_hud","Unknown original Spectator HUD program");
  unsigned count=0;for(const auto& entry:params){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment","Spectator HUD unexpected program option/stage");++count;}Require(count==2,"Spectator HUD program source incomplete");
  Require(targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"Spectator HUD engine definitions changed");
  auto vertex=params["vertex"].get<std::string>();const auto at=vertex.find(engine);Require(at!=std::string::npos&&vertex.find(engine,at+engine.size())==std::string::npos,"Spectator HUD assembled definitions differ");vertex.replace(at,engine.size(),"//__ENGINEUNIFORMBUFFERDEFS__");
  // Only the original vertex source contains the replacement marker.
  Require(targetPriorityProgram::Hash(vertex)==spectatorHUDProgram::VertexHash&&targetPriorityProgram::Hash(params["fragment"].get<std::string>())==spectatorHUDProgram::FragmentHash,"Original Spectator HUD shaders changed");
  owner->validated=true;return true;
 });
 Bind(context,"Activate",[owner](){Require(owner->validated&&targetPriorityProgram::Available(),"Spectator HUD compiled program missing");owner->active=true;});
 Bind(context,"Deactivate",[owner](){owner->active=false;});
 Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;});
 Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> instance,std::shared_ptr<Buffer> index,GLenum primitive,int count,int first,int instances,int base){
  Require(owner->active&&owner->validated&&targetPriorityProgram::Available(),"Spectator HUD active program missing");
  for(const auto& b:{vertex,instance,index})Require(b&&b->owner==owner&&!b->deleted,"Spectator HUD actual VAO buffer missing/deleted");
  Require(primitive==GL_TRIANGLES&&count>=0&&count%3==0&&first==0&&instances>=0&&base==0,"Spectator HUD original indexed invocation differs");
  const auto positions=vertex->value->CaptureNativeSnapshot(),knobs=instance->value->CaptureNativeSnapshot(),indices=index->value->CaptureNativeSnapshot();
  for(unsigned stream=0;stream<2;++stream){const auto& layout=(stream?knobs:positions)->layout;const unsigned attributes=stream?2:1;
   Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==16*attributes&&layout.attributes.size()==attributes,"Spectator HUD vertex/instance stream layout differs");
   for(unsigned n=0;n<attributes;++n){const auto& a=layout.attributes[n];Require(a.id==stream+n&&a.offset==n*16&&a.byteSize==16&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Spectator HUD original vec4 attribute differs");}
  }
  const auto& layout=indices->layout;Require(layout.kind==NativeBufferKind::Index&&layout.stride==4&&layout.attributes.size()==1&&layout.attributes[0].scalarType==NativeScalarType::UInt32,"Spectator HUD original uint index stream differs");
  Require(size_t(count)<=layout.elements&&size_t(instances)<=knobs->layout.elements,"Spectator HUD original range exceeds buffer");
  Require(!instances||size_t(count)<=std::numeric_limits<uint32_t>::max()/size_t(instances),"Spectator HUD triangle count overflow");
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> clipW;const size_t total=size_t(count)*size_t(instances);vertices.reserve(total);clipW.reserve(total);
  for(int n=0;n<instances;++n){std::array<float,4> bias{},color{};std::memcpy(bias.data(),knobs->bytes.data()+size_t(n)*32,16);std::memcpy(color.data(),knobs->bytes.data()+size_t(n)*32+16,16);
   for(int j=0;j<count;++j){uint32_t id;std::memcpy(&id,indices->bytes.data()+size_t(j)*4,4);Require(id<positions->layout.elements,"Spectator HUD index outside original vertex stream");
    std::array<float,4> pos;std::memcpy(pos.data(),positions->bytes.data()+size_t(id)*16,16);for(unsigned k=0;k<4;++k)pos[k]+=bias[k];
    vertices.push_back({pos[0],pos[1],pos[2],color});clipW.push_back({pos[3],0,0,0});
   }
  }
  // Freeze the exact range now: later partial uploads, soft reset or Delete
  // cannot change either the outline draw or the following inner draw.
  ui::GetRecorder().AppendSpectatorHUDTriangles(std::move(vertices),std::move(clipW));
 });
 return context;
}
} // namespace LuaNativeSpectatorHUD
