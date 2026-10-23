/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeUnitPrimitive.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaNativeUI.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaShaders.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeUnitPrimitiveData.h"
#include "Rendering/Backend/NativeUnitPrimitivePrograms.h"
#include "Rendering/Backend/NativeUnitPrimitive31657Programs.h"
#include "Rendering/Backend/NativeAllySelectedPrograms.h"
#include "Rendering/Backend/NativeUnitIconsPrograms.h"
#include "Rendering/Backend/NativeRankHaloPrograms.h"
#include "LuaAtlasTextures.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "System/Config/ConfigHandler.h"
#include <tuple>
#include "Rendering/Backend/NativeBlueprintData.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/FileSystem/FileHandler.h"
#include <charconv>
#include <cmath>
#include <map>
#include <optional>
namespace LuaNativeUnitPrimitive {
namespace {
using namespace rendering;
NativeUnitPrimitiveRecorder recorder;
std::optional<NativeUnitPrimitiveUniforms> uniforms;
void Check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename... Args>struct LuaBoundary<R(Args...)>{
	template<typename F>static auto Wrap(F function){return [function=std::move(function)](sol::this_state lua,Args... args)->R{
		try{return function(std::forward<Args>(args)...);}catch(const std::exception& error){lua_pushstring(lua,error.what());}lua_error(lua);std::terminate();};}
};
template<typename T,typename F>void Bind(T& table,const char* name,F function){table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));}
#include "NativeRankAtlasContext.h"
struct Program;struct Buffer;
struct Context {
	std::weak_ptr<Program> active;
	std::map<std::string,std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::shared_ptr<ui::NativeUITextureBinding>> images;
	std::shared_ptr<ui::NativeUITextureBinding> texture;
	std::string textureName;bool selected=false,source31657=false,ally=false,halo=false,tracker=false,enemy=false;int icon=-1;
	void ClearState(){active.reset();texture.reset();textureName.clear();}
};
std::vector<std::weak_ptr<Context>> contexts;
bool SelectedReady()
{
	const auto& textures=OriginalNativeTextureContext();
	return IsNativeUnitPrimitiveConsumerAvailable() && IsNativeBlueprintConsumerAvailable() &&
		textures.Initialized() && textures.UIAttached() && OriginalCUSModelPublication().Initialized() && OriginalCUSUniformCapture().Initialized();
}
struct Buffer:std::enable_shared_from_this<Buffer>{
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;
	Buffer(lua_State* L,GLenum target,bool frequent,bool source31657){
		// 31657 fillVAOSlots owns a real unused index buffer as well as its
		// unused attribute-15 instance buffer. Preserve those original calls.
		Check(target==GL_ARRAY_BUFFER || (source31657 && target==GL_ELEMENT_ARRAY_BUFFER),"Original DPAT source buffer target differs");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
};
struct Program:std::enable_shared_from_this<Program>{
	std::shared_ptr<Context> context;NativeUnitPrimitiveKind kind=NativeUnitPrimitiveKind::BuildingAO;
	float addRadius=0,iconDistance=20000,opacity=1,initialSize=0,whiteMix=0,lineOpacity=0;std::uint32_t maxVertices=64;bool deleted=false,disableQuads=false;
	void Activate(){Check(!deleted,"Deleted original unit primitive program");context->active=shared_from_this();}
	void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
	void Delete(){Deactivate();deleted=true;}
};
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,unusedIndex,unusedInstance;bool deleted=false;
	void AttachUnused(std::shared_ptr<Buffer> buffer,bool index){
		Check(context->source31657 && !deleted && buffer && buffer->context.lock()==context,"Original unit primitive placeholder owner differs");
		auto& slot=index?unusedIndex:unusedInstance;Check(!slot,"Original unit primitive placeholder slot already attached");
		const auto snapshot=buffer->value->CaptureNativeSnapshot();
		Check(snapshot && snapshot->layout.kind==(index?NativeBufferKind::Index:NativeBufferKind::Vertex),"Original unit primitive placeholder buffer target differs");
		if(!index){
			// The exact selected vertex shader reads locations 0..5; this
			// actual helper attachment fills only otherwise unused location 15.
			const auto& attributes=snapshot->layout.attributes;
			Check(attributes.size()==1 && attributes.front().id==15,"Original unit primitive unused instance attribute differs");
		}
		slot=std::move(buffer);
	}
	void Draw(GLenum primitive,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){
		Check(IsNativeUnitPrimitiveConsumerAvailable() && (context->icon>=0 || recorder.Active()) && !deleted && vertices,"Original unit primitive draw needs its paired frame and vertex stream");
		auto program=context->active.lock();Check(program && !program->deleted,"Original unit primitive active program missing");
		auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();
		const bool selected=IsNativeSelectedUnits(program->kind),icon=IsNativeUnitIcon(program->kind);
		const auto requiredStage=program->kind==NativeUnitPrimitiveKind::SelectedUnfinished || program->kind==NativeUnitPrimitiveKind::SelectedWater || context->halo?NativeWorldDrawStage::World:NativeWorldDrawStage::PreUnit;
		Check(icon ? (common.IsScreenStage() && !stage && !common.HasActiveTarget() && !common.IsCompiling()) : (stage && (context->ally?(*stage==NativeWorldDrawStage::PreUnit || *stage==NativeWorldDrawStage::World):*stage==requiredStage) && !common.HasActiveTarget()),"Original unit primitive draw must retain its actual source stage");
		if(selected || context->ally || icon || context->halo || context->tracker || context->enemy)Check(SelectedReady(),"Original selected unit draw lost its actual program/image/stencil services");
		Check(primitive==GL_POINTS && !instances && !base && count.value_or(0)>=0 && first.value_or(0)>=0,"Original unit primitive GS point-stream invocation differs");
		NativeUnitPrimitiveDraw draw;draw.screenEffects=icon;draw.stage=stage.value_or(NativeWorldDrawStage::World);draw.kind=program->kind;draw.addRadius=program->addRadius;draw.iconDistance=program->iconDistance;draw.opacity=program->opacity;draw.initialSize=program->initialSize;draw.whiteMix=program->whiteMix;draw.disableQuads=program->disableQuads;draw.lineOpacity=program->lineOpacity;draw.maxVertices=program->maxVertices;
		draw.vertices=vertices->value->CaptureNativeSnapshot();draw.first=first.value_or(0);draw.count=count.value_or(draw.vertices->layout.elements);
		draw.transforms=OriginalCUSModelPublication().Transforms();draw.modelUniforms=OriginalCUSModelPublication().ModelUniforms();
		if(icon || context->halo || context->tracker || context->enemy){
			auto& shared=OriginalNativeTextureContext();auto* observer=common.TextureObserver();Check(observer==&shared.UI(),"Original icon shared image observer missing");
			const auto sampled=observer->BoundImage(0);draw.atlas=sampled.image;draw.sampler=sampled.sampler;
		}else if(draw.kind!=NativeUnitPrimitiveKind::TeamPlatter && !context->ally){const auto index=draw.kind==NativeUnitPrimitiveKind::FeatureAO?1:0;
			Check(context->texture && context->textureName==(selected?unitPrimitiveProgram::SelectedTexture:unitPrimitiveProgram::AtlasNames[index]),"Original unit primitive source image binding differs");const auto resolved=context->texture->Resolve();draw.atlas=resolved.image;draw.sampler=resolved.sampler;}
		draw.surface=common.CurrentState().draw;draw.world=common.CurrentWorldState();
		if(icon){auto frame=std::make_shared<NativeUnitPrimitiveFrame>();frame->uniforms={globalRendering->drawFrame,OriginalCUSUniformCapture().Current()};frame->draws.push_back(std::move(draw));common.AppendUnitIcons(std::move(frame));}
		else recorder.Record(std::move(draw));
	}
};
std::map<std::string,std::string> ReadDefines(std::string_view text)
{
	std::map<std::string,std::string> result;
	while(!text.empty()){
		Check(text.starts_with("#define "),"Original unit primitive define syntax differs");text.remove_prefix(8);const auto space=text.find(' '),end=text.find('\n');
		Check(space!=std::string_view::npos && end!=std::string_view::npos && space<end,"Original unit primitive define line missing");
		Check(result.emplace(std::string(text.substr(0,space)),std::string(text.substr(space+1,end-space-1))).second,"Original unit primitive define repeated");text.remove_prefix(end+1);
	}
	return result;
}
template<typename Numbers,typename Texts>float ValidateDefines(const std::map<std::string,std::string>& source,const Numbers& numbers,const Texts& texts,bool mutableOpacity,bool selected=false)
{
	Check(source.size()==numbers.size()+texts.size(),"Original unit primitive compile variant has unknown/missing defines");float opacity=1;
	for(const auto& definition:numbers){auto it=source.find(std::string(definition.name));Check(it!=source.end(),"Original unit primitive numeric define missing");const auto& text=it->second;float value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
		Check(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && std::isfinite(value),"Original unit primitive numeric define malformed");
		if(definition.name=="TRANSPARENCY"){opacity=value;if(mutableOpacity)continue;}
		if(selected && definition.name=="TEAMCOLORIZATION")continue; // Original shader does not read this define; POST_SHADING uses its actual generated literal.
		Check(value==definition.value,"Original unit primitive compile variant differs");
	}
	for(const auto& definition:texts){auto it=source.find(std::string(definition.name));Check(it!=source.end() && it->second==definition.value,"Original unit primitive source macro differs");}
	return opacity;
}
float SelectedWhiteMix(const std::map<std::string,std::string>& defines)
{
	using namespace unitPrimitiveProgram;
	auto it=defines.find("POST_SHADING");Check(it!=defines.end(),"Original selected unit POST_SHADING missing");std::string_view text=it->second;
	Check(text.starts_with(SelectedShadingPrefix) && text.ends_with(SelectedShadingSuffix),"Original selected unit POST_SHADING source differs");
	text.remove_prefix(SelectedShadingPrefix.size());text.remove_suffix(SelectedShadingSuffix.size());float mix=0;
	const auto parsed=std::from_chars(text.data(),text.data()+text.size(),mix);
	Check(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && std::isfinite(mix),"Original selected unit white-mix literal malformed");return mix;
}
sol::table CreateContext(lua_State* L,bool selected=false,bool source31657=false,bool ally=false,int icon=-1,bool halo=false,bool tracker=false,bool enemy=false)
{
	Check(!CLuaHandle::GetHandleSynced(L) && IsNativeUnitPrimitiveConsumerAvailable() && (!(selected || ally || icon>=0 || halo || tracker || enemy) || SelectedReady()),"Native unit primitives require the unsynced paired consumer");
	sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();std::erase_if(contexts,[](const auto& p){return p.expired();});contexts.emplace_back(context);context->selected=selected;context->source31657=source31657;context->ally=ally;context->icon=icon;context->halo=halo;context->tracker=tracker;context->enemy=enemy;
	if(selected){
		// Expose the exact original CPU callouts only to this private table.
		// Named image loading remains at the original Texture call below;
		// connected consumer services do not imply decoded/completed contents.
		sol::stack::push(L,table);LuaShaders::PushModelBufferUniformEntries(L);lua_pop(L,1);
	}
	Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false),context->source31657);b->context=context;return b;});
	Bind(table,"GetVAO",[context](){auto vao=std::make_shared<VertexArray>();vao->context=context;return vao;});
	auto createProgram=[context](const std::string& name,const std::string& vertex,const std::string& geometry,const std::string& fragment,const std::string& engine,const std::string& quaternion,const std::string& definitions,const sol::table& floats,const sol::table& samplers){
		using namespace unitPrimitiveProgram;auto found=std::find(Names.begin(),Names.end(),name);Check(context->enemy?name=="enemyspotterShader GL4":context->tracker?name=="unitTrackerShader GL4":context->halo?name=="ResurrectionHalosShader GL4":context->icon==6?name=="Rank IconsShader GL4":context->icon>=0?name==unitIconsProgram::Names.at(context->icon):(context->ally?name==allySelectedProgram::Name:found!=Names.end()),"Unknown original unit primitive program");
		if(context->source31657){
			Check(context->enemy || context->tracker || context->halo || context->icon>=0 || context->ally || found-Names.begin()<3,"The 31657 profile only owns building AO, feature AO and TeamPlatter");
			Check(NativeUnitPrimitiveSourceHash(vertex)==unitPrimitive31657Program::VertexHash && NativeUnitPrimitiveSourceHash(geometry)==unitPrimitive31657Program::GeometryHash && NativeUnitPrimitiveSourceHash(fragment)==unitPrimitive31657Program::FragmentHash && NativeUnitPrimitiveSourceHash(quaternion)==unitPrimitive31657Program::QuaternionHash,"Original 31657 unit primitive shader/quaternion source identity differs");
			// Same UBO layout; the new original helper no longer appends
			// the unused map-specific waterBlend definitions to this string.
			Check(NativeUnitPrimitiveSourceHash(engine)==unitPrimitive31657Program::EngineDefinitionsHash,"Original 31657 unit primitive engine definitions differ");
		}else{
			Check(NativeUnitPrimitiveSourceHash(vertex)==VertexHash && NativeUnitPrimitiveSourceHash(geometry)==GeometryHash && NativeUnitPrimitiveSourceHash(fragment)==FragmentHash && NativeUnitPrimitiveSourceHash(quaternion)==QuaternionHash,"Original unit primitive shader/quaternion source identity differs");
			ValidateNativeUnitPrimitiveEngineDefinitions(engine);
		}
		auto program=std::make_shared<Program>();program->context=context;program->kind=context->enemy?NativeUnitPrimitiveKind::EnemySpotter:context->tracker?NativeUnitPrimitiveKind::UnitTrackerDebug:context->halo?NativeUnitPrimitiveKind::ResurrectionHalo:context->icon>=0?NativeUnitPrimitiveKind(unsigned(NativeUnitPrimitiveKind::EnergyIcon)+context->icon):(context->ally?NativeUnitPrimitiveKind::AllySelected:NativeUnitPrimitiveKind(found-Names.begin()));
		Check(IsNativeSelectedUnits(program->kind)==context->selected,"Original unit primitive program belongs to another private family");
		const auto defines=ReadDefines(definitions);
		switch(program->kind){
		case NativeUnitPrimitiveKind::EnemySpotter:program->opacity=ValidateDefines(defines,unitPrimitive31657Program::EnemySpotterNumbers,TeamTexts,true);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::UnitTrackerDebug:program->opacity=ValidateDefines(defines,unitPrimitive31657Program::UnitTrackerNumbers,TeamTexts,false);break;
		case NativeUnitPrimitiveKind::RankIcon:program->opacity=ValidateDefines(defines,rankHaloProgram::RankNumbers,rankHaloProgram::RankTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::ResurrectionHalo:program->opacity=ValidateDefines(defines,rankHaloProgram::HaloNumbers,rankHaloProgram::HaloTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::EnergyIcon: program->opacity=ValidateDefines(defines,unitIconsProgram::EnergyIconNumbers,unitIconsProgram::EnergyIconTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::FireStateIcon: program->opacity=ValidateDefines(defines,unitIconsProgram::FireStateIconNumbers,unitIconsProgram::FireStateIconTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::GroupNumber: program->opacity=ValidateDefines(defines,unitIconsProgram::GroupNumberNumbers,unitIconsProgram::GroupNumberTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::IdleBuilderIcon: program->opacity=ValidateDefines(defines,unitIconsProgram::IdleBuilderIconNumbers,unitIconsProgram::IdleBuilderIconTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::RepeatIcon: program->opacity=ValidateDefines(defines,unitIconsProgram::RepeatIconNumbers,unitIconsProgram::RepeatIconTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::WaitIcon: program->opacity=ValidateDefines(defines,unitIconsProgram::WaitIconNumbers,unitIconsProgram::WaitIconTexts,false);program->maxVertices=4;break;
		case NativeUnitPrimitiveKind::AllySelected:{
			auto numbers=allySelectedProgram::Numbers;
			for(auto& number:numbers)if(number.name=="LINETRANSPARANCY" || number.name=="MAXVERTICES"){
				auto it=defines.find(std::string(number.name));Check(it!=defines.end(),"Original ally selected definition missing");
				float value=0;const auto& text=it->second;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
				Check(parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() && std::isfinite(value),"Original ally selected definition malformed");
				if(number.name=="MAXVERTICES"){Check(value==8 || value==64,"Original ally selected shape limit differs");program->maxVertices=unsigned(value);}else program->lineOpacity=value;
				number.value=value;
			}
			program->opacity=ValidateDefines(defines,numbers,allySelectedProgram::Texts,true);
			program->disableQuads=true;program->initialSize=.75f;break;
		}
		case NativeUnitPrimitiveKind::BuildingAO:program->opacity=ValidateDefines(defines,BuildingNumbers,BuildingTexts,false);break;
		case NativeUnitPrimitiveKind::FeatureAO:program->opacity=ValidateDefines(defines,FeatureNumbers,FeatureTexts,false);break;
		case NativeUnitPrimitiveKind::TeamPlatter:
			program->disableQuads=context->source31657;
			program->opacity=context->source31657?ValidateDefines(defines,unitPrimitive31657Program::TeamNumbers,TeamTexts,true):ValidateDefines(defines,TeamNumbers,TeamTexts,true);
			break;
		case NativeUnitPrimitiveKind::SelectedGround:case NativeUnitPrimitiveKind::SelectedUnfinished:case NativeUnitPrimitiveKind::SelectedWater:{
			auto texts=TeamTexts;program->whiteMix=SelectedWhiteMix(defines);
			// The precise POST_SHADING prefix/suffix and only its numeric slot
			// were checked above; all other original macros remain exact.
			for(auto& t:texts)if(t.name=="POST_SHADING")t.value=defines.at("POST_SHADING");
			const bool unfinished=program->kind==NativeUnitPrimitiveKind::SelectedUnfinished;
			program->opacity=ValidateDefines(defines,unfinished?SelectedUnfinishedNumbers:SelectedNumbers,texts,true,true);
			program->initialSize=unfinished?.9999f:.75f;break;
		}}

		unsigned floatCount=0;for(const auto& entry:floats){const auto key=entry.first.as<std::string>();Check((key=="addRadius" && entry.second.as<float>()==0) || (key=="iconDistance" && entry.second.as<float>()==20000),"Original unit primitive initial float uniform differs");++floatCount;}Check(floatCount==2,"Original unit primitive initial uniforms missing");
		// Lua treats numeric zero as truthy, so USETEXTURE=0 still supplies this
		// unused sampler declaration in the original team-platter helper.
		unsigned samplerCount=0;for(const auto& entry:samplers){Check(entry.first.as<std::string>()=="DrawPrimitiveAtUnitTexture" && entry.second.as<int>()==0,"Original unit primitive sampler declaration differs");++samplerCount;}Check(samplerCount==1,"Original unit primitive sampler declaration missing");return program;
	};
	Bind(table,"CreateProgram",createProgram);Bind(table,"CreateShader",createProgram);
	if(selected){
		Bind(table,"Texture",[context](int unit,sol::object value){
			Check(unit==0,"Original selected unit shader samples texture zero");
			// The wrapper already executed public gl.Texture exactly once. A
			// false argument only disabled the actual target: retain its name.
			if(value.is<bool>() && !value.as<bool>())return true;
			if(!value.is<bool>())Check(value.as<std::string>()==unitPrimitiveProgram::SelectedTexture,"Unknown original selected unit source texture");
			auto& shared=OriginalNativeTextureContext();auto& common=ui::GetRecorder();auto* observer=common.TextureObserver();
			Check(shared.Initialized() && shared.UIAttached() && observer==&shared.UI() && !common.IsCompiling(),"Original selected texture has no actual shared binding observer");
			// Shader sampler0 reads actual unit0, even if a zero-request public
			// operation used another inherited active unit. Never force/rebind it.
			const auto observed=observer->BoundImage(0);
			Check(bool(observed.image),"Original selected texture public event has no observed image owner");
			// An actual default/empty object stays incomplete. Enabling it is
			// still a valid state event; existing draw validation rejects sampling.
			if(!context->texture)context->texture=std::make_shared<ui::NativeUITextureBinding>();
			context->texture->current=observed.image;context->texture->sampler=observed.sampler;
			context->textureName=observed.image->sourceName;return true;
		});
	}else{
		Bind(table,"Texture",[L,context](int unit,sol::object value){Check(unit==0,"Original unit primitive atlas uses texture zero");if(value.is<bool>()){Check(!value.as<bool>(),"Original unit primitive texture enable has no image");return true;}
			const auto name=value.as<std::string>();Check(std::find(unitPrimitiveProgram::AtlasNames.begin(),unitPrimitiveProgram::AtlasNames.end(),name)!=unitPrimitiveProgram::AtlasNames.end(),"Unknown original unit primitive source texture");
			auto& image=context->images[name];if(!image)image=LuaNativeUI::ResolveTextureBinding(L,name);if(!image)return false;context->texture=image;context->textureName=name;return true;
		});
	}
	if(icon>=0 || halo || tracker || enemy)table.set("Texture",sol::lua_nil);
	if(icon==6)rankAtlas::Install(L,table);
	Bind(table,"ObjectLabel",[context](GLenum type,const std::string& identity,const std::string& label){Check(type==0x82e0 && label.size()<=1024,"Original unit primitive buffer label invalid");auto it=context->buffers.find(identity);Check(it!=context->buffers.end(),"Unknown original unit primitive buffer identity");auto b=it->second.lock();Check(bool(b),"Deleted original unit primitive buffer identity");b->label=label;});return table;
}
}
void CaptureUniforms(){NativeUnitPrimitiveUniforms value{globalRendering->drawFrame,OriginalCUSUniformCapture().Current()};(void)ReadNativeUnitPrimitiveUniforms(value);uniforms=value;}
void BeginWorldFrame(){Check(uniforms && uniforms->sourceDrawFrame==globalRendering->drawFrame,"Original unit primitive UBO-stage capture is missing/stale");recorder.Begin(*uniforms);}
std::shared_ptr<const NativeUnitPrimitiveFrame> EndWorldFrame(){return recorder.End();}
void AbortWorldFrame()noexcept{recorder.Abort();for(auto& weak:contexts)if(auto c=weak.lock())c->ClearState();}
void Reset(){AbortWorldFrame();uniforms.reset();for(auto& weak:contexts)if(auto c=weak.lock())c->images.clear();}
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
		const bool transparency=p.context->source31657 && (p.kind==NativeUnitPrimitiveKind::TeamPlatter || p.kind==NativeUnitPrimitiveKind::EnemySpotter) && name=="transparency";
		Check(!p.deleted && (name=="addRadius" || name=="iconDistance" || transparency) && args.size()==1,"Original unit primitive scalar uniform differs");
		const float value=args[0].as<float>();Check(std::isfinite(value),"Original unit primitive scalar is nonfinite");
		if(name=="addRadius")p.addRadius=value;else if(name=="iconDistance")p.iconDistance=value;else p.opacity=value;
	});
	auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted && b,"Original unit primitive vertex attachment missing");a.vertices=std::move(b);});Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.unusedIndex.reset();a.unusedInstance.reset();});
	Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.AttachUnused(std::move(b),true);});
	Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){a.AttachUnused(std::move(b),false);});
	Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&IsNativeUnitPrimitiveConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"SelectedAvailable",&SelectedReady);Bind(family,"CreateSelectedContext",[L](){return CreateContext(L,true);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Original unit primitive source is unsynced-only");
		constexpr std::array<std::string_view,10> names{"LuaUI/Widgets/gui_selectedunits_gl4.lua","LuaUI/Widgets/gui_ground_ao_plates_gl4.lua","LuaUI/Widgets/gui_ground_ao_plates_features_gl4.lua","LuaUI/Widgets/gui_team_platter.lua","LuaUI/Widgets/api_unit_tracker_gl4.lua","LuaUI/Include/DrawPrimitiveAtUnit.lua","modules/graphics/instancevbotable.lua","modules/graphics/luashader.lua","unittextures/decals/unitaoplates_atlas.lua","unittextures/decals_features/featureaoplates_atlas.lua"};Check(std::find(names.begin(),names.end(),path)!=names.end(),"Unknown original unit primitive archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source),"Original unit primitive archive source missing");return source;
	});
	Bind(family,"PrepareSource",[](std::string source){using namespace unitPrimitiveProgram;constexpr std::array hashes{SelectedWidgetHash,BuildingWidgetHash,FeatureWidgetHash,TeamWidgetHash,UnitTrackerWidgetHash,PrimitiveHelperHash,InstanceHelperHash,ShaderHelperHash,UnitAtlasHash,FeatureAtlasHash};const auto hash=NativeUnitPrimitiveSourceHash(source);Check(std::find(hashes.begin(),hashes.end(),hash)!=hashes.end(),"Original unit primitive archive identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	Bind(family,"RankHaloAvailable",&SelectedReady);Bind(family,"CreateRankHaloContext31657",[L](int profile){Check(profile==0 || profile==1,"Unknown original rank/halo family");return CreateContext(L,false,true,false,profile==0?6:-1,profile==1);});
	Bind(family,"LoadRankHaloArchiveSource31657",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Original rank/halo source is unsynced-only");const auto& sources=rankHaloProgram::Sources;const auto found=std::find_if(sources.begin(),sources.end(),[&](const auto& e){return e.path==path;});Check(found!=sources.end(),"Unknown original rank/halo archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string text;Check(file.FileExists() && file.LoadStringData(text) && NativeUnitPrimitiveSourceHash(text)==found->hash,"Original rank/halo archive identity differs");return text;});
	Bind(family,"PrepareRankHaloSource31657",[](std::string text){const auto& sources=rankHaloProgram::Sources;const auto hash=NativeUnitPrimitiveSourceHash(text);Check(std::find_if(sources.begin(),sources.end(),[&](const auto& e){return e.lua && e.hash==hash;})!=sources.end(),"Original rank/halo Lua identity differs");LuaUtils::TracyRemoveAlsoExtras(text.data());return text;});
	Bind(family,"IconsAvailable",&SelectedReady);Bind(family,"CreateIconContext31657",[L](int icon){Check(icon>=0 && icon<6,"Unknown original icon family");return CreateContext(L,false,true,false,icon);});
	Bind(family,"LoadIconArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original unit icon source is unsynced-only");const auto& sources=unitIconsProgram::Sources;
		const auto found=std::find_if(sources.begin(),sources.end(),[&path](const auto& entry){return entry.path==path;});Check(found!=sources.end(),"Unknown unit icon archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source) && NativeUnitPrimitiveSourceHash(source)==found->hash,"Original unit icon archive identity differs");return source;
	});
	Bind(family,"PrepareIconSource31657",[](std::string source){const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& sources=unitIconsProgram::Sources;Check(std::find_if(sources.begin(),sources.end(),[hash](const auto& e){return e.lua && e.hash==hash;})!=sources.end(),"Original unit icon Lua identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	Bind(family,"AllyAvailable",&SelectedReady);Bind(family,"CreateAllyContext31657",[L](){return CreateContext(L,false,true,true);});
	Bind(family,"LoadAllyArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original ally selected source is unsynced-only");const auto& sources=allySelectedProgram::Sources;
		const auto found=std::find_if(sources.begin(),sources.end(),[&path](const auto& entry){return entry.path==path;});Check(found!=sources.end(),"Unknown ally selected archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists() && file.LoadStringData(source) && NativeUnitPrimitiveSourceHash(source)==found->hash,"Original ally selected archive identity differs");return source;
	});
	Bind(family,"PrepareAllySource31657",[](std::string source){const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& sources=allySelectedProgram::Sources;Check(std::find_if(sources.begin(),sources.end(),[hash](const auto& e){return e.lua && e.hash==hash;})!=sources.end(),"Original ally selected Lua identity differs");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	Bind(family,"CreateEnemyContext31657",[L](){return CreateContext(L,false,true,false,-1,false,false,true);});
	Bind(family,"CreateTrackerContext31657",[L](){return CreateContext(L,false,true,false,-1,false,true);});
	Bind(family,"CreateContext31657",[L](){return CreateContext(L,false,true);});
	Bind(family,"LoadArchiveSource31657",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Original unit primitive source is unsynced-only");
		const auto& sources=unitPrimitive31657Program::Sources;
		const auto found=std::find_if(sources.begin(),sources.end(),[&path](const auto& entry){return entry.path==path;});
		Check(found!=sources.end(),"Unknown 31657 unit primitive archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;
		Check(file.FileExists() && file.LoadStringData(source),"Original 31657 unit primitive archive source missing");
		Check(NativeUnitPrimitiveSourceHash(source)==found->hash,"Original 31657 unit primitive archive identity differs");return source;
	});
	Bind(family,"PrepareSource31657",[](std::string source){
		const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& sources=unitPrimitive31657Program::Sources;
		Check(std::find_if(sources.begin(),sources.end(),[hash](const auto& entry){return entry.lua && entry.hash==hash;})!=sources.end(),"Original 31657 unit primitive Lua source identity differs");
		LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
	});
	gl.set("NativeUnitPrimitive",family);return true;
}
}
#else
namespace LuaNativeUnitPrimitive {
void CaptureUniforms(){}void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeUnitPrimitiveFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
