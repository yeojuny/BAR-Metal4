/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeAirjets.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaNativeCUSTextures.h"
#include "LuaNativeBlueprint.h"
#include "LuaShaders.h"
#include "Rendering/Backend/NativeParalyzePrograms.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeUnitEffectsData.h"
#include "Rendering/Backend/NativeAirjetsPrograms.h"
#include "Rendering/Backend/NativeOrbPrograms.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <span>
#include <utility>
namespace LuaNativeAirjets {
namespace {
using namespace rendering;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
 try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct ProgramState {
 bool orb=false,paralyze=false,empRework=false;int reflectionPass=0;
 int Location(std::string_view name)const{if(paralyze)return -1;if(name=="noiseMap")return orb?-1:0;if(name=="mask")return orb?-1:1;if(name=="reflectionPass")return 2;return -1;}
 bool Integers(int location,std::span<const int32_t> v){if(location==-1)return true;if(v.size()!=1)return false;if(location<2)return location>=0&&v[0]==location;if(location!=2||(v[0]!=0&&v[0]!=1))return false;reflectionPass=v[0];return true;}
 bool Floats(int location,std::span<const float> values){if(location==-1)return true;if(!orb||location!=2||values.size()!=1||(values[0]!=0&&values[0]!=1))return false;reflectionPass=int(values[0]);return true;}
 std::vector<std::string_view> ActiveUniforms()const{if(paralyze)return {};return orb?std::vector<std::string_view>{"reflectionPass"}:std::vector<std::string_view>{"noiseMap","mask","reflectionPass"};}
};
struct Program;struct Buffer;
struct Context {
 bool orb=false,paralyze=false;
 std::shared_ptr<Program> active;
 std::map<std::string,std::weak_ptr<Buffer>> buffers;
 std::map<unsigned,std::weak_ptr<Program>> programs;
 std::vector<std::string> groups;
 unsigned next=1,error=0;
};
struct Buffer:std::enable_shared_from_this<Buffer> {
 std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;GLenum target;bool deleted=false,modelView=false;
 Buffer(lua_State* L,GLenum t,bool frequent):target(t){Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER,"Airjet buffer target differs");value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
 std::string Identity(){Check(!deleted,"Airjet buffer deleted");auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
 void Delete(){value->Delete();deleted=true;}
};
struct Program {
 ProgramState state;std::shared_ptr<const void> compiledFamily;unsigned identity;std::string label;bool deleted=false;std::weak_ptr<Context> context;
 Program(unsigned id,std::string_view v,std::string_view f,std::string_view engine,std::string_view quaternion,const std::string& defines,bool paralyzed=false):compiledFamily(NativeUnitEffectsConsumer()),identity(id){Check(bool(compiledFamily),"Airjets compiled consumer missing");if(paralyzed){state.paralyze=true;state.empRework=paralyze::ValidateProgram(v,f,engine,quaternion,defines);}else airjets::ValidateProgram(v,f,engine,quaternion,defines);}
 Program(unsigned id,std::string_view vertex,std::string_view fragment):compiledFamily(NativeUnitEffectsConsumer()),identity(id){Check(bool(compiledFamily),"Orb compiled consumer missing");state.orb=true;orbs::ValidateProgram(vertex,fragment);}

};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted&&c->active->compiledFamily==NativeUnitEffectsConsumer()&&c->active->context.lock()==c,"Airjet active program has no current compiled family");return c->active;}
struct VertexArray {
 std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;std::vector<NativeEffectModel> submissions;
 void Draw(GLenum mode,int count,int offset,int n,int baseVertex,int baseInstance){
  Check(!context->paralyze,"Paralyze uses actual ModelsVBO submissions");const auto p=Active(context);
  Check(!deleted&&vertices&&indices&&instances&&!vertices->deleted&&!indices->deleted&&!instances->deleted,"Airjet indexed draw lost its actual buffers");
  Check(mode==GL_TRIANGLES&&count>=0&&offset>=0&&n>=0&&baseVertex==0&&baseInstance>=0,"Airjet indexed draw interval differs");if(!count||!n)return;
  auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();
  Check(common.IsWorldStage()&&stage&&(*stage==NativeWorldDrawStage::WaterReflection||(context->orb?(*stage==NativeWorldDrawStage::AlphaAboveWater||*stage==NativeWorldDrawStage::AlphaAll):*stage==NativeWorldDrawStage::World))&&!common.IsCompiling()&&!common.HasActiveTarget()&&!common.HasActiveTypedTarget(),"Airjets outside original World/reflection callback");
  auto d=std::make_shared<NativeUnitEffectInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->orb=context->orb;d->compiled=p->compiledFamily;
  d->vertices=vertices->value->CaptureNativeSnapshot();d->indices=indices->value->CaptureNativeSnapshot();d->instances=instances->value->CaptureNativeSnapshot();d->original=OriginalCUSUniformCapture().Current();if(!context->orb)d->transforms=OriginalCUSModelPublication().Transforms();d->models=OriginalCUSModelPublication().ModelUniforms();
  d->reflectionPass=p->state.reflectionPass;d->count=count;d->indexOffset=offset;d->instanceCount=n;d->baseVertex=baseVertex;d->baseInstance=baseInstance;
  auto* observer=common.TextureObserver();Check(observer==&OriginalNativeTextureContext().UI(),"Airjets shared texture observer missing");if(!context->orb)for(unsigned unit=0;unit<2;++unit){auto sample=observer->BoundImage(unit);d->images[unit]=sample.image;d->samplers[unit]=sample.sampler;}
  GetWorldDrawRecorder().RecordUnitEffect(*stage,common.CurrentState().draw,common.CurrentWorldState(),std::move(d));
 }
 void Submit(){
  const auto p=Active(context);Check(context->paralyze&&p->state.paralyze&&!deleted&&vertices&&indices&&instances&&!vertices->deleted&&!indices->deleted&&!instances->deleted&&vertices->modelView&&indices->modelView&&!instances->modelView,"Paralyze actual ModelsVBO attachments missing");if(submissions.empty())return;
  auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();Check(common.IsWorldStage()&&stage&&*stage==NativeWorldDrawStage::World&&!common.IsCompiling()&&!common.HasActiveTarget()&&!common.HasActiveTypedTarget(),"Paralyze outside original World callback");
  auto d=std::make_shared<NativeUnitEffectInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->paralyze=true;d->empRework=p->state.empRework;d->compiled=p->compiledFamily;d->submissions=submissions;d->instanceCount=submissions.size();d->instances=instances->value->CaptureNativeSnapshot();d->original=OriginalCUSUniformCapture().Current();d->transforms=OriginalCUSModelPublication().Transforms();d->models=OriginalCUSModelPublication().ModelUniforms();
  GetWorldDrawRecorder().RecordUnitEffect(*stage,common.CurrentState().draw,common.CurrentWorldState(),std::move(d));
 }

};
sol::table CreateContext(lua_State* L,bool orb=false,bool paralyzed=false){
 Check(!CLuaHandle::GetHandleSynced(L)&&bool(NativeUnitEffectsConsumer()),"Airjets require the unsynced compiled family");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();c->orb=orb;c->paralyze=paralyzed;if(paralyzed){Check(LuaNativeBlueprint::HasModelProvider(),"Paralyze original model provider missing");sol::stack::push(L,table);LuaShaders::PushModelBufferUniformEntries(L);lua_pop(L,1);}
 Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=c;return result;});
 Bind(table,"GetVAO",[c](){auto result=std::make_shared<VertexArray>();result->context=c;return result;});
 Bind(table,"CreateProgram",[c](const std::string& name,const std::string& vertex,const std::string& fragment,const std::string& engine,const std::string& quaternion,const std::string& defines,sol::table floats,sol::table ints){
  Check(!c->orb&&name==(c->paralyze?"paralyzedUnitShader":"JetShader GL4"),"Original unit effect program name differs");auto result=std::make_shared<Program>(c->next++,vertex,fragment,engine,quaternion,defines,c->paralyze);
  for(const auto& item:floats){(void)item;Check(false,"Airjets original initial float uniforms differ");}
  unsigned count=0;for(const auto& item:ints){Check(!c->paralyze,"Paralyze has no original initial samplers");const auto key=item.first.as<std::string>();Check(key=="noiseMap"||key=="mask","Airjets original sampler uniform differs");std::array<int32_t,1> v{item.second.as<int32_t>()};Check(result->state.Integers(result->state.Location(key),v),"Airjets sampler unit differs");++count;}Check(count==(c->paralyze?0u:2u),"Original effect sampler initial values differ");
  result->context=c;c->programs[result->identity]=result;return result;
 });
 Bind(table,"CreateShader",[c](sol::table source){Check(c->orb,"Airjets uses its source-verified constructor");for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","Original Orb shader option differs");}
  auto p=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());p->context=c;
  auto floats=source["uniformFloat"].get<sol::table>();unsigned count=0;for(const auto& item:floats){Check(item.first.as<std::string>()=="reflectionPass","Orb float uniform differs");std::array<float,1> value{item.second.as<float>()};Check(p->state.Floats(2,value),"Orb reflection value differs");++count;}Check(count==1,"Orb reflection initializer missing");
  auto ints=source["uniformInt"].get<sol::table>();count=0;for(const auto& item:ints){auto key=item.first.as<std::string>();Check((key=="noiseMap"&&item.second.as<int>()==0)||(key=="mask"&&item.second.as<int>()==1),"Orb unused sampler initializer differs");++count;}Check(count==2,"Orb unused declared samplers differ");c->programs[p->identity]=p;return std::tuple{p,p->identity};
 });
 Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted&&p->compiledFamily==NativeUnitEffectsConsumer()&&p->context.lock()==c,"31657 airjets program has no live compiled family");c->active=std::move(p);return true;});
 Bind(table,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
 Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"31657 airjets uniform program deleted");return p->state.Location(name);});
 Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"31657 airjets reflection program deleted");sol::state_view lua(L);auto result=lua.create_table();unsigned i=0;for(auto name:p->state.ActiveUniforms()){auto row=lua.create_table();row["name"]=std::string(name);row["size"]=1;row["type"]=name=="reflectionPass"?(p->state.orb?GL_FLOAT:GL_INT):GL_SAMPLER_2D;result[++i]=row;}return result;});
 Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"GetError",[c](){return std::exchange(c->error,0);});
 Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"31657 airjets label exceeds bound");if(type==0x82e0){auto it=c->buffers.find(id.as<std::string>());Check(it!=c->buffers.end(),"31657 resource labelled buffer owner missing");auto buffer=it->second.lock();Check(bool(buffer),"31657 resource labelled buffer deleted");buffer->label=label;}else{Check(type==GL_PROGRAM,"31657 label object kind differs");auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"31657 resource labelled program missing");auto p=it->second.lock();Check(bool(p),"31657 resource labelled program deleted");p->label=label;}});
 Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"31657 airjets debug group underflow");c->groups.pop_back();});
