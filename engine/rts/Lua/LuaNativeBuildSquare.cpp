/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeBuildSquare.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/UniformConstants.h"
#include "Rendering/Backend/NativeBuildSquarePrograms.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

namespace LuaNativeBuildSquare {
namespace {
using namespace rendering;
MiniMapSink miniMapSink;bool ready=false;
std::optional<std::vector<NativeBuildSquareDraw>> world;
std::uint64_t sourceFrame=0;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program;
struct Context {std::shared_ptr<Program> active;unsigned error=0;};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer {
	std::shared_ptr<LuaVBOImpl> value;bool deleted=false;
	Buffer(lua_State* L,GLenum target,bool frequent){Check(target==GL_ARRAY_BUFFER,"Original BuildSquare buffer target differs");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	void Define(int count,sol::optional<sol::object> layout){Check(!deleted,"Deleted BuildSquare buffer");value->Define(count,layout);}
	size_t Upload(const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!deleted,"Deleted BuildSquare buffer");return value->Upload(data,attr,offset,first,last);}
	void Delete(){if(!deleted)value->Delete();deleted=true;}
};
struct Program {NativeBuildSquareProgramState state;bool deleted=false;Program(std::string_view v,std::string_view f,std::string_view e):state(v,f,e){}};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted,"Original BuildSquare uniform/draw has no live program");return c->active;}
std::string EngineDefinition(int index)
{
	Check(index==0||index==1,"Original BuildSquare engine UBO index differs");const auto& text=UniformConstants::GetInstance().GetGLSLDefinition(index);
	Check(!text.empty(),"Original BuildSquare typed engine UBO definition missing");return text;
}
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances;bool deleted=false;
	void AttachVertexBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted,"Original BuildSquare vertex owner deleted");vertices=std::move(b);}
	void AttachInstanceBuffer(std::shared_ptr<Buffer> b){Check(!deleted&&b&&!b->deleted,"Original BuildSquare instance owner deleted");instances=std::move(b);}
	void Delete(){deleted=true;vertices.reset();instances.reset();}
	void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base)
	{
		Check(Available()&&!deleted&&vertices&&!vertices->deleted&&instances&&!instances->deleted,"Original BuildSquare needs live producer and real consumer");
		Check(mode==GL_TRIANGLE_STRIP && count.value_or(4)==4 && first.value_or(0)==0 && instanceCount.value_or(1)>=0 && base.value_or(0)>=0,"Original BuildSquare DrawArrays arguments differ");
		auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"Original BuildSquare VBO draw is not display-list code");
		NativeBuildSquareDraw d;d.sourceDrawFrame=globalRendering->drawFrame;d.program=Active(context)->state.Capture();d.vertices=vertices->value->CaptureNativeSnapshot();d.instances=instances->value->CaptureNativeSnapshot();
		d.firstVertex=first.value_or(0);d.vertexCount=count.value_or(4);d.firstInstance=base.value_or(0);d.instanceCount=instanceCount.value_or(1);d.original=OriginalCUSUniformCapture().Current();d.surface=common.CurrentState().draw;d.raster=common.CurrentWorldState();
		if(d.program.isMiniMap){
			const bool pipScreen=common.IsScreenStage()&&!common.IsWorldStage()&&!common.HasActiveTarget();
			Check(common.IsMiniMapStage()||pipScreen,"Original BuildSquare minimap requires its real engine/PiP callback");ValidateNativeBuildSquareDraw(d);miniMapSink(common,std::move(d));return;
		}
		Check(d.program.heightUnit==0,"Original BuildSquare height sampler differs");d.height=OriginalNativeTextureContext().History().Binding(0,NativeCUSTextureTarget::Image2D);
		if(auto map=d.height.object?std::dynamic_pointer_cast<const NativeMapTextureBacking>(d.height.object->backing):nullptr)d.mapSourceDependent=!map->Resource()||!map->Resource()->Completed();
		else d.dependencyImage=OriginalNativeTextureContext().UI().DescribeBinding(d.height).image;
		const auto stage=common.CurrentWorldStage();Check(world&&sourceFrame==d.sourceDrawFrame&&stage&&(*stage==NativeWorldDrawStage::PreUnit||*stage==NativeWorldDrawStage::World)&&!common.HasActiveTarget(),"Original BuildSquare requires its real world callback");
		d.stage=*stage;ValidateNativeBuildSquareDraw(d);d.sequence=NextNativeWorldDrawSequence();world->push_back(std::move(d));
	}
};
bool Texture(unsigned unit,sol::object value)
{
	Check(unit==0,"Original BuildSquare height unit differs");auto& common=ui::GetRecorder();
	if(value.is<bool>()){
		const auto enabled=value.as<bool>();common.RecordOperation([unit,enabled](ui::NativeUIRecorder& replay){auto& shared=OriginalNativeTextureContext();const auto actual=unit?unit:shared.History().ActiveUnit().value();shared.History().LuaTexture(unit,enabled);if(actual<2)replay.SyncObservedTextureBinding(actual);});return true;
	}
	Check(value.as<std::string>()=="$heightmap","Original BuildSquare texture source differs");auto lookup=OriginalNativeMapTextures().Lookup("$heightmap");const bool parsed=lookup.parsed;
	common.RecordOperation([unit,lookup=std::move(lookup)](ui::NativeUIRecorder& replay){auto& shared=OriginalNativeTextureContext();const auto actual=unit?unit:shared.History().ActiveUnit().value();shared.Registry().LuaTexture(unit,[&]{auto entry=lookup;if(entry.object)entry.object=shared.Registry().ResolveRecordedName(entry.object);return entry;});if(actual<2)replay.SyncObservedTextureBinding(actual);});return parsed;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Original BuildSquare needs unsynced paired consumer");sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();std::erase_if(contexts,[](const auto& v){return v.expired();});contexts.push_back(c);
	Bind(t,"GetVBO",[L](GLenum target,bool frequent){return std::make_shared<Buffer>(L,target,frequent);});
	Bind(t,"GetVAO",[c](){auto v=std::make_shared<VertexArray>();v->context=c;return v;});
	Bind(t,"GetEngineUniformBufferDef",&EngineDefinition);
	Bind(t,"CreateShader",[](sol::table source){
		for(const auto& item:source){const auto k=item.first.as<std::string>();Check(k=="vertex"||k=="fragment"||k=="uniformInt"||k=="uniformFloat","Original BuildSquare shader option differs");}
		auto p=std::make_shared<Program>(source["vertex"].get<std::string>(),source["fragment"].get<std::string>(),EngineDefinition(0)+"\n"+EngineDefinition(1));
		if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){const int at=p->state.Location(item.first.as<std::string>());std::array<int32_t,1> v{item.second.as<int32_t>()};Check(at>=15&&p->state.Integers(at,v),"Original BuildSquare initial integer uniform differs");}
		if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){const int at=p->state.Location(item.first.as<std::string>());std::vector<float> v;if(item.second.is<sol::table>()){auto a=item.second.as<sol::table>();for(unsigned i=1;i<=a.size();++i)v.push_back(a[i].get<float>());}else v.push_back(item.second.as<float>());Check(at>=0&&at<15&&p->state.Floats(at,v),"Original BuildSquare initial float uniform differs");}return p;
	});
	Bind(t,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted,"Original BuildSquare UseShader owner deleted");c->active=std::move(p);return true;});
	Bind(t,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
	Bind(t,"GetShaderLog",[](){return std::string{};});
	Bind(t,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"Original BuildSquare uniform owner deleted");return p->state.Location(name);});
	Bind(t,"Uniform",[c](int at,sol::variadic_args args){auto p=Active(c);std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(at,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
	Bind(t,"UniformInt",[c](int at,sol::variadic_args args){auto p=Active(c);std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(at,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
	Bind(t,"GetError",[c](){return std::exchange(c->error,0);});Bind(t,"Texture",&Texture);return t;
}
}
void SetMiniMapSink(MiniMapSink sink,bool available){miniMapSink=std::move(sink);ready=available;}
bool Available(){return ready&&bool(miniMapSink);}
void BeginWorldFrame(){Check(!world,"Original BuildSquare nested frame");world.emplace();sourceFrame=globalRendering->drawFrame;}
std::vector<NativeBuildSquareDraw> EndWorldFrame(){Check(bool(world),"Original BuildSquare frame has no begin");auto out=std::move(*world);world.reset();return out;}
void AbortWorldFrame()noexcept{world.reset();for(auto& w:contexts)if(auto c=w.lock())c->active.reset();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto f=lua.create_table();
	f.new_usertype<Buffer>("Buffer",sol::no_constructor,"Define",&Buffer::Define,"Upload",&Buffer::Upload,"Delete",&Buffer::Delete);
	f.new_usertype<VertexArray>("VertexArray",sol::no_constructor,"AttachVertexBuffer",&VertexArray::AttachVertexBuffer,"AttachInstanceBuffer",&VertexArray::AttachInstanceBuffer,"DrawArrays",&VertexArray::Draw,"Delete",&VertexArray::Delete);
	f.new_usertype<Program>("Program",sol::no_constructor);f["Buffer"]=sol::lua_nil;f["VertexArray"]=sol::lua_nil;f["Program"]=sol::lua_nil;
	f.set_function("Available",Available);Bind(f,"CreateContext",[L](){return CreateContext(L);});
	Bind(f,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L)&&path=="LuaUI/Widgets/gui_buildsquare_gl4.lua","Original BuildSquare archive path differs");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeBuildSquareSourceHash(source)==buildSquareProgram::WidgetHash,"Original31657 BuildSquare source identity differs");return source;});
	Bind(f,"PrepareSource",[](std::string source){Check(NativeBuildSquareSourceHash(source)==buildSquareProgram::WidgetHash,"Original31657 BuildSquare prepared source differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeBuildSquare",f);return true;
}
}
#else
namespace LuaNativeBuildSquare {
void SetMiniMapSink(MiniMapSink,bool){}bool Available(){return false;}void BeginWorldFrame(){}
std::vector<rendering::NativeBuildSquareDraw> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return false;}
}
#endif
