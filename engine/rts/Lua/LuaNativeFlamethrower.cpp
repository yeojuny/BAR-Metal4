/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeFlamethrower.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeFlamethrowerData.h"
#include "Rendering/Backend/NativeFlamethrowerPrograms.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <bitset>
#include <charconv>
#include <cmath>
#include <map>
#include <optional>

namespace LuaNativeFlamethrower {
namespace {
using namespace rendering;
NativeFlamethrowerRecorder recorder;
std::optional<NativeFlamethrowerUniforms> uniforms;
void Check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
std::uint64_t Hash(std::string_view source)
{
	std::uint64_t hash=14695981039346656037ULL;
	for (unsigned char byte:source) hash=(hash^byte)*1099511628211ULL;
	return hash;
}
template<typename Signature> struct LuaBoundary;
template<typename R,typename... Args> struct LuaBoundary<R(Args...)> {
	template<typename F> static auto Wrap(F function) { return [function=std::move(function)](sol::this_state lua,Args... args)->R {
		try { return function(std::forward<Args>(args)...); }
		catch (const std::exception& error) { lua_pushstring(lua,error.what()); }
		lua_error(lua); std::terminate();
	}; }
};
template<typename T,typename F> void Bind(T& table,const char* name,F function)
{
	table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));
}
struct Program;
struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	void ClearState() noexcept { active.reset(); }
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent)
	{
		Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER,"Original Flamethrower buffer target differs");
		value=std::make_shared<LuaVBOImpl>(target,frequent);
		CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	std::string Identity()
	{
		auto id=std::to_string(value->CaptureNativeSnapshot()->object);
		if (auto owner=context.lock()) owner->buffers[id]=shared_from_this();
		return id;
	}
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	std::array<float,12> defines{};
	std::array<float,3> wind{};
	bool deleted=false,windSet=false;
	void Activate() { Check(!deleted,"Deleted original Flamethrower program"); context->active=shared_from_this(); }
	void Deactivate() { if (context->active.lock().get()==this) context->active.reset(); }
	void Delete() { Deactivate(); deleted=true; }
	void SetUniformFloat(const std::string& name,sol::variadic_args arguments)
	{
		Check(!deleted && name=="windVelocity" && arguments.size()==3,"Original Flamethrower uniform name/arity differs");
		std::array<float,3> values{};
		for (unsigned n=0;n<3;++n) { values[n]=arguments[n].as<float>(); Check(std::isfinite(values[n]),"Original Flamethrower wind is nonfinite"); }
		wind=values; windSet=true;
	}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,instances,indices;
	bool deleted=false;
	void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base)
	{
		Check(IsNativeFlamethrowerConsumerAvailable() && recorder.Active() && !deleted && vertices && indices && instances,
			"Original Flamethrower draw requires its connected world scope and buffers");
		Check(mode==GL_TRIANGLES && count.value_or(6)==6 && first.value_or(0)==0 && base.value_or(0)==0 && instanceCount.value_or(0)>=0,
			"Original Flamethrower DrawElements invocation differs");
		auto program=context->active.lock();
		Check(program && !program->deleted && program->windSet,"Original Flamethrower active program/uniform is missing");
		auto& common=ui::GetRecorder(); const auto stage=common.CurrentWorldStage();
		Check(stage && *stage==NativeWorldDrawStage::World && !common.HasActiveTarget(),
			"Original Flamethrower draw is outside its original later-world stage");
		NativeFlamethrowerDraw draw;
		draw.stage=*stage; draw.vertices=vertices->value->CaptureNativeSnapshot(); draw.instances=instances->value->CaptureNativeSnapshot();
		draw.indices=indices->value->CaptureNativeSnapshot();
		Check(draw.indices->layout.elements==6,"Original Flamethrower attached index extent differs");
		draw.firstIndex=first.value_or(0); draw.indexCount=count.value_or(int(draw.indices->layout.elements));
		draw.firstInstance=base.value_or(0);
		draw.instanceCount=instanceCount.value_or(0); draw.defines=program->defines; draw.windVelocity=program->wind;
		// Ordinary gl.Texture and raster calls already reached the shared recorder
		// and logical texture observer exactly once. Read that actual state here.
		draw.surface=common.CurrentState().draw; draw.world=common.CurrentWorldState();
		// GLSL samplers consume the current bound objects, independently of
		// fixed-function enables. The shared observer is the sole bind writer.
		auto& history=OriginalNativeTextureContext().UI();
		for (unsigned n=0;n<2;++n) {
			const auto bound=history.BoundImage(n);
			draw.textures[n]=bound.image;draw.samplers[n]=bound.sampler;
		}
		recorder.Record(std::move(draw));
	}
	void Delete() noexcept { deleted=true; vertices.reset(); instances.reset(); indices.reset(); }
};
std::array<float,12> ReadDefines(std::string_view text)
{
	using namespace flamethrowerProgram;
	std::array<float,12> values{}; std::bitset<12> seen;
	while (!text.empty()) {
		Check(text.starts_with("#define "),"Original Flamethrower define syntax differs"); text.remove_prefix(8);
		const auto space=text.find(' '),end=text.find('\n');
		Check(space!=std::string_view::npos && end!=std::string_view::npos && space<end,"Original Flamethrower define line missing");
		const auto name=text.substr(0,space); auto found=std::find(ParameterNames.begin(),ParameterNames.end(),name);
		Check(found!=ParameterNames.end(),"Original Flamethrower define is unknown"); const auto index=std::size_t(found-ParameterNames.begin());
		Check(!seen[index],"Original Flamethrower define is duplicated"); const auto literal=text.substr(space+1,end-space-1);
		const auto parsed=std::from_chars(literal.data(),literal.data()+literal.size(),values[index]);
		Check(parsed.ec==std::errc{} && parsed.ptr==literal.data()+literal.size() && std::isfinite(values[index]),"Original Flamethrower GLSL numeric define is malformed");
		seen[index]=true; text.remove_prefix(end+1);
	}
	Check(seen.all(),"Original Flamethrower shader definition is missing"); return values;
}
void ValidateEngineDefinitions(std::string_view source)
{
	using namespace flamethrowerProgram;
	// game31657 returns this complete map-independent UBO/helper body. Water
	// functions are a separate marker-driven helper, unused by these exact stages.
	Check(source==EnginePrefix,"Original Flamethrower engine definitions differ");
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeFlamethrowerConsumerAvailable(),"Native Flamethrower requires its unsynced connected consumer");
	sol::state_view lua(L); auto table=lua.create_table(); auto context=std::make_shared<Context>();
	std::erase_if(contexts,[](const auto& value) { return value.expired(); }); contexts.emplace_back(context);
	Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent) {
		auto buffer=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false)); buffer->context=context; return buffer;
	});
	Bind(table,"GetVAO",[context]() { auto array=std::make_shared<VertexArray>(); array->context=context; return array; });
	auto create=[context](const std::string& name,const std::string& vertex,const std::string& fragment,const std::string& engine,const std::string& definitions,const sol::table& floats,const sol::table& samplers) {
		using namespace flamethrowerProgram;
		Check(name=="FlamethrowerGL4" && Hash(vertex)==VertexHash && Hash(fragment)==FragmentHash,"Original Flamethrower shader identity differs");
		ValidateEngineDefinitions(engine);
		auto program=std::make_shared<Program>(); program->context=context; program->defines=ReadDefines(definitions);
		Check(floats.empty(),"Original Flamethrower initial float uniforms differ"); std::bitset<2> seen;
		for (const auto& entry:samplers) {
			const auto key=entry.first.as<std::string>(); const auto slot=entry.second.as<int>();
			Check((key=="fireTex" && slot==0) || (key=="smokeTex" && slot==1),"Original Flamethrower sampler binding differs"); seen[slot]=true;
		}
		Check(seen.all(),"Original Flamethrower sampler is missing"); return program;
	};
	Bind(table,"CreateProgram",create); Bind(table,"CreateShader",create);
	Bind(table,"ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label) {
		Check(type==0x82e0 && label.size()<=1024,"Original Flamethrower buffer label differs"); const auto it=context->buffers.find(identity);
		Check(it!=context->buffers.end(),"Unknown original Flamethrower buffer identity"); auto buffer=it->second.lock();
		Check(bool(buffer),"Deleted original Flamethrower buffer identity"); buffer->label=label;
	});
	return table;
}
}
void CaptureUniforms()
{
	NativeFlamethrowerUniforms next{globalRendering->drawFrame,OriginalCUSUniformCapture().Current()};
	(void)ReadNativeFlamethrowerUniforms(next); uniforms=std::move(next);
}
void BeginWorldFrame()
{
	Check(uniforms && uniforms->sourceDrawFrame==globalRendering->drawFrame,"Original Flamethrower UBO-stage capture is missing/stale");
	recorder.Begin(*uniforms);
}
std::shared_ptr<const NativeFlamethrowerFrame> EndWorldFrame() { return recorder.End(); }
void AbortWorldFrame() noexcept { recorder.Abort(); for (auto& weak:contexts) if (auto context=weak.lock()) context->ClearState(); }
void Reset() { AbortWorldFrame(); uniforms.reset(); }
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl=sol::stack::get<sol::table>(L,-1); auto family=lua.create_table();
	auto buffer=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(buffer,"Define",[](Buffer& self,int count,sol::optional<sol::object> layout) { self.value->Define(count,layout); });
	Bind(buffer,"Upload",[](Buffer& self,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last) { return self.value->Upload(values,attribute,offset,first,last); });
	Bind(buffer,"Delete",[](Buffer& self) { self.value->Delete(); }); Bind(buffer,"GetBufferSize",[](Buffer& self) { return self.value->GetBufferSize(); });
	Bind(buffer,"GetID",[](Buffer& self) { return self.Identity(); }); Bind(buffer,"GetLabel",[](Buffer& self) { return self.label; });
	auto program=family.new_usertype<Program>("Program",sol::no_constructor);
	Bind(program,"Activate",[](Program& self) { self.Activate(); }); Bind(program,"Deactivate",[](Program& self) { self.Deactivate(); });
	Bind(program,"Finalize",[](Program& self) { self.Delete(); }); Bind(program,"Delete",[](Program& self) { self.Delete(); });
	Bind(program,"SetUniformFloat",[](Program& self,const std::string& name,sol::variadic_args args) { self.SetUniformFloat(name,args); });
	auto array=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(array,"AttachVertexBuffer",[](VertexArray& self,std::shared_ptr<Buffer> value) { Check(!self.deleted && value,"Original Flamethrower vertex attachment missing"); self.vertices=std::move(value); });
	Bind(array,"AttachInstanceBuffer",[](VertexArray& self,std::shared_ptr<Buffer> value) { Check(!self.deleted && value,"Original Flamethrower instance attachment missing"); self.instances=std::move(value); });
	// game31657 passes this to makeVAOandAttach, whose original Draw closure
	// selects DrawElements. Keep the actual unsigned-short buffer publication.
	Bind(array,"AttachIndexBuffer",[](VertexArray& self,std::shared_ptr<Buffer> value) { Check(!self.deleted && value,"Original Flamethrower index attachment missing"); self.indices=std::move(value); });
	Bind(array,"Delete",[](VertexArray& self) { self.Delete(); });
	Bind(array,"DrawElements",[](VertexArray& self,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base) { self.Draw(mode,count,first,instances,base); });
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);
	Bind(family,"Available",&IsNativeFlamethrowerConsumerAvailable); Bind(family,"CreateContext",[L]() { return CreateContext(L); });
	Bind(family,"LoadArchiveSource",[L](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(L),"Original Flamethrower source is unsynced-only");
		Check(path=="LuaRules/Gadgets/gfx_flamethrower_gl4.lua" || path=="modules/graphics/instancevbotable.lua" || path=="modules/graphics/luashader.lua","Unknown original Flamethrower archive source");
		CFileHandler file(path,SPRING_VFS_ZIP); std::string source;
		Check(file.FileExists() && file.LoadStringData(source),"Original Flamethrower archive source missing"); return source;
	});
	Bind(family,"PrepareSource",[](std::string source) {
		using namespace flamethrowerProgram; const auto hash=Hash(source);
		Check(hash==GadgetHash || hash==InstanceHelperHash || hash==ShaderHelperHash,"Original Flamethrower archive identity differs");
		LuaUtils::TracyRemoveAlsoExtras(source.data()); return source;
	});
	gl.set("NativeFlamethrower",family); return true;
}
}
#else
namespace LuaNativeFlamethrower {
void CaptureUniforms() {} void BeginWorldFrame() {} std::shared_ptr<const rendering::NativeFlamethrowerFrame> EndWorldFrame() { return {}; }
void AbortWorldFrame() noexcept {} void Reset() {} bool PushEntries(lua_State*) { return true; }
}
#endif
