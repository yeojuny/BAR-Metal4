/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeSquadHullProgram.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <cstring>
#include <vector>

namespace LuaNativeSquadHull {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner {bool validated=false,active=false;NativeSquadHullInput uniforms;};
struct Buffer {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;};
inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L),"Squad Hull context must be unsynced");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("SquadHullBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Squad Hull buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"Squad Hull buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("SquadHullBuffer",sol::lua_nil);
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER&&!frequent,"Squad Hull original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& params){
		Require(!owner->validated,"Squad Hull original program already exists");
		unsigned count=0;for(const auto& entry:params){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment","Squad Hull unexpected shader option/stage");++count;}Require(count==2,"Squad Hull original source incomplete");
		Require(squadHullProgram::Hash(params["vertex"].get<std::string>())==squadHullProgram::VertexHash&&squadHullProgram::Hash(params["fragment"].get<std::string>())==squadHullProgram::FragmentHash,"Original Squad Hull shader source changed");
		owner->uniforms={};owner->uniforms.compiled=NativeSquadHullConsumer();if(!owner->uniforms.compiled)return false;
		ValidateNativeSquadHull(owner->uniforms);owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated,"Squad Hull original program missing");ValidateNativeSquadHull(owner->uniforms);owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;owner->uniforms={};});
	Bind(context,"SetUniform",[owner](const std::string& name,sol::variadic_args values){
		Require(owner->active&&owner->validated,"Squad Hull uniform without active program");
		float* output=nullptr;unsigned count=0;
		if(name=="color"){output=owner->uniforms.color.data();count=4;}
		else if(name=="stripe"){output=owner->uniforms.stripe.data();count=3;}
		else if(name=="centroidRadius"){output=owner->uniforms.centroidRadius.data();count=3;}
		else if(name=="pulse"){output=&owner->uniforms.animation[0];count=1;}
		else if(name=="gradientCenter"){output=&owner->uniforms.animation[1];count=1;}
		Require(output&&values.size()==count,"Squad Hull original uniform name/extent differs");
		for(unsigned i=0;i<count;++i){const float value=values[i].as<float>();Require(std::isfinite(value),"Squad Hull nonfinite uniform");output[i]=value;}
	});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,GLenum primitive,int count,int first,int instances,int base){
		Require(owner->active&&owner->validated,"Squad Hull active original program missing");ValidateNativeSquadHull(owner->uniforms);
		Require(vertex&&vertex->owner==owner&&!vertex->deleted,"Squad Hull original VAO buffer missing/deleted");
		Require((primitive==GL_TRIANGLE_FAN||primitive==GL_LINE_LOOP)&&count>=3&&count<=512&&first==0&&instances==1&&base==0,"Squad Hull original draw invocation differs");
		const auto data=vertex->value->CaptureNativeSnapshot();const auto& layout=data->layout;
		Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==12&&layout.elements==512&&layout.attributes.size()==1,"Squad Hull original 512-vertex vec3 stream differs");
		const auto& a=layout.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==12&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Squad Hull original position attribute differs");
		std::vector<std::array<float,3>> positions(count);for(int i=0;i<count;++i)std::memcpy(positions[i].data(),data->bytes.data()+size_t(i)*12,12);
		// Copy every scalar before the next squad reuses the same VBO/program.
		ui::GetRecorder().AppendSquadHull(positions,primitive==GL_TRIANGLE_FAN?NativeDrawPrimitive::TriangleFan:NativeDrawPrimitive::LineLoop,std::make_shared<const NativeSquadHullInput>(owner->uniforms));
	});
	return context;
}
} // namespace LuaNativeSquadHull
