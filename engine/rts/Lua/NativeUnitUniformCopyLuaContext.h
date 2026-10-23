/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeUnitUniformCopy.h"
#include "Rendering/Models/ModelsMemStorage.h"
#include "Rendering/GlobalRendering.h"
#include <limits>

namespace LuaNativeUnitUniformCopy {
using namespace rendering;
inline void Require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer:std::enable_shared_from_this<Buffer>{
 std::shared_ptr<LuaVBOImpl> value;const void* handle=nullptr;bool deleted=false;
 ~Buffer(){Unbind();}
 void Unbind(){auto it=NativeUnitCopyBindings().find(handle);if(it!=NativeUnitCopyBindings().end()&&it->second.owner==this)NativeUnitCopyBindings().erase(it);}
 NativeBufferSnapshotRef Snapshot()const{Require(!deleted,"Unit uniform copy buffer deleted");return value->CaptureNativeSnapshot();}
};
struct Owner{std::shared_ptr<const NativeUnitUniformCopyProgram> program;bool active=false;uint32_t entries=0;};
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Unit uniform copy must be unsynced");
 const void* handle=CLuaHandle::GetHandle(L);auto owner=std::make_shared<Owner>();sol::state_view lua(L);auto context=lua.create_table();
 auto bufferType=context.new_usertype<Buffer>("UnitUniformCopyBuffer",sol::no_constructor);
 Bind(bufferType,"Define",[](Buffer& b,int elements,sol::optional<sol::object> layout){
  Require(!b.deleted&&elements>0&&elements%2048==0,"Unit uniform copy allocation differs");b.value->Define(elements,layout);
  const auto data=b.Snapshot();const auto& l=data->layout;Require(l.kind==NativeBufferKind::Storage&&l.stride==128&&l.attributes.size()==1,"Unit uniform copy storage layout differs");
  const auto& a=l.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==128&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Unit uniform copy vec4[8] layout differs");
 });
 Bind(bufferType,"Clear",[](Buffer& b){auto s=b.Snapshot();std::vector<uint8_t> zero(s->bytes.size());b.value->ReplaceNativeContents(zero);});
 Bind(bufferType,"Delete",[](Buffer& b){b.Unbind();b.value->Delete();b.deleted=true;});
 Bind(bufferType,"GetBufferSize",[](Buffer& b){b.Snapshot();return b.value->GetBufferSize();});
 Bind(bufferType,"Download",[](Buffer& b,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> count,sol::optional<bool> force){
  b.Snapshot();(void)force;return b.value->Download(attribute,offset,count,false); // Already completed real GPU contents.
 });
 Bind(bufferType,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){b.Snapshot();return b.value->Upload(values,attribute,offset,first,last);});
 Bind(bufferType,"BindBufferRange",[](Buffer& b,unsigned binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){
  Require(binding==4&&target.value_or(GL_SHADER_STORAGE_BUFFER)==GL_SHADER_STORAGE_BUFFER,"Unit uniform copy selected binding must be SSBO4");
  const auto snapshot=b.Snapshot();const auto elements=snapshot->layout.elements;
  Require(elements<=size_t(std::numeric_limits<int>::max()),"Unit uniform copy elements overflow");
  const size_t offset=std::max(first.value_or(0),0),used=std::clamp(count.value_or(int(elements)),1,int(elements));
  Require(offset<=elements&&used<=elements-offset,"Unit uniform copy binding range invalid");
  // Original BindBufferRange validates count but binds allocation remainder.
  std::weak_ptr<Buffer> weak=b.shared_from_this();
  NativeUnitCopyBindings()[b.handle]={&b,
   [weak,offset](){auto actual=weak.lock();Require(bool(actual),"Unit uniform copy bound buffer expired");auto data=actual->Snapshot();return MakeNativeBufferRange(data,4,offset,data->layout.elements-offset,5);},
   [weak](const NativeBufferSnapshotRef& previous,std::span<const uint8_t> bytes){auto actual=weak.lock();Require(bool(actual),"Unit uniform copy destination expired");const auto current=actual->Snapshot();Require(previous&&current->object==previous->object&&current->revision==previous->revision,"Unit uniform copy destination generation changed during compute");actual->value->ReplaceNativeContents(bytes);}};
  return 4;
 });
 Bind(bufferType,"UnbindBufferRange",[](Buffer& b,unsigned binding,sol::optional<int>,sol::optional<int>,sol::optional<GLenum> target){Require(binding==4&&target.value_or(GL_SHADER_STORAGE_BUFFER)==GL_SHADER_STORAGE_BUFFER,"Unit uniform copy binding differs");b.Snapshot();NativeUnitCopyBindings().erase(b.handle);return 4;});
 context.set("UnitUniformCopyBuffer",sol::lua_nil);
 Bind(context,"GetVBO",[L,handle](GLenum target,bool frequent){Require(target==GL_SHADER_STORAGE_BUFFER&&!frequent,"Unit uniform copy original allocation differs");auto b=std::make_shared<Buffer>();b->handle=handle;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
 Bind(context,"GetEngineModelUniformDataSize",[](int index){
  Require(index==0&&OriginalCUSModelPublication().Initialized(),"Unit uniform copy model uploader is unavailable");
  auto lock=CModelsLock::GetScopedLock();const auto entries=modelUniformsStorage.GetSize();
  Require(entries&&entries<=size_t(std::numeric_limits<int>::max()),"Unit uniform copy entry count invalid");return std::make_tuple(entries,entries*size_t(128));
 });
 Bind(context,"Initialize",[owner](const sol::table& source,const std::string& name){
  Require(!owner->program&&name=="cmpShader","Unit uniform copy program identity differs");
  unsigned keys=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="compute"||key=="uniformInt","Unit uniform copy unexpected shader field");++keys;}Require(keys==2,"Unit uniform copy shader fields missing");
  const auto shader=source["compute"].get<std::string>();Require(HashNativeUnitCopyShader(shader)==NativeUnitCopyShaderHash,"Unit uniform copy original compute source changed");
  const auto initial=source["uniformInt"].get<sol::table>();keys=0;for(const auto& entry:initial){Require(entry.first.as<std::string>()=="numEntries","Unit uniform copy uniform differs");++keys;}Require(keys==1,"Unit uniform copy entry count missing");
  const int entries=initial["numEntries"].get<int>();Require(entries>0,"Unit uniform copy entry count invalid");owner->program=CompileNativeUnitUniformCopy();owner->entries=entries;return bool(owner->program);
 });
 Bind(context,"Activate",[owner](){Require(bool(owner->program),"Unit uniform copy actual pipeline missing");owner->active=true;});
 Bind(context,"Deactivate",[owner](){owner->active=false;});
 Bind(context,"Finalize",[owner](){owner->active=false;owner->program.reset();});
 Bind(context,"SetUniformInt",[owner](const std::string& name,int entries){Require(owner->active&&owner->program&&name=="numEntries"&&entries>0,"Unit uniform copy uniform differs");owner->entries=entries;});
 Bind(context,"DispatchCompute",[owner,handle](unsigned x,unsigned y,unsigned z,unsigned barrier){
  Require(owner->active&&owner->program&&x==(uint64_t(owner->entries)+63)/64&&y==1&&z==1&&barrier==GL_SHADER_STORAGE_BARRIER_BIT,"Unit uniform copy dispatch differs");
  const auto selected=NativeUnitCopyBindings().find(handle);Require(selected!=NativeUnitCopyBindings().end()&&selected->second.capture&&selected->second.publish,"Unit uniform copy bound producer missing");
  const auto transaction=selected->second;const auto binding=transaction.capture();Require(binding.offset==0&&binding.size==binding.snapshot->bytes.size(),"Unit uniform copy dispatch must cover original full allocation");
  auto source=OriginalCUSModelPublication().ModelUniforms();Require(bool(source)&&globalRendering,"Unit uniform copy original published source missing");
  const auto bytes=owner->program->Dispatch(*source,*binding.snapshot,owner->entries,globalRendering->drawFrame);
  // Binding owner remains alive during the synchronous producer. Publishing is
  // separate so a failed encode/retirement leaves the old destination intact.
  transaction.publish(binding.snapshot,bytes);
 });
 return context;
}
} // namespace LuaNativeUnitUniformCopy
