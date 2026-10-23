/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeLOSViewProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include <cstring>
#include <map>

namespace LuaNativeLOSView {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;
struct Owner {bool validated=false,active=false;NativeLOSViewInput uniforms;std::map<std::string,std::weak_ptr<Buffer>> buffers;};
struct Buffer:std::enable_shared_from_this<Buffer> {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;GLenum target=0;std::string label;};
inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L),"LOS View context must be unsynced");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("LOSViewBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"LOS View buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"LOS View buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("LOSViewBuffer",sol::lua_nil);
	Bind(type,"GetID",[](Buffer& b){Require(!b.deleted,"LOS View buffer deleted");auto id=std::to_string(b.value->CaptureNativeSnapshot()->object);b.owner->buffers[id]=b.shared_from_this();return id;});
	Bind(context,"ObjectLabel",[owner](GLenum kind,const std::string& id,const std::string& label){Require(kind==0x82e0,"LOS View label must name a buffer");const auto it=owner->buffers.find(id);Require(it!=owner->buffers.end(),"LOS View unknown buffer identity");auto b=it->second.lock();Require(b&&!b->deleted,"LOS View deleted buffer label");b->label=label;});
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require((target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER)&&!frequent,"LOS View original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->target=target;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& cache,const std::string& vertex,const std::string& fragment,const std::string& engine){
		Require(!owner->validated,"LOS View original program already exists");
		unsigned count=0;for(const auto& entry:cache){const auto key=entry.first.as<std::string>();Require(key=="vssrcpath"||key=="fssrcpath"||key=="uniformInt"||key=="uniformFloat"||key=="shaderName"||key=="shaderConfig","LOS View unexpected shader option");++count;}Require(count==6,"LOS View original cache incomplete");
		Require(cache["shaderName"].get<std::string>()=="LosViewShader GL4"&&cache["vssrcpath"].get<std::string>()=="LuaUI/Shaders/infolos_view.vert.glsl"&&cache["fssrcpath"].get<std::string>()=="LuaUI/Shaders/infolos_view.frag.glsl","LOS View original program identity differs");
		Require(targetPriorityProgram::Hash(vertex)==losViewProgram::VertexHash&&targetPriorityProgram::Hash(fragment)==losViewProgram::FragmentHash&&targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"LOS View original shader/engine source changed");
		auto defines=cache["shaderConfig"].get<sol::table>();count=0;for(const auto& entry:defines){const auto name=entry.first.as<std::string>();const auto value=entry.second.as<int>();Require((name=="DEBUG"&&value==0)||(name=="PREUNIT"&&value==1),"LOS View original feature defines differ");++count;}Require(count==2,"LOS View original defines missing");
		auto samplers=cache["uniformInt"].get<sol::table>();count=0;for(const auto& entry:samplers){const auto name=entry.first.as<std::string>();const auto unit=entry.second.as<int>();Require((name=="mapDepths"&&unit==0)||(name=="modelDepths"&&unit==1)||(name=="screenCopyTex"&&unit==2)||(name=="losTex"&&unit==3),"LOS View original sampler unit differs");++count;}Require(count==4,"LOS View sampler missing");
		auto uniforms=cache["uniformFloat"].get<sol::table>();count=0;for(const auto& entry:uniforms){Require(entry.first.as<std::string>()=="blendfactors","LOS View unknown uniform");++count;}Require(count==1,"LOS View original uniform missing");
		auto initial=uniforms["blendfactors"].get<sol::table>();Require(initial.size()==4,"LOS View uniform extent differs");for(unsigned i=0;i<4;++i){owner->uniforms.blendFactors[i]=initial[i+1].get<float>();Require(std::isfinite(owner->uniforms.blendFactors[i]),"LOS View nonfinite uniform");}
		owner->uniforms.compiled=NativeLOSViewConsumer();if(!owner->uniforms.compiled)return false;owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated&&owner->uniforms.compiled==NativeLOSViewConsumer(),"LOS View actual program missing");owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;owner->uniforms={};});
	Bind(context,"SetUniform",[owner](const std::string& name,sol::variadic_args values){Require(owner->active&&owner->validated&&name=="blendfactors"&&values.size()==4,"LOS View active uniform differs");auto& output=owner->uniforms.blendFactors;for(unsigned i=0;i<4;++i){const float value=values[i].as<float>();Require(std::isfinite(value),"LOS View nonfinite uniform");output[i]=value;}});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> index,std::shared_ptr<Buffer> instance,GLenum primitive,sol::optional<int> count,int first,int instances,int base){
		Require(owner->active&&owner->validated,"LOS View active original program missing");
		Require(vertex&&vertex->owner==owner&&!vertex->deleted&&vertex->target==GL_ARRAY_BUFFER,"LOS View original VAO buffer missing/deleted");
		Require(primitive==GL_TRIANGLES&&(!count||*count==6)&&first==0&&instances==1&&base==0,"LOS View original draw invocation differs");
		Require(index&&instance&&index->owner==owner&&instance->owner==owner&&!index->deleted&&!instance->deleted&&index->target==GL_ELEMENT_ARRAY_BUFFER&&instance->target==GL_ARRAY_BUFFER,"LOS View original fillVAOSlots placeholders missing");
		const auto indexData=index->value->CaptureNativeSnapshot(),instanceData=instance->value->CaptureNativeSnapshot();Require(indexData->layout.elements==1&&instanceData->layout.elements==1,"LOS View original placeholder extents differ");
		const auto data=vertex->value->CaptureNativeSnapshot();const auto& layout=data->layout;
		Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==16&&layout.elements==6&&layout.attributes.size()==1,"LOS View original six-vertex vec4 stream differs");
		const auto& a=layout.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==16&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"LOS View original position/UV attribute differs");
		auto input=std::make_shared<NativeLOSViewInput>(owner->uniforms);input->engine=OriginalCUSUniformCapture().Current();
		auto& recorder=ui::GetRecorder();Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"LOS View actual sampler observer missing");
		const auto los=recorder.TextureObserver()->BoundImage(3);input->los=los.image;input->losSampler=los.sampler;ValidateNativeLOSView(*input);
		Require(input->los->format!=ScreenTextureFormat::Depth32Float&&input->los->format!=ScreenTextureFormat::Depth24Unorm&&input->los->format!=ScreenTextureFormat::Depth16Unorm,"LOS View local allyteam image must have original color channels");
		Require(globalRendering&&input->engine->matrices->sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&input->engine->params->sourceDrawFrame==uint64_t(globalRendering->drawFrame),"LOS View current engine UBO missing");
		std::array<std::array<float,4>,6> positions;for(unsigned i=0;i<6;++i)std::memcpy(positions[i].data(),data->bytes.data()+i*16,16);
		ui::GetRecorder().AppendLOSView(positions,std::move(input));
	});
	return context;
}
} // namespace LuaNativeLOSView
