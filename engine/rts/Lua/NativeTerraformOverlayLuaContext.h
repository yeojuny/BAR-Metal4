/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeTerraformOverlayProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include <cstring>
#include <map>

namespace LuaNativeTerraformOverlay {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;
struct Owner {bool validated=false,active=false;NativeTerraformOverlayInput uniforms;std::map<std::string,std::weak_ptr<Buffer>> buffers;};
struct Buffer:std::enable_shared_from_this<Buffer> {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;std::string label;};
inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L),"Terraform Overlay context must be unsynced");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("TerraformOverlayBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Terraform Overlay buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"Terraform Overlay buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("TerraformOverlayBuffer",sol::lua_nil);
	Bind(type,"GetID",[](Buffer& b){Require(!b.deleted,"Terraform Overlay buffer deleted");auto id=std::to_string(b.value->CaptureNativeSnapshot()->object);b.owner->buffers[id]=b.shared_from_this();return id;});
	Bind(context,"ObjectLabel",[owner](GLenum kind,const std::string& id,const std::string& label){Require(kind==0x82e0,"Terraform Overlay label must name a buffer");const auto it=owner->buffers.find(id);Require(it!=owner->buffers.end(),"Terraform Overlay unknown buffer identity");auto b=it->second.lock();Require(b&&!b->deleted,"Terraform Overlay deleted buffer label");b->label=label;});
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER&&!frequent,"Terraform Overlay original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& cache,const std::string& vertex,const std::string& fragment,const std::string& engine){
		Require(!owner->validated,"Terraform Overlay original program already exists");
		unsigned count=0;for(const auto& entry:cache){const auto key=entry.first.as<std::string>();Require(key=="vssrcpath"||key=="fssrcpath"||key=="uniformInt"||key=="uniformFloat"||key=="shaderName"||key=="shaderConfig","Terraform Overlay unexpected shader option");++count;}Require(count==6,"Terraform Overlay original cache incomplete");
		Require(cache["shaderName"].get<std::string>()=="Terraform Image Overlay"&&cache["vssrcpath"].get<std::string>()=="LuaUI/Shaders/terraform_image_overlay.vert.glsl"&&cache["fssrcpath"].get<std::string>()=="LuaUI/Shaders/terraform_image_overlay.frag.glsl","Terraform Overlay original program identity differs");
		Require(targetPriorityProgram::Hash(vertex)==terraformOverlayProgram::VertexHash&&targetPriorityProgram::Hash(fragment)==terraformOverlayProgram::FragmentHash&&targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"Terraform Overlay original shader/engine source changed");
		auto defines=cache["shaderConfig"].get<sol::table>();for(const auto& entry:defines){(void)entry;Require(false,"Terraform Overlay original shader has no feature defines");}
		auto samplers=cache["uniformInt"].get<sol::table>();count=0;for(const auto& entry:samplers){const auto name=entry.first.as<std::string>();const auto unit=entry.second.as<int>();Require((name=="mapDepths"&&unit==0)||(name=="overlayTex"&&unit==1),"Terraform Overlay original sampler unit differs");++count;}Require(count==2,"Terraform Overlay sampler missing");
		auto uniforms=cache["uniformFloat"].get<sol::table>();count=0;for(const auto& entry:uniforms){const auto name=entry.first.as<std::string>();Require(name=="params1"||name=="params2","Terraform Overlay unknown uniform");++count;}Require(count==2,"Terraform Overlay original uniform missing");
		for(unsigned kind=0;kind<2;++kind){auto initial=uniforms[kind?"params2":"params1"].get<sol::table>();Require(initial.size()==4,"Terraform Overlay uniform extent differs");auto& output=kind?owner->uniforms.fit:owner->uniforms.placement;for(unsigned i=0;i<4;++i){output[i]=initial[i+1].get<float>();Require(std::isfinite(output[i]),"Terraform Overlay nonfinite uniform");}}
		owner->uniforms.compiled=NativeTerraformOverlayConsumer();if(!owner->uniforms.compiled)return false;owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated&&owner->uniforms.compiled==NativeTerraformOverlayConsumer(),"Terraform Overlay actual program missing");owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;owner->uniforms={};});
	Bind(context,"SetUniform",[owner](const std::string& name,sol::variadic_args values){Require(owner->active&&owner->validated&&(name=="params1"||name=="params2")&&values.size()==4,"Terraform Overlay active uniform differs");auto& output=name=="params1"?owner->uniforms.placement:owner->uniforms.fit;for(unsigned i=0;i<4;++i){const float value=values[i].as<float>();Require(std::isfinite(value),"Terraform Overlay nonfinite uniform");output[i]=value;}});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,GLenum primitive,sol::optional<int> count,int first,int instances,int base){
		Require(owner->active&&owner->validated,"Terraform Overlay active original program missing");
		Require(vertex&&vertex->owner==owner&&!vertex->deleted,"Terraform Overlay original VAO buffer missing/deleted");
		Require(primitive==GL_TRIANGLES&&(!count||*count==6)&&first==0&&instances==1&&base==0,"Terraform Overlay original draw invocation differs");
		const auto data=vertex->value->CaptureNativeSnapshot();const auto& layout=data->layout;
		Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==16&&layout.elements==6&&layout.attributes.size()==1,"Terraform Overlay original six-vertex vec4 stream differs");
		const auto& a=layout.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==16&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Terraform Overlay original position/UV attribute differs");
		auto input=std::make_shared<NativeTerraformOverlayInput>(owner->uniforms);input->engine=OriginalCUSUniformCapture().Current();ValidateNativeTerraformOverlay(*input);
		Require(globalRendering&&input->engine->matrices->sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&input->engine->params->sourceDrawFrame==uint64_t(globalRendering->drawFrame),"Terraform Overlay current engine UBO missing");
		std::array<std::array<float,4>,6> positions;for(unsigned i=0;i<6;++i)std::memcpy(positions[i].data(),data->bytes.data()+i*16,16);
		ui::GetRecorder().AppendTerraformOverlay(positions,std::move(input));
	});
	return context;
}
} // namespace LuaNativeTerraformOverlay
