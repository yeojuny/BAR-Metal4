/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/UniformConstants.h"
#include "Rendering/GlobalRendering.h"
#include <cstring>
#include <cstddef>
#include <stdexcept>

namespace LuaNativeTargetPriority {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner { bool validated=false,active=false; };
struct Buffer {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;};

inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L)&&targetPriorityProgram::Available(),"TargetPriorityLines needs its actual compiled screen consumer");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("TargetPriorityBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"TargetPriority buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values){Require(!b.deleted,"TargetPriority buffer deleted");return b.value->Upload(values,sol::nullopt,sol::nullopt,sol::nullopt,sol::nullopt);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("TargetPriorityBuffer",sol::lua_nil);
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER&&frequent,"TargetPriority original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& params,const std::string& name,const std::string& engine){
		Require(targetPriorityProgram::Available()&&name=="TargetPriorityLines","Unknown original target-priority program");
		unsigned count=0;for(const auto& entry:params){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment","TargetPriority unexpected program option/stage");++count;}Require(count==2,"TargetPriority program source incomplete");
		Require(targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"TargetPriority engine UBO source changed");
		auto normalize=[&](std::string text){const auto at=text.find(engine);Require(at!=std::string::npos&&text.find(engine,at+engine.size())==std::string::npos,"TargetPriority assembled engine definitions differ");text.replace(at,engine.size(),"//__ENGINEUNIFORMBUFFERDEFS__");return text;};
		Require(targetPriorityProgram::Hash(normalize(params["vertex"].get<std::string>()))==targetPriorityProgram::VertexHash&&targetPriorityProgram::Hash(normalize(params["fragment"].get<std::string>()))==targetPriorityProgram::FragmentHash,"Original target-priority shader source changed");
		owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated&&targetPriorityProgram::Available(),"TargetPriority compiled program missing");owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> buffer,GLenum primitive,int count){
		Require(owner->active&&owner->validated&&targetPriorityProgram::Available()&&buffer&&buffer->owner==owner&&!buffer->deleted,"TargetPriority active program/buffer ownership differs");
		Require(primitive==GL_TRIANGLES&&count>=0&&count%3==0,"TargetPriority original triangle invocation differs");
		const auto source=buffer->value->CaptureNativeSnapshot();const auto& layout=source->layout;
		Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==28&&layout.attributes.size()==2&&size_t(count)<=layout.elements,"TargetPriority original seven-float vertex layout differs");
		for(unsigned i=0;i<2;++i){const auto& a=layout.attributes[i];Require(a.id==i&&a.offset==(i?12u:0u)&&a.byteSize==(i?16u:12u)&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"TargetPriority vertex attributes differ");}
		const auto engine=OriginalCUSUniformCapture().Current();Require(engine&&globalRendering,"TargetPriority actual engine UBO missing");ValidateNativeCUSUniformPublication(*engine);
		Require(engine->params->sourceDrawFrame<=uint64_t(globalRendering->drawFrame),"TargetPriority engine UBO from a future frame");
		std::array<float,2> view{};static_assert(offsetof(UniformParamsBuffer,viewGeometry)==32);std::memcpy(view.data(),engine->params->bytes.data()+offsetof(UniformParamsBuffer,viewGeometry),sizeof(view));
		std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> coords;vertices.reserve(count);coords.reserve(count);
		for(int i=0;i<count;++i){std::array<float,7> row;std::memcpy(row.data(),source->bytes.data()+size_t(i)*28,sizeof(row));vertices.push_back({row[0],row[1],0,{row[3],row[4],row[5],row[6]}});coords.push_back({row[2],0,0,0});}
		ui::GetRecorder().AppendTargetPriorityTriangles(std::move(vertices),std::move(coords),view);
	});
	return context;
}
} // namespace LuaNativeTargetPriority
