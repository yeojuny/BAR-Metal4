/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeStartBox.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaNativeUI.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeStartBoxPrograms.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <map>
#include <optional>
namespace LuaNativeStartBox {
namespace {
using namespace rendering;
MiniMapSink miniMapSink;bool ready=false;
std::optional<NativeStartBoxFrame> world;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;struct Program;
struct Context {std::weak_ptr<Program> active;std::shared_ptr<Buffer> polygons;std::map<std::string,std::weak_ptr<Buffer>> buffers;unsigned error=0;};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER || target==GL_SHADER_STORAGE_BUFFER,"Original StartBox buffer target differs");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
};
struct Program:std::enable_shared_from_this<Program> {
	std::shared_ptr<Context> context;NativeStartBoxProgramState state;bool deleted=false;
	Program(std::shared_ptr<Context> c,std::string_view v,std::string_view f,std::string_view d):context(std::move(c)),state(v,f,d){}
	void Activate(){Check(!deleted,"Deleted original StartBox program");context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Delete(){Deactivate();deleted=true;}
	void Floats(const std::string& name,sol::variadic_args args){Check(!deleted,"Deleted original StartBox uniform owner");const auto location=state.GetUniformLocation(name);if(location<0)return;std::vector<float> values;for(auto arg:args)values.push_back(arg.as<float>());if(!state.SetFloats(location,values)&&!context->error)context->error=GL_INVALID_OPERATION;}
	void Integers(const std::string& name,sol::variadic_args args){Check(!deleted,"Deleted original StartBox uniform owner");const auto location=state.GetUniformLocation(name);if(location<0)return;std::vector<int32_t> values;for(auto arg:args)values.push_back(arg.as<int32_t>());if(!state.SetIntegers(location,values)&&!context->error)context->error=GL_INVALID_OPERATION;}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
	void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
		Check(Available() && !deleted && vertices,"Original StartBox needs actual consumer and vertex owner");auto program=context->active.lock();Check(program && !program->deleted,"Original StartBox draw has no live program");
		Check(mode==GL_TRIANGLES && count.value_or(0)>=0 && first.value_or(0)>=0 && instanceCount.value_or(1)>=0 && base.value_or(0)>=0,"Original StartBox DrawArrays parameters differ");
		auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"Original StartBox VBO draw is not part of its icon display list");
		NativeStartBoxDraw draw;draw.sourceDrawFrame=globalRendering->drawFrame;draw.program=program->state.Capture();draw.uniforms=OriginalCUSUniformCapture().Current();draw.vertices=vertices->value->CaptureNativeSnapshot();draw.firstVertex=first.value_or(0);draw.vertexCount=count.value_or(draw.vertices->layout.elements);draw.firstInstance=base.value_or(0);draw.instanceCount=instanceCount.value_or(1);
		if(instances)draw.instances=instances->value->CaptureNativeSnapshot();
		if(indices)draw.indices=indices->value->CaptureNativeSnapshot();
		if(draw.program.program==NativeStartBoxProgram::SDFBake){Check(bool(context->polygons),"Original StartBox SSBO binding4 is missing");auto snapshot=context->polygons->value->CaptureNativeSnapshot();draw.polygons=context->polygons->value->CaptureNativeRange(4,0,snapshot->layout.elements,5,64);}
		const auto active=NativeStartBoxActiveSamplers(draw.program);for(unsigned i=0;i<active.size();++i)if(active[i]){Check(draw.program.samplerUnits[i]>=0,"Original StartBox sampler unit is negative");draw.images[i]=OriginalNativeTextureContext().History().Binding(draw.program.samplerUnits[i],NativeCUSTextureTarget::Image2D);
			if(auto map=draw.images[i].object?std::dynamic_pointer_cast<const NativeMapTextureBacking>(draw.images[i].object->backing):nullptr)draw.mapSourceDependent=draw.mapSourceDependent || !map->Resource() || !map->Resource()->Completed();
			else draw.dependencyImages[i]=OriginalNativeTextureContext().UI().DescribeBinding(draw.images[i]).image;}
		if(draw.program.program==NativeStartBoxProgram::SDFBake){
			const auto stage=common.CurrentWorldStage();
			Check(stage && *stage==NativeWorldDrawStage::PreUnit && common.HasActiveTarget() && !common.IsMiniMapStage(),
				"Original StartBox bake requires the actual world PreUnit RTT");
			draw.stage=NativeStartBoxStage::TextureBake;miniMapSink(common,std::move(draw));return;
		}
		// Exact gui_pip calls DrawInMiniMap directly from DrawScreen after its
		// RTT blit. Its explicit minimap shader/viewport retain the screen stage;
		// entering an engine minimap scope here would replace original state.
		const bool minimapShader=draw.program.program==NativeStartBoxProgram::Polygon?draw.program.isMiniMap==1:draw.program.coneIsMiniMap>0.5f;
		const bool pipScreen=common.IsScreenStage() && !common.IsWorldStage() && !common.HasActiveTarget() && minimapShader;
		if(common.IsMiniMapStage() || pipScreen){draw.stage=NativeStartBoxStage::MiniMap;miniMapSink(common,std::move(draw));return;}
		const auto stage=common.CurrentWorldStage();Check(world && world->sourceDrawFrame==draw.sourceDrawFrame && stage && !common.HasActiveTarget(),"Original StartBox needs its actual world frame/callback");
		Check(*stage==NativeWorldDrawStage::PreUnit || *stage==NativeWorldDrawStage::World,"Original StartBox world callback differs");draw.stage=*stage==NativeWorldDrawStage::PreUnit?NativeStartBoxStage::WorldPreUnit:NativeStartBoxStage::World;
		draw.sequence=NextNativeWorldDrawSequence();draw.surface=common.CurrentState().draw;draw.raster=common.CurrentWorldState();draw.viewportWidth=globalRendering->winSizeX;draw.viewportHeight=globalRendering->winSizeY;ValidateNativeStartBoxDraw(draw);world->draws.push_back(std::move(draw));
	}
};
bool Texture(lua_State* L,const sol::variadic_args& args)
{
	Check(args.size()==1 || args.size()==2,"Original StartBox Texture argument count differs");unsigned unit=0;sol::object value=args[0];if(args.size()==2){unit=args[0].as<unsigned>();value=args[1];}
	Check(unit<NativeCUSTextureHistory::Units,"Original StartBox texture unit exceeds source limit");auto& common=ui::GetRecorder();
	if(value.is<bool>()){const bool enabled=value.as<bool>();common.RecordOperation([unit,enabled](ui::NativeUIRecorder& replay){const auto actual=OriginalNativeTextureContext().UI().LuaEnable(unit,enabled);if(actual<2)replay.SyncObservedTextureBinding(actual);});return true;}
	const auto name=value.as<std::string>();auto& shared=OriginalNativeTextureContext();NativeTextureLookup lookup;
	if(name=="$normals" || name=="$heightmap")lookup=OriginalNativeMapTextures().Lookup(name);
	else if(name=="$map_gbuffer_zvaltex"){const auto texture=shared.Geometry(NativeGeometryTextureRole::Map).Name(NativeGeometryTextureNames::DepthAttachment);lookup={true,shared.Registry().Find(texture),false};}
	else {auto binding=LuaNativeUI::ResolveTextureBinding(L,name);if(binding){const auto image=binding->Resolve().image;lookup={true,shared.UI().CaptureBind(name.starts_with('!')||name.starts_with('#')?binding:nullptr,image),false};}}
	const bool parsed=lookup.parsed;
	common.RecordOperation([unit,lookup=std::move(lookup)](ui::NativeUIRecorder& replay){auto& shared=OriginalNativeTextureContext();const auto actual=unit?unit:shared.History().ActiveUnit().value();
		shared.Registry().LuaTexture(unit,[&]{auto value=lookup;if(value.object)value.object=shared.Registry().ResolveRecordedName(value.object);return value;});if(actual<2)replay.SyncObservedTextureBinding(actual);});
	return parsed;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && Available(),"Original StartBox requires its unsynced paired consumer");sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();std::erase_if(contexts,[](const auto& c){return c.expired();});contexts.push_back(context);
	Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto buffer=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));buffer->context=context;return buffer;});
	Bind(table,"GetVAO",[context](){auto array=std::make_shared<VertexArray>();array->context=context;return array;});
	Bind(table,"CreateProgram",[context](const std::string& name,const std::string& vertex,const std::string& fragment,const std::string& engine,const std::string& definitions,sol::table floats,sol::table integers){
		ValidateNativeStartBoxEngineDefinitions(engine);auto program=std::make_shared<Program>(context,vertex,fragment,definitions);
		const auto kind=program->state.Capture().program;
		Check(name==(kind==NativeStartBoxProgram::Polygon?"Start Polygons GL4":kind==NativeStartBoxProgram::Cone?"Start Cones GL4":"Start Polygons SDF bake GL4"),"Original StartBox program name differs");
		for(const auto& item:integers){std::array<int32_t,1> values{item.second.as<int32_t>()};if(!program->state.SetIntegers(program->state.GetUniformLocation(item.first.as<std::string>()),values)&&!context->error)context->error=GL_INVALID_OPERATION;}
		for(const auto& item:floats){std::vector<float> values;if(item.second.is<sol::table>()){auto valuesTable=item.second.as<sol::table>();for(unsigned i=1;i<=valuesTable.size();++i)values.push_back(valuesTable[i].get<float>());}else values.push_back(item.second.as<float>());if(!program->state.SetFloats(program->state.GetUniformLocation(item.first.as<std::string>()),values)&&!context->error)context->error=GL_INVALID_OPERATION;}return program;
	});
	Bind(table,"Texture",[L](sol::variadic_args args){return Texture(L,args);});
	Bind(table,"GetError",[context](){return std::exchange(context->error,0);});
	Bind(table,"ObjectLabel",[context](GLenum type,const std::string& id,const std::string& label){Check(type==0x82e0 && label.size()<=1024,"Original StartBox label differs");auto it=context->buffers.find(id);Check(it!=context->buffers.end(),"Original StartBox buffer label has no source owner");auto buffer=it->second.lock();Check(bool(buffer),"Original StartBox labelled buffer was deleted");buffer->label=label;});return table;
}
}
void SetMiniMapSink(MiniMapSink sink,bool available){miniMapSink=std::move(sink);ready=available;}
bool Available(){return ready && bool(miniMapSink);}
void BeginWorldFrame(){Check(!world,"Original StartBox world frame began twice");world.emplace();world->sourceDrawFrame=globalRendering->drawFrame;}
std::shared_ptr<const NativeStartBoxFrame> EndWorldFrame(){Check(bool(world),"Original StartBox world frame end has no begin");auto result=std::make_shared<NativeStartBoxFrame>(std::move(*world));world.reset();return result;}
void AbortWorldFrame()noexcept{world.reset();for(auto& weak:contexts)if(auto c=weak.lock())c->active.reset();}
void Reset(){AbortWorldFrame();for(auto& weak:contexts)if(auto c=weak.lock())c->polygons.reset();}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);});Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.value->Upload(data,attribute,offset,first,last);});
	Bind(b,"Delete",[](Buffer& b){b.value->Delete();});Bind(b,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
	Bind(b,"BindBufferRange",[](std::shared_ptr<Buffer> b,unsigned index,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){Check(index==4 && !first && !count && !target,"Original StartBox SSBO binding invocation differs");auto context=b->context.lock();Check(bool(context),"Original StartBox buffer context was deleted");auto snapshot=b->value->CaptureNativeSnapshot();(void)b->value->CaptureNativeRange(index,0,snapshot->layout.elements,5,64);context->polygons=std::move(b);return int(index);});
	auto p=family.new_usertype<Program>("Program",sol::no_constructor);Bind(p,"Activate",[](Program& p){p.Activate();});Bind(p,"Deactivate",[](Program& p){p.Deactivate();});Bind(p,"Delete",[](Program& p){p.Delete();});Bind(p,"Finalize",[](Program& p){p.Delete();});Bind(p,"SetUniform",[](Program& p,const std::string& n,sol::variadic_args a){p.Floats(n,a);});Bind(p,"SetUniformFloat",[](Program& p,const std::string& n,sol::variadic_args a){p.Floats(n,a);});Bind(p,"SetUniformInt",[](Program& p,const std::string& n,sol::variadic_args a){p.Integers(n,a);});
	auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original StartBox vertex buffer missing");a.vertices=std::move(b);});Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original StartBox instance buffer missing");a.instances=std::move(b);});Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original StartBox index buffer missing");a.indices=std::move(b);});Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Available);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Original StartBox is unsynced-only");constexpr std::array<std::string_view,10> paths{"LuaUI/Widgets/map_startbox.lua","modules/graphics/luashader.lua","modules/graphics/instancevbotable.lua","LuaUI/Shaders/map_startpolygon_gl4.vert.glsl","LuaUI/Shaders/map_startpolygon_gl4.frag.glsl","LuaUI/Shaders/map_startcone_gl4.vert.glsl","LuaUI/Shaders/map_startcone_gl4.frag.glsl","luaui/include/startpolygon_sdf_gl4.lua","LuaUI/Shaders/startpolygon_sdf_bake_gl4.vert.glsl","LuaUI/Shaders/startpolygon_sdf_bake_gl4.frag.glsl"};Check(std::find(paths.begin(),paths.end(),path)!=paths.end(),"Unknown original StartBox archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source),"Original StartBox archive source missing");return source;});
	Bind(family,"PrepareSource",[](std::string source){using namespace startBoxProgram;constexpr std::array hashes{WidgetHash,ShaderHelperHash,InstanceHelperHash,PolygonVertexHash,PolygonFragmentHash,ConeVertexHash,ConeFragmentHash,SDFHelperHash,SDFBakeVertexHash,SDFBakeFragmentHash};Check(std::find(hashes.begin(),hashes.end(),NativeStartBoxSourceHash(source))!=hashes.end(),"Original StartBox archive identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeStartBox",family);return true;
}
}
#else
namespace LuaNativeStartBox {
void SetMiniMapSink(MiniMapSink,bool){}bool Available(){return false;}void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeStartBoxFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
