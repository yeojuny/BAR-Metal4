/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeHealthBars.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaShaders.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeHealthBarsData.h"
#include "Rendering/Backend/NativeHealthBarsPrograms.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <charconv>
#include <cmath>
#include <map>
namespace LuaNativeHealthBars {
namespace {
using namespace rendering;
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename... Args>struct LuaBoundary<R(Args...)>{
	template<typename F>static auto Wrap(F function){return [function=std::move(function)](sol::this_state lua,Args... args)->R{
		try{return function(std::forward<Args>(args)...);}catch(const std::exception& error){lua_pushstring(lua,error.what());}lua_error(lua);std::terminate();};}
};
template<typename T,typename F>void Bind(T& table,const char* name,F function){table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));}
bool Ready(){const auto& textures=OriginalNativeTextureContext();return IsNativeHealthBarsConsumerAvailable() && textures.Initialized() && textures.UIAttached() && OriginalCUSUniformCapture().Initialized() && OriginalCUSModelPublication().Initialized();}
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
};
struct Buffer:std::enable_shared_from_this<Buffer>{
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){
		Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER,"Original Healthbars buffer target differs");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
};
struct Program:std::enable_shared_from_this<Program>{
	std::shared_ptr<Context> context;NativeHealthBarsConfig config;
	float iconDistance=27,cameraDistanceMult=1,cameraDistanceMultGlyph=4,skipGlyphsNumbers=0;bool deleted=false;
	void Activate(){Check(!deleted && Ready(),"Deleted/unavailable original Healthbars program");context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Delete(){Deactivate();deleted=true;}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,unusedIndex,unusedInstance;bool deleted=false;
	void Attach(std::shared_ptr<Buffer> b,unsigned slot){
		Check(!deleted && b && b->context.lock()==context,"Original Healthbars VAO attachment owner differs");
		auto& target=slot==0?vertices:slot==1?unusedIndex:unusedInstance;Check(!target,"Original Healthbars VAO slot already attached");
		const auto snapshot=b->value->CaptureNativeSnapshot();Check(snapshot && snapshot->layout.kind==(slot==1?NativeBufferKind::Index:NativeBufferKind::Vertex),"Original Healthbars VAO slot target differs");
		if(slot==2)Check(snapshot->layout.attributes.size()==1 && snapshot->layout.attributes[0].id==15,"Original Healthbars unused instance attribute differs");
		target=std::move(b);
	}
	void Draw(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){
		Check(Ready() && !deleted && vertices,"Original Healthbars consumer/point stream missing");
		auto program=context->active.lock();Check(program && !program->deleted,"Original Healthbars active program missing");
		auto& common=ui::GetRecorder();
		Check(common.IsScreenStage() && !common.CurrentWorldStage() && !common.HasActiveTarget() && !common.IsCompiling(),"Original Healthbars require their actual root DrawScreenEffects insertion");
		Check(primitive==GL_POINTS && !instances && !base && count.value_or(0)>=0 && first.value_or(0)>=0,"Original Healthbars GS point invocation differs");
		NativeHealthBarsDraw draw;draw.vertices=vertices->value->CaptureNativeSnapshot();draw.first=first.value_or(0);
		Check(draw.vertices->layout.elements<=UINT32_MAX,"Original Healthbars point capacity exceeds draw range");
		draw.count=count?std::uint32_t(*count):std::uint32_t(draw.vertices->layout.elements);
		draw.config=program->config;draw.iconDistance=program->iconDistance;draw.cameraDistanceMult=program->cameraDistanceMult;
		draw.cameraDistanceMultGlyph=program->cameraDistanceMultGlyph;draw.skipGlyphsNumbers=program->skipGlyphsNumbers;
		draw.uniforms={globalRendering->drawFrame,OriginalCUSUniformCapture().Current()};draw.modelUniforms=OriginalCUSModelPublication().ModelUniforms();
		// Public gl.Texture has already run exactly once, including its load,
		// active-unit semantics and sampler state. Read the shader's real unit0.
		auto& shared=OriginalNativeTextureContext();auto* observer=common.TextureObserver();
		Check(observer==&shared.UI(),"Original Healthbars shared texture observer missing");
		const auto sampled=observer->BoundImage(0);draw.atlas=sampled.image;draw.sampler=sampled.sampler;
		draw.surface=common.CurrentState().draw;common.AppendHealthBars(std::make_shared<NativeHealthBarsDraw>(std::move(draw)));
	}
};
std::map<std::string,std::string> ReadDefines(std::string_view text)
{
	std::map<std::string,std::string> result;
	while(!text.empty()){
		Check(text.starts_with("#define "),"Original Healthbars define syntax differs");text.remove_prefix(8);const auto space=text.find(' '),end=text.find('\n');
		Check(space!=std::string_view::npos && end!=std::string_view::npos && space<end,"Original Healthbars define line missing");
		Check(result.emplace(std::string(text.substr(0,space)),std::string(text.substr(space+1,end-space-1))).second,"Original Healthbars define repeated");text.remove_prefix(end+1);
	}
	return result;
}
NativeHealthBarsConfig ValidateDefines(const std::string& definitions)
{
	const auto d=ReadDefines(definitions);const bool debug=d.contains("DEBUGSHOW");
	Check(d.size()==18+unsigned(debug),"Original Healthbars compile variant has missing/unknown defines");
	const auto number=[&](const char* name){auto found=d.find(name);Check(found!=d.end(),"Original Healthbars numeric define missing");const auto& t=found->second;float value=0;const auto p=std::from_chars(t.data(),t.data()+t.size(),value);
		Check(p.ec==std::errc{} && p.ptr==t.data()+t.size() && std::isfinite(value),"Original Healthbars numeric define malformed");return value;};
	const std::pair<const char*,float> fixed[]={{"HEIGHTOFFSET",3},{"MAXVERTICES",40},{"CLIPTOLERANCE",1.2f},{"BARWIDTH",2.56f},{"BARSCALE",4},
		{"PERCENT_VISIBILITY_MAX",.99f},{"TIMER_VISIBILITY_MIN",0},{"BARSTEP",10},{"BOTTOMDARKENFACTOR",.5f},{"BARFADESTART",3200},{"BARFADEEND",3800},{"ATLASSTEP",.0625f},{"MINALPHA",.2f}};
	for(const auto& [name,expected]:fixed)Check(number(name)==expected,"Original Healthbars fixed compile value differs");
	Check(d.at("BGBOTTOMCOLOR")=="vec4(0.25, 0.25, 0.25, 0.8)" && d.at("BGTOPCOLOR")=="vec4(0.1, 0.1, 0.1, 0.8)","Original Healthbars background color differs");
	NativeHealthBarsConfig result;result.height=number("BARHEIGHT");result.corner=number("BARCORNER");result.smallCorner=number("SMALLERCORNER");result.debugShow=debug;
	Check(std::abs(result.corner-(.06f+result.height/9.f))<.00001f && std::abs(result.smallCorner-result.corner*.6f)<.00001f,"Original Healthbars derived corner shape differs");
	if(debug)Check(number("DEBUGSHOW")==1,"Original Healthbars debug variant differs");return result;
}
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && Ready(),"Original Healthbars require their actual unsynced renderer services");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();
	// These are the original CPU setters. Their values become visible at the
	// existing uploader's next publication, not a forced later live-store read.
	sol::stack::push(L,table);LuaShaders::PushModelBufferUniformEntries(L);lua_pop(L,1);
	Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));b->context=context;return b;});
	Bind(table,"GetVAO",[context](){auto vao=std::make_shared<VertexArray>();vao->context=context;return vao;});
	auto create=[context](const std::string& name,const std::string& vertex,const std::string& geometry,const std::string& fragment,const std::string& engine,const std::string& definitions,const sol::table& floats,const sol::table& samplers){
		using namespace healthBarsProgram;
		Check(name=="Health Bars Shader GL4" && NativeUnitPrimitiveSourceHash(vertex)==Sources[3].hash && NativeUnitPrimitiveSourceHash(geometry)==Sources[4].hash &&
			NativeUnitPrimitiveSourceHash(fragment)==Sources[5].hash && NativeUnitPrimitiveSourceHash(engine)==EngineDefinitionsHash,"Original Healthbars full shader source/engine layout differs");
		auto p=std::make_shared<Program>();p->context=context;p->config=ValidateDefines(definitions);
		unsigned count=0;for(const auto& item:floats){const auto name=item.first.as<std::string>();const auto value=item.second.as<float>();
			Check((name=="iconDistance" && value==27) || (name=="cameraDistanceMult" && value==1) || (name=="cameraDistanceMultGlyph" && value==4) || (name=="skipGlyphsNumbers" && value==0) || (name=="globalSizeMult" && value==1),"Original Healthbars initial float uniform differs");++count;}
		Check(count==5,"Original Healthbars initial float uniforms missing");count=0;
		for(const auto& item:samplers){Check(item.first.as<std::string>()=="healthbartexture" && item.second.as<int>()==0,"Original Healthbars sampler differs");++count;}
		Check(count==1,"Original Healthbars sampler missing");return p;
	};
	Bind(table,"CreateProgram",create);Bind(table,"CreateShader",create);
	Bind(table,"ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label){
		Check(type==0x82e0 && label.size()<=1024,"Original Healthbars buffer label invalid");auto it=context->buffers.find(identity);Check(it!=context->buffers.end(),"Unknown original Healthbars buffer");
		auto b=it->second.lock();Check(bool(b),"Deleted original Healthbars buffer");b->label=label;
	});return table;
}
}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);});
	Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.value->Upload(data,attribute,offset,first,last);});
	Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
	Bind(b,"InstanceDataFromFeatureIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){if(ids.is<int>())return b.value->InstanceDataFromFeatureIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromFeatureIDs(ids.as<sol::stack_table>(),attribute,offset);});
	Bind(b,"Delete",[](Buffer& b){b.value->Delete();});Bind(b,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
	auto p=family.new_usertype<Program>("Program",sol::no_constructor);Bind(p,"Activate",[](Program& p){p.Activate();});Bind(p,"Deactivate",[](Program& p){p.Deactivate();});Bind(p,"Delete",[](Program& p){p.Delete();});Bind(p,"Finalize",[](Program& p){p.Delete();});
	Bind(p,"SetUniform",[](Program& p,const std::string& name,sol::variadic_args args){
		Check(!p.deleted && args.size()==1,"Original Healthbars scalar invocation differs");const float value=args[0].as<float>();Check(std::isfinite(value),"Original Healthbars uniform is nonfinite");
		if(name=="iconDistance")p.iconDistance=value;else if(name=="cameraDistanceMult")p.cameraDistanceMult=value;else if(name=="cameraDistanceMultGlyph")p.cameraDistanceMultGlyph=value;
		else if(name=="skipGlyphsNumbers")p.skipGlyphsNumbers=value;else Check(false,"Unknown original Healthbars uniform");
	});
	auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(std::move(b),0);});Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(std::move(b),1);});Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.Attach(std::move(b),2);});
	Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.unusedIndex.reset();a.unusedInstance.reset();});
	Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Ready);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original Healthbars source is unsynced-only");const auto& sources=healthBarsProgram::Sources;
		const auto found=std::find_if(sources.begin(),sources.end(),[&](const auto& s){return s.path==path;});Check(found!=sources.end(),"Unknown original Healthbars source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source),"Original Healthbars archive source missing");
		Check(NativeUnitPrimitiveSourceHash(source)==found->hash,"Original Healthbars archive identity differs");return source;
	});
	Bind(family,"PrepareSource",[](std::string source){const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& sources=healthBarsProgram::Sources;
		Check(std::find_if(sources.begin(),sources.begin()+3,[&](const auto& s){return s.hash==hash;})!=sources.begin()+3,"Original Healthbars Lua identity differs");
		LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
	});
	gl.set("NativeHealthBars",family);return true;
}
}
#else
namespace LuaNativeHealthBars { bool PushEntries(lua_State*){return true;} }
#endif
