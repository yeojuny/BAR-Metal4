/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeProfilerHistogramProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/UniformConstants.h"
#include "Rendering/GlobalRendering.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace LuaNativeProfilerHistogram {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner {bool validated=false,active=false;std::array<float,4> box{},color{1,1,1,1};};
struct Buffer {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;};

inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L)&&targetPriorityProgram::Available(),"ProfilerHistograms needs its actual compiled screen consumer");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("ProfilerHistogramBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"ProfilerHistograms buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"ProfilerHistograms buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("ProfilerHistogramBuffer",sol::lua_nil);
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER&&frequent,"ProfilerHistograms original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& params,const std::string& name,const std::string& engine){
		Require(targetPriorityProgram::Available()&&name.empty(),"Unknown original histogram program");
		unsigned count=0;for(const auto& entry:params){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","ProfilerHistograms unexpected program option/stage");++count;}Require(count==4,"ProfilerHistograms program source incomplete");
		Require(targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"ProfilerHistograms engine UBO source changed");
		auto normalize=[&](std::string text){const auto at=text.find(engine);Require(at!=std::string::npos&&text.find(engine,at+engine.size())==std::string::npos,"ProfilerHistograms assembled engine definitions differ");text.replace(at,engine.size(),"//__ENGINEUNIFORMBUFFERDEFS__");return text;};
		Require(targetPriorityProgram::Hash(normalize(params["vertex"].get<std::string>()))==profilerHistogramProgram::VertexHash&&targetPriorityProgram::Hash(normalize(params["fragment"].get<std::string>()))==profilerHistogramProgram::FragmentHash,"Original histogram shader source changed");
		auto samplers=params["uniformInt"].get<sol::table>();for(const auto& entry:samplers){(void)entry;Require(false,"ProfilerHistograms has no samplers");}
		auto uniforms=params["uniformFloat"].get<sol::table>();count=0;for(const auto& entry:uniforms){const auto key=entry.first.as<std::string>();Require(key=="boundingbox"||key=="color","ProfilerHistograms unknown uniform");++count;}Require(count==2,"ProfilerHistograms uniform inputs missing");
		for(unsigned kind=0;kind<2;++kind){auto initial=uniforms[kind?"color":"boundingbox"].get<sol::table>();Require(initial.size()==4,"ProfilerHistograms uniform extent differs");auto& output=kind?owner->color:owner->box;for(unsigned i=0;i<4;++i){output[i]=initial[i+1].get<float>();Require(std::isfinite(output[i]),"ProfilerHistograms nonfinite uniform");}}
		owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated&&targetPriorityProgram::Available(),"ProfilerHistograms compiled screen program missing");owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;});
	Bind(context,"SetUniform",[owner](const std::string& name,sol::variadic_args values){Require(owner->active&&owner->validated&&(name=="boundingbox"||name=="color")&&values.size()==4,"ProfilerHistograms active uniform differs");auto& output=name=="color"?owner->color:owner->box;for(unsigned i=0;i<4;++i){const float value=values[i].as<float>();Require(std::isfinite(value),"ProfilerHistograms nonfinite uniform");output[i]=value;}});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,GLenum primitive,sol::optional<int> requestedCount,int first,int instances,int base){
		Require(owner->active&&owner->validated&&targetPriorityProgram::Available(),"ProfilerHistograms active compiled screen program missing");
		Require(vertex&&vertex->owner==owner&&!vertex->deleted,"ProfilerHistograms original VAO buffer missing/deleted");
		Require(primitive==GL_LINE_STRIP&&first>=0&&instances==1&&base==0,"ProfilerHistograms original line-strip invocation differs");
		const auto data=vertex->value->CaptureNativeSnapshot();const auto& layout=data->layout;
		Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==8&&layout.attributes.size()==1,"ProfilerHistograms original vec2 stream differs");
		const auto& a=layout.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==8&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"ProfilerHistograms original coordinate attribute differs");
		Require(size_t(first)<=layout.elements,"ProfilerHistograms first vertex exceeds buffer");
		const size_t available=layout.elements-size_t(first);Require(!requestedCount||*requestedCount>=0,"ProfilerHistograms negative vertex count");
		const size_t count=requestedCount?size_t(*requestedCount):available;Require(count<=available,"ProfilerHistograms original draw range exceeds buffer");
		const auto engine=OriginalCUSUniformCapture().Current();Require(engine&&globalRendering,"ProfilerHistograms actual engine UBO missing");ValidateNativeCUSUniformPublication(*engine);
		Require(engine->params->sourceDrawFrame<=uint64_t(globalRendering->drawFrame),"ProfilerHistograms engine UBO from a future frame");
		std::array<float,2> view{};static_assert(offsetof(UniformParamsBuffer,viewGeometry)==32);std::memcpy(view.data(),engine->params->bytes.data()+offsetof(UniformParamsBuffer,viewGeometry),sizeof(view));
		std::vector<std::array<float,2>> coords(count);for(size_t i=0;i<count;++i)std::memcpy(coords[i].data(),data->bytes.data()+(size_t(first)+i)*8,8);
		ui::GetRecorder().AppendProfilerHistogram(std::move(coords),view,owner->box,owner->color);
	});
	return context;
}
} // namespace LuaNativeProfilerHistogram
