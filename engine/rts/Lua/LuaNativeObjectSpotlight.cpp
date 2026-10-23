/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeObjectSpotlight.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeObjectSpotlightPrograms.h"
#include "Rendering/Backend/NativeCommanderWreckPrograms.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include <map>
namespace LuaNativeObjectSpotlight {
namespace {
using namespace rendering;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct Boundary;
template<typename R,typename...Args>struct Boundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
	try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;struct Program;
struct Context {
	bool released=false,commanderWreck=false;
	unsigned next=1;
	std::shared_ptr<Program> active;
	std::vector<std::weak_ptr<Buffer>> buffers;
	std::map<std::string,std::weak_ptr<Buffer>> bufferIDs;
	std::map<unsigned,std::weak_ptr<Program>> programs;
	std::vector<std::string> groups;
};
void Live(const std::shared_ptr<Context>& c){Check(c&&!c->released&&IsNativeObjectSpotlightConsumerAvailable(),"Object Spotlight actual context/compiled consumer was retired");}
struct Buffer:std::enable_shared_from_this<Buffer> {
	std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;bool deleted=false;
	Buffer(lua_State* L,GLenum target,bool frequent,const std::shared_ptr<Context>& c):context(c){
		Check(target==GL_ARRAY_BUFFER||(c->commanderWreck&&target==GL_ELEMENT_ARRAY_BUFFER),"Object Spotlight only consumes original vertex/instance buffers");
		value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
	}
	void CheckLive(){Live(context.lock());Check(!deleted,"Object Spotlight actual buffer was deleted");}
	void Delete(){if(deleted)return;value->Delete();deleted=true;}
	std::string Identity(){CheckLive();auto id=std::to_string(value->CaptureNativeSnapshot()->object);context.lock()->bufferIDs[id]=shared_from_this();return id;}
};
struct Program {
	unsigned identity=0;bool deleted=false,commanderWreck=false;std::string label;
	std::shared_ptr<const void> compiledFamily;
};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){
	Live(c);auto p=c->active;Check(p&&!p->deleted&&p->compiledFamily==NativeObjectSpotlightConsumer()&&p->commanderWreck==c->commanderWreck,"Object Spotlight active program lacks the current actual compiled family");return p;
}
struct VertexArray {
	std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,unusedIndex;bool deleted=false;
	void Draw(GLenum mode,int count,int first,int instanceCount,int base){
		const auto p=Active(context);Check(!deleted&&vertices&&instances,"Object Spotlight VAO/attachments missing");vertices->CheckLive();instances->CheckLive();
		Check(mode==GL_TRIANGLE_STRIP&&count==(context->commanderWreck?34:66)&&first==0&&instanceCount>=0&&base==0,"Object Spotlight original DrawArrays arguments differ");if(!instanceCount)return;
		auto& common=ui::GetRecorder();Check(common.CurrentWorldStage()==NativeWorldDrawStage::World&&!common.IsCompiling()&&!common.HasActiveTarget(),"Object Spotlight draw requires its original main DrawWorld callin");
		auto d=std::make_shared<NativeObjectSpotlightInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->commanderWreck=context->commanderWreck;
  if(context->commanderWreck){Check(bool(unusedIndex),"Original Commander Wreck placeholder index missing");unusedIndex->CheckLive();}
		d->vertices=vertices->value->CaptureNativeSnapshot();d->instances=instances->value->CaptureNativeSnapshot();
		d->original=OriginalCUSUniformCapture().Current();if(!context->commanderWreck)d->models=OriginalCUSModelPublication().ModelUniforms();
		d->compiledFamily=p->compiledFamily;d->vertexHash=context->commanderWreck?commanderwreck::VertexHash:objectspotlight::VertexHash;d->fragmentHash=context->commanderWreck?commanderwreck::FragmentHash:objectspotlight::FragmentHash;
		d->first=first;d->count=count;d->firstInstance=base;d->instanceCount=instanceCount;
		GetWorldDrawRecorder().RecordObjectSpotlight(std::move(d),common.CurrentState().draw,common.CurrentWorldState());
	}
};
void Release(const std::shared_ptr<Context>& c){
	if(c->released)return;c->active.reset();
	for(auto& weak:c->programs)if(auto p=weak.second.lock())p->deleted=true;
	for(auto& weak:c->buffers)if(auto b=weak.lock())b->Delete();
	c->released=true;c->groups.clear();c->bufferIDs.clear();c->programs.clear();c->buffers.clear();
}
sol::table CreateContext(lua_State* L,bool commanderWreck=false){
	Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeObjectSpotlightConsumerAvailable(),"Object Spotlight requires its actual unsynced paired native consumer");
	sol::state_view lua(L);auto t=lua.create_table();auto c=std::make_shared<Context>();c->commanderWreck=commanderWreck;
	Bind(t,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){Live(c);auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false),c);std::erase_if(c->buffers,[](const auto& weak){return weak.expired();});c->buffers.emplace_back(b);return b;});
	Bind(t,"GetVAO",[c](){Live(c);auto a=std::make_shared<VertexArray>();a->context=c;return a;});
	Bind(t,"CreateShader",[c](sol::table source){
		Live(c);unsigned stages=0;for(const auto& kv:source){const auto key=kv.first.as<std::string>();Check(key=="vertex"||key=="fragment","Object Spotlight shader options differ");++stages;}Check(stages==2,"Object Spotlight exact vertex/fragment pair required");
		if(c->commanderWreck)commanderwreck::ValidateProgram(source["vertex"].get<std::string>(),source["fragment"].get<std::string>());else objectspotlight::ValidateProgram(source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
		auto p=std::make_shared<Program>();p->identity=c->next++;p->commanderWreck=c->commanderWreck;p->compiledFamily=NativeObjectSpotlightConsumer();c->programs[p->identity]=p;return std::tuple{p,p->identity};
	});
	Bind(t,"UseShader",[c](sol::object value){Live(c);if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted&&p->compiledFamily==NativeObjectSpotlightConsumer()&&p->commanderWreck==c->commanderWreck,"Object Spotlight program no longer has its compiled consumer");c->active=std::move(p);return true;});
	Bind(t,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
	// This exact program has only UBO/SSBO members, no standalone uniforms.
	Bind(t,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"Object Spotlight reflection owner deleted");return sol::state_view(L).create_table();});
	Bind(t,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string&){Check(p&&!p->deleted,"Object Spotlight uniform owner deleted");return -1;});
	Bind(t,"Uniform",[c](int location,sol::variadic_args){Active(c);Check(location==-1,"Object Spotlight has no standalone float uniforms");});
	Bind(t,"UniformInt",[c](int location,sol::variadic_args){Active(c);Check(location==-1,"Object Spotlight has no standalone integer uniforms");});
	Bind(t,"GetShaderLog",[](){return std::string{};});
	Bind(t,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Live(c);Check(label.size()<=1024,"Object Spotlight label exceeds bound");if(type==0x82e0){const auto it=c->bufferIDs.find(id.as<std::string>());Check(it!=c->bufferIDs.end(),"Object Spotlight labelled buffer missing");auto b=it->second.lock();Check(b&&!b->deleted,"Object Spotlight labelled buffer deleted");b->label=label;}else{Check(type==GL_PROGRAM,"Object Spotlight label kind differs");const auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"Object Spotlight labelled program missing");auto p=it->second.lock();Check(p&&!p->deleted,"Object Spotlight labelled program deleted");p->label=label;}});
	Bind(t,"PushDebugGroup",[c](unsigned,const std::string& label){Live(c);c->groups.push_back(label);});
	Bind(t,"PopDebugGroup",[c](){Live(c);Check(!c->groups.empty(),"Object Spotlight debug group underflow");c->groups.pop_back();});
	Bind(t,"ReleaseContext",[c](){Release(c);});return t;
}
}
bool PushEntries(lua_State* L){
	sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
	auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
	Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.CheckLive();b.value->Define(count,layout);});
	Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){b.CheckLive();return b.value->Upload(data,attribute,offset,first,last);});
	Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});
	Bind(b,"GetBufferSize",[](Buffer& b){b.CheckLive();return b.value->GetBufferSize();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
	Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){b.CheckLive();if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
	Bind(b,"InstanceDataFromFeatureIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){b.CheckLive();if(ids.is<int>())return b.value->InstanceDataFromFeatureIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromFeatureIDs(ids.as<sol::stack_table>(),attribute,offset);});
	family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
	Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Live(a.context);Check(!a.deleted&&b&&b->context.lock()==a.context&&!a.vertices,"Object Spotlight vertex attachment differs");b->CheckLive();a.vertices=std::move(b);});
	Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Live(a.context);Check(!a.deleted&&b&&b->context.lock()==a.context&&!a.instances,"Object Spotlight instance attachment differs");b->CheckLive();a.instances=std::move(b);});
 Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Live(a.context);Check(a.context->commanderWreck&&!a.deleted&&b&&b->context.lock()==a.context&&!a.unusedIndex,"Commander Wreck placeholder index attachment differs");b->CheckLive();const auto s=b->value->CaptureNativeSnapshot();Check(s->layout.kind==NativeBufferKind::Index&&s->layout.elements==1,"Commander Wreck original unused index buffer differs");a.unusedIndex=std::move(b);});
	Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.unusedIndex.reset();});
	Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,int count,int first,int instances,int base){a.Draw(mode,count,first,instances,base);});
	family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);
	Bind(family,"Available",&IsNativeObjectSpotlightConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
	Bind(family,"LoadArchiveSource",[L](const std::string& path){
		Check(!CLuaHandle::GetHandleSynced(L),"Object Spotlight source is unsynced-only");const auto& sources=objectspotlight::ArchiveSources;
		const auto it=std::find_if(std::begin(sources),std::end(sources),[&](const auto& v){return v.path==path;});Check(it!=std::end(sources),"Unknown original Object Spotlight archive source");
		CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeObjectSpotlightSourceHash(source)==it->hash,"Original31657 Object Spotlight source identity differs");return source;
	});
	Bind(family,"PrepareSource",[](std::string source){const auto h=NativeObjectSpotlightSourceHash(source);const auto& sources=objectspotlight::ArchiveSources;Check(std::any_of(std::begin(sources),std::end(sources),[&](const auto& v){return v.hash==h;}),"Object Spotlight source preprocessing requires exact original bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 Bind(family,"CreateWreckContext",[L](){return CreateContext(L,true);});
 Bind(family,"LoadWreckArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Commander Wreck source is unsynced-only");const auto& sources=commanderwreck::ArchiveSources;const auto it=std::find_if(std::begin(sources),std::end(sources),[&](const auto& v){return v.path==path;});Check(it!=std::end(sources),"Unknown original Commander Wreck archive source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeObjectSpotlightSourceHash(source)==it->hash,"Original Commander Wreck source identity differs");return source;});
 Bind(family,"PrepareWreckSource",[](std::string source){const auto h=NativeObjectSpotlightSourceHash(source);const auto& sources=commanderwreck::ArchiveSources;Check(std::any_of(std::begin(sources),std::end(sources),[&](const auto& v){return v.hash==h;}),"Commander Wreck preprocessing needs exact original source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
	gl.set("NativeObjectSpotlight",family);return true;
}
}
#else
namespace LuaNativeObjectSpotlight {bool PushEntries(lua_State*){return true;}}
#endif
