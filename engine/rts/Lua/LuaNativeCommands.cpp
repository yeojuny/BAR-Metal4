/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeCommands.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaOpenGLUtils.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCommandsPrograms.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/WorldFrameData.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "System/FileSystem/FileHandler.h"
#include <optional>
namespace LuaNativeCommands {
namespace {
using namespace rendering;
ModelProvider modelProvider;
std::optional<NativeCommandsFrame> world;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program;
struct Context{std::shared_ptr<Program> active;unsigned error=0;};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;
	Buffer(lua_State* L,GLenum target,bool frequent){Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER,"Original31657 Commands FX buffer target differs");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
};
struct Program {
	NativeCommandsProgramState state;bool deleted=false;
	Program(std::string_view vertex,std::string_view fragment):state(vertex,fragment){}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
	void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
		Check(Available() && world && !deleted && vertices && instances,"Original Commands FX actual consumer/buffers/frame are missing");
		Check(context->active && !context->active->deleted,"Original Commands FX draw has no active source program");
		Check(mode==GL_TRIANGLE_STRIP && count.value_or(0)>=0 && first.value_or(0)>=0 && instanceCount.value_or(1)>=0 && base.value_or(0)>=0,"Original Commands FX DrawArrays arguments differ");
		auto& common=ui::GetRecorder();auto stage=common.CurrentWorldStage();Check(stage && *stage==NativeWorldDrawStage::PreUnit && !common.IsCompiling() && !common.HasActiveTarget(),"Original Commands FX draw must be in original DrawWorldPreUnit");
		NativeCommandsDraw draw;draw.sourceDrawFrame=globalRendering->drawFrame;Check(world->sourceDrawFrame==draw.sourceDrawFrame,"Original Commands FX frame is stale");
		draw.sequence=NextNativeWorldDrawSequence();draw.program=context->active->state.Capture();draw.vertices=vertices->value->CaptureNativeSnapshot();draw.instances=instances->value->CaptureNativeSnapshot();
		draw.firstVertex=first.value_or(0);draw.vertexCount=count.value_or(draw.vertices->layout.elements);draw.firstInstance=base.value_or(0);draw.instanceCount=instanceCount.value_or(1);
		draw.original=OriginalCUSUniformCapture().Current();draw.surface=common.CurrentState().draw;draw.raster=common.CurrentWorldState();
		Check(draw.program.textureUnit>=0 && draw.program.textureUnit<int(NativeCUSTextureHistory::Units),"Original Commands FX sampler unit is invalid");
		draw.image=OriginalNativeTextureContext().History().Binding(draw.program.textureUnit,NativeCUSTextureTarget::Image2D);
		ValidateNativeCommandsDraw(draw);world->draws.push_back(std::move(draw));
	}
};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& context){Check(context->active && !context->active->deleted,"Original Commands FX uniform update needs a live ActiveShader scope");return context->active;}
void RawUnitShape(int definition,int,sol::optional<bool> raw,sol::optional<bool> screen,sol::optional<bool> opaque)
{
	Check(Available() && raw.value_or(true) && !screen.value_or(false) && !opaque.value_or(true),"Original Commands FX raw UnitShape invocation differs");
	const auto* def=unitDefHandler->GetUnitDefByID(definition);if(!def)return;const auto* source=def->LoadModel();if(!source)return;
	const auto model=modelProvider(source);Check(bool(model),"Original Commands FX model provider lost the actual model");
	// GLObjectShape selects the legacy drawer, but rawState=true bypasses all
	// alpha material/texture setup. DrawStatic retains the caller's state.
	ui::GetRecorder().RecordOperation([model](ui::NativeUIRecorder& replay){
		const auto stage=replay.CurrentWorldStage();Check(stage && *stage==NativeWorldDrawStage::PreUnit,"Original Commands FX ghost callback differs");
		for(const auto& piece:model->pieces){if(piece.indices.empty())continue;replay.PushMatrix();replay.MultMatrix(piece.bindPose);replay.Begin(ScreenPrimitive::Triangles,"original Commands FX raw UnitShape bind pose");
			for(auto index:piece.indices){Check(index<piece.vertices.size(),"Original Commands FX model index exceeds its owner");const auto& v=piece.vertices[index];replay.Normal(v.normal[0],v.normal[1],v.normal[2]);replay.TexCoord(v.uv[0][0],v.uv[0][1]);replay.MultiTexCoord(1,v.uv[1][0],v.uv[1][1]);replay.Vertex(v.position[0],v.position[1],v.position[2]);}
			replay.End();replay.PopMatrix();}
	});
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && Available(),"Original Commands FX requires the unsynced paired consumer");sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();std::erase_if(contexts,[](const auto& value){return value.expired();});contexts.push_back(context);
	Bind(table,"GetVBO",[L](sol::optional<GLenum> target,sol::optional<bool> frequent){return std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));});
	Bind(table,"GetVAO",[context](){auto value=std::make_shared<VertexArray>();value->context=context;return value;});
	Bind(table,"CreateShader",[context](sol::table source){
		for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex" || key=="fragment" || key=="uniformInt" || key=="uniformFloat","Original Commands FX shader option differs");}
		auto result=std::make_shared<Program>(source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
		if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){const auto location=result->state.Location(item.first.as<std::string>());std::array<int32_t,1> v{item.second.as<int32_t>()};if(!result->state.Integers(location,v)&&!context->error)context->error=GL_INVALID_OPERATION;}
		if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){const auto location=result->state.Location(item.first.as<std::string>());std::array<float,1> v{item.second.as<float>()};if(!result->state.Floats(location,v)&&!context->error)context->error=GL_INVALID_OPERATION;}
		return result;
	});
	Bind(table,"DeleteShader",[](std::shared_ptr<Program> program){if(program)program->deleted=true;});
	Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> program,const std::string& name){Check(program && !program->deleted,"Original Commands FX shader was deleted");return program->state.Location(name);});
	Bind(table,"ActiveShader",[context](std::shared_ptr<Program> program,sol::protected_function callback,sol::variadic_args args){
		Check(program && !program->deleted,"Original Commands FX ActiveShader program was deleted");auto previous=std::exchange(context->active,std::move(program));
		auto result=callback(sol::as_args(args));context->active=std::move(previous);if(!result.valid()){sol::error error=result;throw std::runtime_error(error.what());}
	});
	Bind(table,"Uniform",[context](int location,sol::variadic_args args){auto p=Active(context);if(location==-1)return;std::vector<float> values;for(auto value:args)values.push_back(value.as<float>());if(!p->state.Floats(location,values)&&!context->error)context->error=GL_INVALID_OPERATION;});
	Bind(table,"UniformInt",[context](int location,sol::variadic_args args){auto p=Active(context);if(location==-1)return;std::vector<int32_t> values;for(auto value:args)values.push_back(value.as<int32_t>());if(!p->state.Integers(location,values)&&!context->error)context->error=GL_INVALID_OPERATION;});
	Bind(table,"UniformMatrix",[context](int location,const std::string& name){auto p=Active(context);if(location==-1)return;Check(name=="camera" || name=="projection","Original Commands FX named matrix differs");const auto* matrix=LuaOpenGLUtils::GetNamedMatrix(name.c_str());Check(bool(matrix),"Original Commands FX actual named camera matrix missing");std::array<float,16> values;std::memcpy(values.data(),*matrix,64);if(!p->state.Matrix(location,values)&&!context->error)context->error=GL_INVALID_OPERATION;});
	Bind(table,"GetError",[context](){return std::exchange(context->error,0);});Bind(table,"UnitShape",&RawUnitShape);return table;
}
}
void SetModelProvider(ModelProvider provider){modelProvider=std::move(provider);}
bool Available(){return IsNativeCommandsConsumerAvailable() && bool(modelProvider);}
void BeginWorldFrame(){Check(!world,"Original Commands FX world frame began twice");world.emplace();world->sourceDrawFrame=globalRendering->drawFrame;}
std::shared_ptr<const rendering::NativeCommandsFrame> EndWorldFrame(){Check(bool(world),"Original Commands FX world frame has no begin");auto result=std::make_shared<NativeCommandsFrame>(std::move(*world));world.reset();return result;}
void AbortWorldFrame()noexcept{world.reset();for(auto& weak:contexts)if(auto c=weak.lock())c->active.reset();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);});Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.value->Upload(data,attribute,offset,first,last);});Bind(b,"Delete",[](Buffer& b){b.value->Delete();});
	auto p=family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original Commands FX vertex buffer missing");a.vertices=std::move(b);});Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original Commands FX instance buffer missing");a.instances=std::move(b);});Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b && !a.indices && b->value->CaptureNativeSnapshot()->layout.kind==NativeBufferKind::Index,"Original31657 Commands FX index attachment differs");a.indices=std::move(b);});Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Available);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L) && (path=="LuaUI/Widgets/gui_commands_fx.lua" || path=="modules/graphics/instancevbotable.lua"),"Unknown original Commands FX archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source),"Original Commands FX archive source missing");return source;});
	Bind(family,"PrepareSource",[](std::string source){const auto hash=NativeCommandsSourceHash(source);Check(hash==commandsProgram::WidgetHash || hash==commandsProgram::InstanceHelperHash,"Original31657 Commands FX archive identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeCommands",family);return true;
}
}
#else
namespace LuaNativeCommands {
void SetModelProvider(ModelProvider){}bool Available(){return false;}void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeCommandsFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
