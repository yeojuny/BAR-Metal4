/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeWeaponRange.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaNativeCUSTextures.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWeaponRangePrograms.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "System/FileSystem/FileHandler.h"
#include <map>
#include <optional>
namespace LuaNativeWeaponRange {
namespace {
using namespace rendering;
MiniMapSink miniMapSink;bool ready=false,coverageReady=false;
std::optional<NativeWeaponRangeFrame> world;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program;struct Buffer;
struct Context {std::shared_ptr<Program> active;std::map<std::string,std::weak_ptr<Buffer>> buffers;std::map<unsigned,std::weak_ptr<Program>> programs;std::vector<std::string> groups;unsigned next=1,error=0;};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){Check((target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER),"Original31657 buffer target differs");value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
	std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
};
struct Program {NativeWeaponRangeProgramState state;unsigned identity;std::string label;bool deleted=false;Program(unsigned id,std::string_view v,std::string_view f):state(v,f),identity(id){}};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active && !c->active->deleted,"Original range uniform update has no live program");return c->active;}
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
	void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
		Check(Available()&&!deleted&&vertices&&instances,"Original range needs actual consumer and uploaded owners");const auto program=Active(context);
		Check((mode==GL_TRIANGLE_FAN||mode==GL_LINE_LOOP||mode==GL_TRIANGLES)&&count.value_or(0)>=0&&first.value_or(0)>=0&&instanceCount.value_or(1)>=0&&base.value_or(0)>=0,"Original range DrawArrays arguments differ");
		auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"Original range VBO draw cannot execute during display list compilation");
		NativeWeaponRangeDraw draw;draw.sourceDrawFrame=globalRendering->drawFrame;draw.program=program->state.Capture();draw.primitive=mode==GL_TRIANGLE_FAN?NativeDrawPrimitive::TriangleFan:mode==GL_LINE_LOOP?NativeDrawPrimitive::LineLoop:NativeDrawPrimitive::Triangles;
		draw.vertices=vertices->value->CaptureNativeSnapshot();draw.instances=instances->value->CaptureNativeSnapshot();draw.firstVertex=first.value_or(0);draw.vertexCount=count.value_or(draw.vertices->layout.elements);draw.firstInstance=base.value_or(0);draw.instanceCount=instanceCount.value_or(1);
		draw.original=OriginalCUSUniformCapture().Current();draw.models=OriginalCUSModelPublication().ModelUniforms();draw.surface=common.CurrentState().draw;draw.raster=common.CurrentWorldState();
  if(!draw.program.defines->composite){
   Check(draw.program.heightUnit>=0&&draw.program.heightUnit<int(NativeCUSTextureHistory::Units),"Original range height sampler unit differs");
   draw.height=OriginalNativeTextureContext().History().Binding(draw.program.heightUnit,NativeCUSTextureTarget::Image2D);
   // Exact31657 binds $info at1, but neither selected shader reads it.
   draw.info=OriginalNativeTextureContext().History().Binding(1,NativeCUSTextureTarget::Image2D);
   if(auto map=draw.height.object?std::dynamic_pointer_cast<const NativeMapTextureBacking>(draw.height.object->backing):nullptr)draw.mapSourceDependent=!map->Resource()||!map->Resource()->Completed();
   else draw.dependencyImage=OriginalNativeTextureContext().UI().DescribeBinding(draw.height).image;
  }
  if(draw.program.defines->composite || draw.program.maskClip>0.5f){
   Check(draw.program.maskUnit>=0&&draw.program.maskUnit<int(NativeCUSTextureHistory::Units),"Original31657 mask sampler unit differs");
   draw.coverageMask=OriginalNativeTextureContext().History().Binding(draw.program.maskUnit,NativeCUSTextureTarget::Image2D);
   draw.coverageDependencyImage=OriginalNativeTextureContext().UI().DescribeBinding(draw.coverageMask).image;
  }
		const bool pipScreen=common.IsScreenStage()&&!common.IsWorldStage()&&!common.HasActiveTarget()&&draw.program.inMiniMap>0.5f;
		if(common.IsMiniMapStage()||pipScreen){Check(draw.program.defines->family==NativeWeaponRangeFamily::Attack&&draw.program.inMiniMap>0.5f,"Original Defense range has no minimap callback");miniMapSink(common,std::move(draw));return;}
  if(common.HasActiveTarget()){
   Check(draw.program.defines->maskPass && common.IsWorldStage(),"Original31657 range target requires the actual world mask producer");
   draw.stage=NativeWorldDrawStage::World;ValidateNativeWeaponRangeDraw(draw);miniMapSink(common,std::move(draw));return;
  }
		const auto stage=common.CurrentWorldStage();Check(world&&world->sourceDrawFrame==draw.sourceDrawFrame&&stage&&*stage==NativeWorldDrawStage::World&&!common.HasActiveTarget(),"Original range requires its actual DrawWorld callback");
		draw.stage=*stage;draw.sequence=NextNativeWorldDrawSequence();ValidateNativeWeaponRangeDraw(draw);world->draws.push_back(std::move(draw));
	}
};
bool RecordTextureBinding(unsigned unit,NativeTextureLookup lookup)
{
	const bool parsed=lookup.parsed;
	ui::GetRecorder().RecordOperation([unit,lookup=std::move(lookup)](ui::NativeUIRecorder& replay){auto& shared=OriginalNativeTextureContext();const auto actual=unit?unit:shared.History().ActiveUnit().value();shared.Registry().LuaTexture(unit,[&]{auto value=lookup;if(value.object)value.object=shared.Registry().ResolveRecordedName(value.object);return value;});if(actual<2)replay.SyncObservedTextureBinding(actual);});return parsed;
}
bool BindHeight(unsigned unit,const std::string& name)
{
	Check(unit<NativeCUSTextureHistory::Units&&name=="$heightmap","Original range dedicated map binding differs");
	return RecordTextureBinding(unit,OriginalNativeMapTextures().Lookup(name));
}
bool BindInfo(lua_State* L,unsigned unit,const std::string& name)
{
	Check(unit<NativeCUSTextureHistory::Units&&name=="$info","Original range dedicated info binding differs");
	// Preserve the original active-info selector, including parsed ID0 and failed lookup.
	// The selected range shaders do not sample it, but its binding event remains real.
	return RecordTextureBinding(unit,LuaNativeCUSTextures::ParseTexture(L,name));
}
bool Texture(lua_State* L,unsigned unit,sol::object value)
{
	Check(unit==0 || unit==1 || unit==3,"Original range selected texture unit differs");
	if(value.is<bool>()){
		const bool enabled=value.as<bool>();
		// Original gl.Texture(unit,false) disables 2D use; it retains the bound object.
		ui::GetRecorder().RecordOperation([unit,enabled](ui::NativeUIRecorder& replay){
			auto& shared=OriginalNativeTextureContext();
			const auto actual=unit?unit:shared.History().ActiveUnit().value();
			shared.History().LuaTexture(unit,enabled);
			if(actual<2)replay.SyncObservedTextureBinding(actual);
		});
		return true; // Original boolean calls return true after the recorded state change.
	}
	const auto name=value.as<std::string>();
	if(name=="$heightmap")return BindHeight(unit,name);
	if(name=="$info")return BindInfo(L,unit,name);
	Check(unit==3 && name.starts_with('!'),"Original range selected coverage texture differs");
	return RecordTextureBinding(unit,LuaNativeCUSTextures::ParseTexture(L,name));
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"Original range needs its unsynced paired consumer");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();std::erase_if(contexts,[](const auto& value){return value.expired();});contexts.push_back(c);
	Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=c;return result;});
	Bind(table,"GetVAO",[c](){auto result=std::make_shared<VertexArray>();result->context=c;return result;});
	Bind(table,"CreateShader",[c](sol::table source){
		for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","Original range shader option differs");}
		auto result=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
		if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){std::array<int32_t,1> v{item.second.as<int32_t>()};if(!result->state.Integers(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
		if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){std::vector<float> v;if(item.second.is<sol::table>()){auto value=item.second.as<sol::table>();for(unsigned i=1;i<=value.size();++i)v.push_back(value[i].get<float>());}else v.push_back(item.second.as<float>());if(!result->state.Floats(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
		c->programs[result->identity]=result;return std::tuple{result,result->identity};
	});
	Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto program=value.as<std::shared_ptr<Program>>();Check(program&&!program->deleted,"Original range UseShader owner was deleted");c->active=std::move(program);return true;});
	Bind(table,"DeleteShader",[c](std::shared_ptr<Program> program){if(!program)return false;program->deleted=true;if(c->active==program)c->active.reset();return true;});
	Bind(table,"GetShaderLog",[](){return std::string{};});
	Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> program,const std::string& name){Check(program&&!program->deleted,"Original range uniform program deleted");return program->state.Location(name);});
	Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> program){Check(program&&!program->deleted,"Original range reflection program deleted");sol::state_view lua(L);auto result=lua.create_table();unsigned i=0;for(auto name:NativeWeaponRangeProgramState::ActiveUniforms()){auto row=lua.create_table();row["name"]=std::string(name);row["size"]=1;row["type"]=(name=="pipVisibleArea"||name.starts_with("fillColor"))?GL_FLOAT_VEC4:name=="rotationMiniMap"?GL_INT:(name=="heightmapTex"||name=="maskTex")?GL_SAMPLER_2D:GL_FLOAT;result[++i]=row;}return result;});
	Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
	Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
	Bind(table,"GetError",[c](){return std::exchange(c->error,0);});Bind(table,"BindHeight",&BindHeight);
	Bind(table,"BindInfo",[L](unsigned unit,const std::string& name){return BindInfo(L,unit,name);});
	Bind(table,"Texture",[L](unsigned unit,sol::object value){return Texture(L,unit,value);});
	Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"Original range object label exceeds bound");if(type==0x82e0){auto it=c->buffers.find(id.as<std::string>());Check(it!=c->buffers.end(),"Original range buffer label owner missing");auto buffer=it->second.lock();Check(bool(buffer),"Original range labelled buffer deleted");buffer->label=label;}else{Check(type==GL_PROGRAM,"Original range label object kind differs");auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"Original range program label owner missing");auto program=it->second.lock();Check(bool(program),"Original range labelled program deleted");program->label=label;}});
	Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"Original range debug group underflow");c->groups.pop_back();});return table;
}
}
void SetMiniMapSink(MiniMapSink sink,bool available){miniMapSink=std::move(sink);ready=available;if(!ready||!miniMapSink)coverageReady=false;}
void SetCoverageMaskConsumerReady(bool value){coverageReady=value;}
bool Available(){return ready&&coverageReady&&bool(miniMapSink);}
void BeginWorldFrame(){Check(!world,"Original range world frame began twice");world.emplace();world->sourceDrawFrame=globalRendering->drawFrame;}
std::shared_ptr<const NativeWeaponRangeFrame> EndWorldFrame(){Check(bool(world),"Original range world frame end has no begin");auto result=std::make_shared<NativeWeaponRangeFrame>(std::move(*world));world.reset();return result;}
void AbortWorldFrame()noexcept{world.reset();for(auto& weak:contexts)if(auto c=weak.lock())c->active.reset();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);});Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.value->Upload(data,attribute,offset,first,last);});Bind(b,"Delete",[](Buffer& b){b.value->Delete();});Bind(b,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
	Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
	family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b,"Original range vertex buffer missing");a.vertices=std::move(b);});Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b,"Original range instance buffer missing");a.instances=std::move(b);});Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.indices&&b->value->CaptureNativeSnapshot()->layout.kind==NativeBufferKind::Index,"Original31657 placeholder index slot differs");a.indices=std::move(b);});Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Available);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Original ranges are unsynced-only");constexpr std::array<std::string_view,9> paths{"LuaUI/Widgets/gui_attackrange_gl4.lua","LuaUI/Widgets/gui_defenserange_gl4.lua","modules/graphics/luashader.lua","modules/graphics/instancevbotable.lua","LuaUI/Shaders/weapon_range_rings_unified_gl4.vert.glsl","LuaUI/Shaders/weapon_range_rings_unified_gl4.frag.glsl","LuaUI/Include/range_coverage_mask_gl4.lua","LuaUI/Shaders/weapon_range_fill_composite_gl4.vert.glsl","LuaUI/Shaders/weapon_range_fill_composite_gl4.frag.glsl"};Check(std::find(paths.begin(),paths.end(),path)!=paths.end(),"Unknown original range archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original range archive source missing");return source;});
	Bind(family,"PrepareSource",[](std::string source){using namespace weaponRangeProgram;const std::array hashes{AttackHash,DefenseHash,uint64_t{12529407370752740489ULL},ShaderHelperHash,InstanceHelperHash,CoverageHelperHash};Check(std::find(hashes.begin(),hashes.end(),NativeWeaponRangeSourceHash(source))!=hashes.end(),"Original range archive identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeWeaponRange",family);return true;
}
}
#else
namespace LuaNativeWeaponRange {
void SetMiniMapSink(MiniMapSink,bool){}void SetCoverageMaskConsumerReady(bool){}bool Available(){return false;}void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeWeaponRangeFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
