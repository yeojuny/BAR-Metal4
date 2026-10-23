/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeNapalmPreview.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeNapalmPreviewData.h"
#include "Rendering/Backend/NativeNapalmPreviewPrograms.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <cmath>
#include <optional>
#include <algorithm>

namespace LuaNativeNapalmPreview {
namespace {
using namespace rendering;
NativeNapalmPreviewRecorder recorder;
void Check(bool valid,const char* reason) { if (!valid) throw std::runtime_error(reason); }
std::uint64_t Hash(std::string_view text)
{
	std::uint64_t value=14695981039346656037ULL; for (unsigned char byte:text) value=(value^byte)*1099511628211ULL; return value;
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
struct Context {
	std::weak_ptr<Program> active;
	std::optional<NativeNapalmPreviewDraw> pending;
	void Clear() noexcept { pending.reset(); active.reset(); }
};
std::vector<std::weak_ptr<Context>> contexts;
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;
	float time=0;
	std::array<float,2> center{};
	std::array<float,4> color{1,0,0,.5f};
	bool deleted=false;
	void Activate() { Check(!deleted && !context->pending,"Original Napalm preview program activation is invalid"); context->active=shared_from_this(); }
	void Deactivate() { Check(!context->pending,"Original Napalm preview deactivation inside primitive"); if (context->active.lock().get()==this) context->active.reset(); }
	void Finalize() { Deactivate(); deleted=true; }
	void SetUniform(const std::string& name,sol::variadic_args arguments)
	{
		Check(!deleted && !context->pending,"Original Napalm preview uniform target is invalid");
		const unsigned count=name=="time"?1:name=="center"?2:name=="u_color"?4:0;
		Check(count && arguments.size()==count,"Original Napalm preview uniform name/arity differs");
		std::array<float,4> values{};
		for (unsigned n=0;n<count;++n) { values[n]=arguments[n].as<float>(); Check(std::isfinite(values[n]),"Original Napalm preview uniform is nonfinite"); }
		if (count==1) time=values[0]; else if (count==2) center={values[0],values[1]}; else color=values;
	}
};
std::array<float,16> Multiply(const std::array<float,16>& a,const std::array<float,16>& b)
{
	std::array<float,16> result{};
	for (unsigned column=0;column<4;++column) for (unsigned row=0;row<4;++row)
		for (unsigned k=0;k<4;++k) result[column*4+row]+=a[k*4+row]*b[column*4+k];
	return result;
}
void Begin(Context& context,GLenum primitive)
{
	Check(IsNativeNapalmPreviewConsumerAvailable() && recorder.Active() && !context.pending,"Original Napalm preview primitive scope is unavailable/nested");
	auto program=context.active.lock(); Check(program && !program->deleted,"Original Napalm preview active program is missing");
	auto& common=ui::GetRecorder(); const auto stage=common.CurrentWorldStage();
	Check(stage && *stage==NativeWorldDrawStage::PreUnit && !common.HasActiveTarget() && !common.IsCompiling(),"Original Napalm preview must retain its immediate DrawWorldPreUnit invocation");
	Check(primitive==GL_TRIANGLE_FAN || primitive==GL_TRIANGLE_STRIP,"Original Napalm preview primitive differs");
	NativeNapalmPreviewDraw draw; draw.stage=*stage;
	draw.primitive=primitive==GL_TRIANGLE_FAN?NativeDrawPrimitive::TriangleFan:NativeDrawPrimitive::TriangleStrip;
	draw.modelViewProjection=Multiply(common.GetMatrix(ui::NativeUIMatrixMode::Projection),common.GetMatrix(ui::NativeUIMatrixMode::ModelView));
	draw.time=program->time; draw.center=program->center; draw.color=program->color;
	draw.surface=common.CurrentState().draw; draw.world=common.CurrentWorldState(); context.pending=std::move(draw);
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeNapalmPreviewConsumerAvailable(),"Native Napalm preview requires its unsynced paired consumer");
	sol::state_view lua(L); auto table=lua.create_table(); auto context=std::make_shared<Context>();
	std::erase_if(contexts,[](const auto& value) { return value.expired(); }); contexts.emplace_back(context);
	Bind(table,"ProgramActive",[context]() { return !context->active.expired(); });
	Bind(table,"BeginProgram",[context](GLenum mode) { Begin(*context,mode); });
	Bind(table,"EndProgram",[context]() {
		Check(bool(context->pending),"Original Napalm preview has no active primitive");
		auto draw=std::move(*context->pending); context->pending.reset(); recorder.Record(std::move(draw));
	});
	Bind(table,"AbortProgram",[context]() { context->pending.reset(); });
	Bind(table,"RecordVertex",[context](sol::variadic_args arguments) {
		if (!context->pending) return false;
		Check(arguments.size()==3,"Original Napalm preview vertex arity differs");
		std::array<float,4> vertex{0,0,0,1};
		for (unsigned n=0;n<3;++n) { vertex[n]=arguments[n].as<float>(); Check(std::isfinite(vertex[n]),"Original Napalm preview vertex is nonfinite"); }
		context->pending->vertices.push_back(vertex); return true;
	});
	auto create=[context](const std::string& name,const std::string& vertex,const std::string& fragment,const sol::table& defines,const sol::table& floats,const sol::table& samplers) {
		using namespace napalmPreviewProgram;
		Check(name=="AoE Napalm Shader" && Hash(vertex)==VertexHash && Hash(fragment)==FragmentHash,"Original Napalm preview shader identity differs");
		Check(defines.empty() && samplers.empty(),"Original Napalm preview defines/samplers differ");
		unsigned found=0;
		for (const auto& entry:floats) {
			const auto name=entry.first.as<std::string>();
			if (name=="time") { Check(entry.second.as<float>()==0,"Original Napalm preview initial time differs"); found|=1; }
			else if (name=="center") {
				const auto values=entry.second.as<sol::table>(); Check(values.size()==2 && values.get<float>(1)==0 && values.get<float>(2)==0,"Original Napalm preview initial center differs"); found|=2;
			} else if (name=="u_color") {
				const auto values=entry.second.as<sol::table>(); Check(values.size()==4 && values.get<float>(1)==1 && values.get<float>(2)==0 && values.get<float>(3)==0 && values.get<float>(4)==.5f,"Original Napalm preview initial color differs"); found|=4;
			} else throw std::runtime_error("Unknown original Napalm preview initial uniform");
		}
		Check(found==7,"Original Napalm preview initial uniform is missing"); auto program=std::make_shared<Program>(); program->context=context; return program;
	};
	Bind(table,"CreateProgram",create); Bind(table,"CreateShader",create); return table;
}
}
void BeginWorldFrame() { recorder.Begin(globalRendering->drawFrame,OriginalCUSUniformCapture().Current()); }
std::shared_ptr<const NativeNapalmPreviewFrame> EndWorldFrame() { return recorder.End(); }
void AbortWorldFrame() noexcept { recorder.Abort(); for (auto& weak:contexts) if (auto context=weak.lock()) context->Clear(); }
void Reset() { AbortWorldFrame(); }
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L); auto gl=sol::stack::get<sol::table>(L,-1); auto family=lua.create_table();
	auto program=family.new_usertype<Program>("Program",sol::no_constructor);
	Bind(program,"Activate",[](Program& self) { self.Activate(); }); Bind(program,"Deactivate",[](Program& self) { self.Deactivate(); });
	Bind(program,"Finalize",[](Program& self) { self.Finalize(); }); Bind(program,"SetUniform",[](Program& self,const std::string& name,sol::variadic_args args) { self.SetUniform(name,args); });
	family.set("Program",sol::lua_nil); Bind(family,"Available",&IsNativeNapalmPreviewConsumerAvailable);
	Bind(family,"CreateContext",[L]() { return CreateContext(L); });
	Bind(family,"LoadArchiveSource",[L](const std::string& path) {
		Check(!CLuaHandle::GetHandleSynced(L),"Original Napalm preview source is unsynced-only");
		Check(path=="LuaUI/Widgets/gui_attack_aoe.lua" || path=="LuaUI/Shaders/gui_attack_aoe_napalm.vert.glsl" || path=="LuaUI/Shaders/gui_attack_aoe_napalm.frag.glsl" || path=="modules/graphics/luashader.lua","Unknown original Napalm preview archive source");
		CFileHandler file(path,SPRING_VFS_ZIP); std::string source; Check(file.FileExists() && file.LoadStringData(source),"Original Napalm preview archive source missing"); return source;
	});
	Bind(family,"PrepareSource",[](std::string source) {
		const auto hash=Hash(source);
		Check(hash==napalmPreviewProgram::WidgetHash || hash==napalmPreviewProgram::ShaderHelperHash,"Original Napalm preview Lua source identity differs");
		LuaUtils::TracyRemoveAlsoExtras(source.data()); return source;
	});
	gl.set("NativeNapalmPreview",family); return true;
}
}
#else
namespace LuaNativeNapalmPreview {
void BeginWorldFrame() {} std::shared_ptr<const rendering::NativeNapalmPreviewFrame> EndWorldFrame() { return {}; }
void AbortWorldFrame() noexcept {} void Reset() {} bool PushEntries(lua_State*) { return true; }
}
#endif
