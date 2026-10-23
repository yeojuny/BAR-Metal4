/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeFrameGrapherProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/UniformConstants.h"
#include "Rendering/GlobalRendering.h"
#include <cstring>
#include <cstddef>
#include <stdexcept>
#include <map>
#include <cmath>

namespace LuaNativeFrameGrapher {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;
struct Owner { bool validated=false,active=false;std::array<float,4> parameters{};std::map<std::string,std::weak_ptr<Buffer>> buffers; };
struct Buffer:std::enable_shared_from_this<Buffer> {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;std::string label;};

inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L)&&targetPriorityProgram::Available(),"FrameGrapher needs its actual compiled screen consumer");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("FrameGrapherBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"FrameGrapher buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"FrameGrapher buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("FrameGrapherBuffer",sol::lua_nil);
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require(target==GL_ARRAY_BUFFER,"FrameGrapher original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});
	Bind(context,"ValidateProgram",[owner](const sol::table& params,const std::string& name,const std::string& engine){
		Require(targetPriorityProgram::Available()&&name.empty(),"Unknown original frame-grapher program");
		unsigned count=0;for(const auto& entry:params){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","FrameGrapher unexpected program option/stage");++count;}Require(count==4,"FrameGrapher program source incomplete");
		Require(targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"FrameGrapher engine UBO source changed");
		auto normalize=[&](std::string text){const auto at=text.find(engine);Require(at!=std::string::npos&&text.find(engine,at+engine.size())==std::string::npos,"FrameGrapher assembled engine definitions differ");text.replace(at,engine.size(),"//__ENGINEUNIFORMBUFFERDEFS__");return text;};
		Require(targetPriorityProgram::Hash(normalize(params["vertex"].get<std::string>()))==frameGrapherProgram::VertexHash&&targetPriorityProgram::Hash(normalize(params["fragment"].get<std::string>()))==frameGrapherProgram::FragmentHash,"Original frame-grapher shader source changed");
		auto samplers=params["uniformInt"].get<sol::table>();for(const auto& entry:samplers){(void)entry;Require(false,"FrameGrapher has no samplers");}
		auto uniforms=params["uniformFloat"].get<sol::table>();count=0;for(const auto& entry:uniforms){Require(entry.first.as<std::string>()=="shaderparams","FrameGrapher unknown uniform");++count;}Require(count==1,"FrameGrapher shaderparams missing");
		auto initial=uniforms["shaderparams"].get<sol::table>();Require(initial.size()==4,"FrameGrapher uniform extent differs");for(unsigned i=0;i<4;++i){owner->parameters[i]=initial[i+1].get<float>();Require(std::isfinite(owner->parameters[i]),"FrameGrapher nonfinite parameter");}
		owner->validated=true;return true;
	});
	Bind(context,"Activate",[owner](){Require(owner->validated&&targetPriorityProgram::Available(),"FrameGrapher compiled program missing");owner->active=true;});
	Bind(context,"Deactivate",[owner](){owner->active=false;});
	Bind(context,"Finalize",[owner](){owner->active=owner->validated=false;});
	Bind(type,"GetID",[](Buffer& b){Require(!b.deleted,"FrameGrapher buffer deleted");auto id=std::to_string(b.value->CaptureNativeSnapshot()->object);b.owner->buffers[id]=b.shared_from_this();return id;});
	Bind(context,"ObjectLabel",[owner](GLenum kind,const std::string& id,const std::string& label){Require(kind==0x82E0,"FrameGrapher label must name a buffer");auto it=owner->buffers.find(id);Require(it!=owner->buffers.end(),"Unknown FrameGrapher buffer label");auto b=it->second.lock();Require(b&&!b->deleted,"Deleted FrameGrapher buffer label");b->label=label;});
	Bind(context,"SetUniform",[owner](const std::string& name,sol::variadic_args values){Require(owner->active&&owner->validated&&name=="shaderparams"&&values.size()==4,"FrameGrapher active uniform differs");for(unsigned i=0;i<4;++i){const float value=values[i].as<float>();Require(std::isfinite(value),"FrameGrapher nonfinite parameter");owner->parameters[i]=value;}});
	Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> instance,GLenum primitive,int count,int first,int instances,int base){
		Require(owner->active&&owner->validated&&targetPriorityProgram::Available(),"FrameGrapher active compiled program missing");
		Require(vertex&&instance&&vertex->owner==owner&&instance->owner==owner&&!vertex->deleted&&!instance->deleted,"FrameGrapher actual VAO buffers missing/deleted");
		Require(primitive==GL_TRIANGLES&&count==6&&first==0&&instances>=0&&base==0,"FrameGrapher original instanced triangle invocation differs");
		const auto coords=vertex->value->CaptureNativeSnapshot(),events=instance->value->CaptureNativeSnapshot();
		for(unsigned i=0;i<2;++i){const auto& layout=(i?events:coords)->layout;Require(layout.kind==NativeBufferKind::Vertex&&layout.stride==16&&layout.attributes.size()==1,"FrameGrapher original vec4 stream differs");const auto& a=layout.attributes[0];Require(a.id==i&&a.offset==0&&a.byteSize==16&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"FrameGrapher original attribute differs");}
		Require(coords->layout.elements==6&&size_t(instances)<=events->layout.elements,"FrameGrapher original draw range exceeds buffers");
		const auto engine=OriginalCUSUniformCapture().Current();Require(engine&&globalRendering,"FrameGrapher actual engine UBO missing");ValidateNativeCUSUniformPublication(*engine);
		Require(engine->params->sourceDrawFrame<=uint64_t(globalRendering->drawFrame),"FrameGrapher engine UBO from a future frame");
		std::array<float,2> view{};static_assert(offsetof(UniformParamsBuffer,viewGeometry)==32);std::memcpy(view.data(),engine->params->bytes.data()+offsetof(UniformParamsBuffer,viewGeometry),sizeof(view));
		std::vector<ScreenVertex> vertices;vertices.reserve(size_t(instances)*6);
		for(int i=0;i<instances;++i){std::array<float,4> event;std::memcpy(event.data(),events->bytes.data()+size_t(i)*16,sizeof(event));for(unsigned j=0;j<6;++j){std::array<float,4> xyuv;std::memcpy(xyuv.data(),coords->bytes.data()+j*16,sizeof(xyuv));vertices.push_back({xyuv[0],xyuv[1],0,event});}}
		ui::GetRecorder().AppendFrameGrapherTriangles(std::move(vertices),view,owner->parameters);
	});
	return context;
}
} // namespace LuaNativeFrameGrapher