return table;
}
}
bool PushEntries(lua_State* L){
 sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Check(!b.deleted&&!b.modelView,"Airjet buffer deleted or ModelsVBO immutable");b.value->Define(count,layout);});
 Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!b.deleted&&!b.modelView,"Airjet buffer deleted or ModelsVBO immutable");return b.value->Upload(data,attribute,offset,first,last);});
 Bind(b,"ModelsVBO",[](Buffer& b){auto c=b.context.lock();Check(!b.deleted&&!b.modelView&&c&&c->paralyze&&LuaNativeBlueprint::HasModelProvider(),"Paralyze ModelsVBO requires actual model provider");b.modelView=true;return true;});
 Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetBufferSize",[](Buffer& b){Check(!b.deleted,"Airjet buffer deleted");return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
 Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){Check(!b.deleted&&!b.modelView,"Airjets instance buffer deleted or ModelsVBO");if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
 family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->context.lock()==a.context&&b->target==GL_ARRAY_BUFFER,"Airjet vertex attachment differs");a.vertices=std::move(b);});
 Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->context.lock()==a.context&&b->target==GL_ARRAY_BUFFER,"Airjet instance attachment differs");Check(!b->modelView,"ModelsVBO cannot be an instance buffer");a.instances=std::move(b);});
 Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->context.lock()==a.context&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Airjet index attachment differs");a.indices=std::move(b);});
 Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();a.submissions.clear();});
 Bind(a,"AddUnitsToSubmission",[](VertexArray& a,int unitID){Check(!a.deleted&&a.context->paralyze&&a.vertices&&a.indices&&a.instances&&!a.vertices->deleted&&!a.indices->deleted&&!a.instances->deleted&&a.vertices->modelView&&a.indices->modelView,"Paralyze submission requires attached model views");Check(a.submissions.size()<a.instances->value->CaptureNativeSnapshot()->layout.elements,"Paralyze submission exceeds actual instance capacity");auto model=LuaNativeBlueprint::CaptureUnitModel(unitID);Check(bool(model),"Paralyze actual unit model missing");const auto index=a.submissions.size();a.submissions.push_back({unitID,std::move(model)});return index;});
 Bind(a,"RemoveFromSubmission",[](VertexArray& a,int index){Check(!a.deleted&&a.context->paralyze&&index>=0&&size_t(index)<a.submissions.size(),"Paralyze removed submission index invalid");if(size_t(index)+1<a.submissions.size())a.submissions[index]=std::move(a.submissions.back());a.submissions.pop_back();});
 Bind(a,"ClearSubmission",[](VertexArray& a){Check(!a.deleted&&a.context->paralyze,"Paralyze cleared submission owner invalid");a.submissions.clear();});
 Bind(a,"Submit",[](VertexArray& a){a.Submit();});
 Bind(a,"DrawElements",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> offset,sol::optional<int> instances,sol::optional<int> baseVertex,sol::optional<int> baseInstance){Check(a.indices&&!a.indices->deleted,"Airjet index owner missing");a.Draw(mode,count.value_or(a.indices->value->CaptureNativeSnapshot()->layout.elements),offset.value_or(0),instances.value_or(1),baseVertex.value_or(0),baseInstance.value_or(0));});
 family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",[](){return bool(NativeUnitEffectsConsumer());});Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){
  Check(!CLuaHandle::GetHandleSynced(L),"Airjet archive is unsynced-only");const auto& allowed=airjets::Sources;
  const auto entry=std::find_if(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.path==path;});Check(entry!=std::end(allowed),"Unknown original resource archive source");
  CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeUnitPrimitiveSourceHash(source)==entry->hash,"Original resource source identity differs");return source;
 });
 Bind(family,"PrepareSource",[](std::string source){const auto h=NativeUnitPrimitiveSourceHash(source);const auto& allowed=airjets::Sources;Check(std::any_of(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.hash==h;}),"Resource source preprocessing requires exact original bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 Bind(family,"CreateOrbContext",[L](){return CreateContext(L,true);});
 Bind(family,"LoadOrbArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Orb source is unsynced-only");const auto& allowed=orbs::Sources;const auto i=std::find_if(std::begin(allowed),std::end(allowed),[&](const auto& a){return a.path==path;});Check(i!=std::end(allowed),"Unknown original Orb source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeUnitPrimitiveSourceHash(source)==i->hash,"Original Orb source identity differs");return source;});
 Bind(family,"PrepareOrbSource",[](std::string source){const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& allowed=orbs::Sources;Check(std::any_of(std::begin(allowed),std::end(allowed),[&](const auto& a){return a.hash==hash;}),"Orb source preprocessing requires exact bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 Bind(family,"CreateParalyzeContext",[L](){return CreateContext(L,false,true);});
 Bind(family,"LoadParalyzeArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"Paralyze source is unsynced-only");const auto& allowed=paralyze::Sources;const auto i=std::find_if(std::begin(allowed),std::end(allowed),[&](const auto& a){return a.path==path;});Check(i!=std::end(allowed),"Unknown original Paralyze source");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeUnitPrimitiveSourceHash(source)==i->hash,"Original Paralyze source identity differs");return source;});
 Bind(family,"PrepareParalyzeSource",[](std::string source){const auto hash=NativeUnitPrimitiveSourceHash(source);const auto& allowed=paralyze::Sources;Check(std::any_of(std::begin(allowed),std::end(allowed),[&](const auto& a){return a.hash==hash;}),"Paralyze source preprocessing requires exact bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 gl.set("NativeAirjets",family);return true;
}
}
#else
namespace LuaNativeAirjets {bool PushEntries(lua_State*){return true;}}
#endif
