/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeGameDecal.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaNativeUI.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeGameDecalData.h"
#include "Rendering/Backend/NativeGameDecalPrograms.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <map>
#include <optional>

namespace LuaNativeGameDecal {
namespace {
using namespace rendering;
NativeGameDecalRecorder recorder;
std::optional<NativeGameDecalUniforms> uniforms;
void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<typename Signature>struct LuaBoundary;
template<typename R,typename... Args>struct LuaBoundary<R(Args...)>{
	template<typename F>static auto Wrap(F function){
		return [function=std::move(function)](sol::this_state lua,Args... args)->R {
			try{return function(std::forward<Args>(args)...);}
			catch(const std::exception& error){lua_pushstring(lua,error.what());}
			lua_error(lua);std::terminate();
		};
	}
};
template<typename T,typename F>void Bind(T& table,const char* name,F function)
{ table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function))); }
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::shared_ptr<ui::NativeUITextureBinding>> images;
	std::array<std::string,9> textureNames;
	std::array<std::shared_ptr<ui::NativeUITextureBinding>,9> textures;
	void ClearState(){active.reset();textureNames={};textures={};}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer>{
	std::shared_ptr<LuaVBOImpl> value;
	std::weak_ptr<Context> context;
	std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent){
		Check(target==GL_ARRAY_BUFFER || target==GL_ELEMENT_ARRAY_BUFFER,"Original game decals only use vertex/index buffers");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
};
struct Program:std::enable_shared_from_this<Program>{
	std::shared_ptr<Context> context;
	NativeGameDecalProgram kind=NativeGameDecalProgram::Geometry;
	NativeGameDecalWater water;
	float fadeDistance=3000;
	bool deleted=false;
	void Activate(){Check(!deleted,"Deleted original game decal program");context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Delete(){Deactivate();deleted=true;}
};
struct VertexArray {
	std::shared_ptr<Context> context;
	std::shared_ptr<Buffer> vertices,indices,instances;
	bool deleted=false;
	void Draw(bool indexed,GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> firstInstance){
		Check(IsNativeGameDecalConsumerAvailable() && recorder.Active() && !deleted && vertices,"Original game decal draw needs its connected frame and vertex stream");
		auto program=context->active.lock();Check(program && !program->deleted,"Original game decal active program missing");
		auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();
		Check(stage && *stage==NativeWorldDrawStage::PreDecals && !common.HasActiveTarget(),"Original game decal draw must retain the original main DrawPreDecals stage");
		NativeGameDecalDraw draw;draw.stage=*stage;draw.program=program->kind;draw.water=program->water;draw.fadeDistance=program->fadeDistance;
		draw.vertices=vertices->value->CaptureNativeSnapshot();
		if(indexed){
			Check(program->kind==NativeGameDecalProgram::LargeMesh && primitive==GL_TRIANGLES && indices && instances,"Original game decal mesh program or attachments differ");
			draw.indices=indices->value->CaptureNativeSnapshot();draw.instances=instances->value->CaptureNativeSnapshot();
			draw.count=count.value_or(draw.indices->layout.elements);draw.instanceCount=instanceCount.value_or(0);
		}else{
			Check(program->kind==NativeGameDecalProgram::Geometry && primitive==GL_POINTS && indices && instances && !instanceCount && !firstInstance,"Original game decal GS must consume its actual point stream and placeholder attachments");
			// The complete31657 helper fills otherwise-unused VAO slots. These
			// real objects remain attached, but POINTS DrawArrays does not read
			// their index or attribute15 values. Validate the exact placeholders
			// rather than dropping the objects or treating them as drawn rows.
			const auto index=indices->value->CaptureNativeSnapshot();
			const auto instance=instances->value->CaptureNativeSnapshot();
			for(const auto& snapshot:{index,instance}) {
				Check(snapshot && snapshot->bytes.size()==CheckedNativeBufferByteSize(snapshot->layout),"Original game decal placeholder storage differs");
				Check(snapshot->layout.elements==1 && snapshot->layout.attributes.size()==1 &&
					std::all_of(snapshot->bytes.begin(),snapshot->bytes.end(),[](uint8_t byte){return byte==0;}),"Original game decal placeholder extent/value differs");
			}
			const auto& ia=index->layout.attributes[0];const auto& va=instance->layout.attributes[0];
			Check(index->layout.kind==NativeBufferKind::Index && index->layout.stride==2 && ia.id==0 && ia.scalarType==NativeScalarType::UInt16,
				"Original game decal unused index slot differs");
			Check(instance->layout.kind==NativeBufferKind::Vertex && instance->layout.stride==4 && va.id==15 && va.offset==0 &&
				va.declaredCount==1 && va.byteSize==4 && va.scalarType==NativeScalarType::Float32 && va.valueShape==NativeValueShape::Scalar && !va.normalized,
				"Original game decal unused instance slot differs");
			draw.count=count.value_or(draw.vertices->layout.elements);draw.instanceCount=1;
		}
		Check(count.value_or(0)>=0 && first.value_or(0)>=0 && instanceCount.value_or(0)>=0 && firstInstance.value_or(0)>=0,"Original game decal negative draw range");
		draw.first=first.value_or(0);draw.firstInstance=firstInstance.value_or(0);
		constexpr std::array<std::string_view,5> engineNames{"$heightmap","$minimap","$info","$shadow","$normals"};
		for(unsigned i=0;i<engineNames.size();++i)Check(context->textureNames[i]==engineNames[i],"Original game decal engine-image slot differs");
		for(unsigned i=0;i<2;++i){
			Check(context->textureNames[i+5]==gameDecalProgram::AtlasNames[i] && bool(context->textures[i+5]),"Original game decal atlas image slot differs");
			auto resolved=context->textures[i+5]->Resolve();draw.atlases[i]={resolved.image,resolved.sampler};
		}
		draw.surface=common.CurrentState().draw;draw.world=common.CurrentWorldState();
		recorder.Record(std::move(draw));
	}
};
sol::table CreateContext(lua_State* L)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeGameDecalConsumerAvailable(),"Native game decals require the unsynced paired consumer");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();
	std::erase_if(contexts,[](const auto& entry){return entry.expired();});contexts.emplace_back(context);
	Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));b->context=context;return b;});
	Bind(table,"GetVAO",[context](){auto vao=std::make_shared<VertexArray>();vao->context=context;return vao;});
	Bind(table,"CreateProgram",[context](const std::string& name,const std::string& vertex,const std::string& geometry,const std::string& fragment,const std::string& engine,const sol::table& defines,const sol::table& floats,const sol::table& samplers){
		const bool small=name=="Decals Gl4 Shader";
		Check(small || name=="Decals Large Gl4 Shader","Unknown original game decal program name");
		Check(NativeGameDecalSourceHash(vertex)==(small?gameDecalProgram::SmallVertexHash:gameDecalProgram::LargeVertexHash) &&
			NativeGameDecalSourceHash(fragment)==gameDecalProgram::FragmentHash &&
			(small?NativeGameDecalSourceHash(geometry)==gameDecalProgram::GeometryHash:geometry.empty()),"Original game decal shader source differs from the actual native family");
		std::bitset<11> seen;
		for(const auto& pair:defines){
			const auto key=pair.first.as<std::string>();auto it=std::find(gameDecalProgram::DefineNames.begin(),gameDecalProgram::DefineNames.end(),key);
			Check(it!=gameDecalProgram::DefineNames.end(),"Unknown original game decal define");const auto index=it-gameDecalProgram::DefineNames.begin();
			Check(pair.second.as<float>()==gameDecalProgram::DefineValues[index],"Original game decal compile variant differs");seen[index]=true;
		}
		Check(seen.all(),"Original game decal required defines missing");
		std::bitset<8> images;
		for(const auto& pair:samplers){
			const auto key=pair.first.as<std::string>();const int slot=pair.second.as<int>();
			if(key=="atlasRG"){Check(slot==9,"Original game decal unused glow sampler declaration differs");images[7]=true;continue;}
			auto it=std::find(gameDecalProgram::SamplerNames.begin(),gameDecalProgram::SamplerNames.end(),key);
			Check(it!=gameDecalProgram::SamplerNames.end() && slot==it-gameDecalProgram::SamplerNames.begin(),"Original game decal sampler declaration differs");images[slot]=true;
		}
		Check(images.all(),"Original game decal sampler declaration missing");
		unsigned floatCount=0;for(const auto& pair:floats){Check(pair.first.as<std::string>()=="fadeDistance" && pair.second.as<float>()==3000,"Original game decal initial float uniform differs");++floatCount;}
		Check(floatCount==1,"Original game decal initial float uniform missing");
		auto program=std::make_shared<Program>();program->context=context;program->kind=small?NativeGameDecalProgram::Geometry:NativeGameDecalProgram::LargeMesh;program->water=ParseNativeGameDecalWater(engine);return program;
	});
	Bind(table,"Texture",[L,context](int unit,sol::object value){
		Check(unit>=0 && unit<=8,"Original game decal texture unit differs");
		if(value.is<bool>()){Check(!value.as<bool>(),"Original game decal texture enable has no image");return true; /* GLSL retains the bound texture after fixed-function disable. */}
		const auto name=value.as<std::string>();
		constexpr std::array<std::string_view,5> engineNames{"$heightmap","$minimap","$info","$shadow","$normals"};
		if(std::find(engineNames.begin(),engineNames.end(),name)!=engineNames.end()){
			Check(unit<5 && engineNames[unit]==name,"Original game decal engine image bound to wrong slot");context->textures[unit].reset();context->textureNames[unit]=name;return true;
		}
		Check(std::find(gameDecalProgram::AtlasNames.begin(),gameDecalProgram::AtlasNames.end(),name)!=gameDecalProgram::AtlasNames.end(),"Unknown original game decal named image");
		auto& image=context->images[name];if(!image)image=LuaNativeUI::ResolveTextureBinding(L,name);
		if(!image)return false; // Original failed bind retains the previous texture.
		context->textures[unit]=image;context->textureNames[unit]=name;return true;
	});
	Bind(table,"ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label){
		Check(type==0x82e0 && label.size()<=1024,"Original game decal buffer label invalid");auto it=context->buffers.find(identity);Check(it!=context->buffers.end(),"Unknown original game decal buffer identity");auto buffer=it->second.lock();Check(bool(buffer),"Deleted original game decal buffer identity");buffer->label=label;
	});
	return table;
}
}
PiPBuffer BorrowPiPBuffer(lua_State* L,int stackIndex)
{
	if(!L || lua_type(L,stackIndex)!=LUA_TUSERDATA)return {};
	const sol::stack_object value(L,stackIndex);
	if(!value.is<std::shared_ptr<Buffer>>())return {};
	auto buffer=value.as<std::shared_ptr<Buffer>>();
	if(!buffer || !buffer->value)return {};
	return {buffer,buffer->value};
}
void CaptureUniforms()
{
	NativeGameDecalUniforms value{globalRendering->drawFrame,OriginalCUSUniformCapture().Current()};
	ValidateNativeGameDecalUniforms(value);uniforms=value;
}
void BeginWorldFrame(){Check(uniforms && uniforms->sourceDrawFrame==globalRendering->drawFrame,"Original game decal UBO-stage capture is missing or stale");recorder.Begin(*uniforms);}
std::shared_ptr<const rendering::NativeGameDecalFrame> EndWorldFrame(){return recorder.End();}
void AbortWorldFrame() noexcept {recorder.Abort();for(auto& weak:contexts)if(auto context=weak.lock())context->ClearState();}
void Reset(){AbortWorldFrame();uniforms.reset();for(auto& weak:contexts)if(auto context=weak.lock())context->images.clear();}
bool PushEntries(lua_State* L)
{
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	auto buffer=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(buffer,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);});
	Bind(buffer,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.value->Upload(data,attribute,offset,first,last);});
	Bind(buffer,"Delete",[](Buffer& b){b.value->Delete();});Bind(buffer,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});
	Bind(buffer,"GetID",[](Buffer& b){return b.Identity();});Bind(buffer,"GetLabel",[](Buffer& b){return b.label;});
	auto program=family.new_usertype<Program>("Program",sol::no_constructor);
	Bind(program,"Activate",[](Program& p){p.Activate();});Bind(program,"Deactivate",[](Program& p){p.Deactivate();});Bind(program,"Delete",[](Program& p){p.Delete();});Bind(program,"Finalize",[](Program& p){p.Delete();});
	Bind(program,"SetUniform",[](Program& p,const std::string& name,sol::variadic_args args){
		Check(!p.deleted && name=="fadeDistance" && args.size()==1,"Original game decal uniform name or arity differs");const float value=args[0].as<float>();Check(std::isfinite(value),"Original game decal uniform nonfinite");p.fadeDistance=value;
	});
	auto vao=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(vao,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && bool(b),"Original game decal vertex attachment missing");a.vertices=std::move(b);});
	Bind(vao,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && bool(b),"Original game decal index attachment missing");a.indices=std::move(b);});
	Bind(vao,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && bool(b),"Original game decal instance attachment missing");a.instances=std::move(b);});
	Bind(vao,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.indices.reset();a.instances.reset();});
	Bind(vao,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(false,mode,count,first,instances,base);});
	Bind(vao,"DrawElements",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(true,mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);
	Bind(family,"Available",&IsNativeGameDecalConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original game decal source is unsynced-only");
		Check(path=="LuaUI/Widgets/gfx_decals_gl4.lua" || path=="modules/graphics/instancevbotable.lua" || path=="modules/graphics/luashader.lua","Unknown original game decal archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source),"Original game decal archive source missing");return source;
	});
	Bind(family,"PrepareSource",[](std::string source){const auto hash=NativeGameDecalSourceHash(source);Check(hash==gameDecalProgram::WidgetHash || hash==gameDecalProgram::InstanceHelperHash || hash==gameDecalProgram::ShaderHelperHash,"Original game decal archive identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeGameDecal",family);return true;
}
}
#else
namespace LuaNativeGameDecal {
PiPBuffer BorrowPiPBuffer(lua_State*,int){return {};}
void CaptureUniforms(){}void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeGameDecalFrame> EndWorldFrame(){return {};}
void AbortWorldFrame() noexcept {}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
