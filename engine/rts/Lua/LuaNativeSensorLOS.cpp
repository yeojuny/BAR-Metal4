/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeSensorLOS.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaNativeCUSTextures.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeSensorLOSData.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
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
namespace LuaNativeSensorLOS {
namespace {
using namespace rendering;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
 try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
uint64_t NativeSensorLOSSourceHash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
struct ArchiveSource {std::string_view path;uint64_t hash;};
const ArchiveSource ArchiveSources[]={
 {"LuaUI/Widgets/gui_sensor_ranges_los.lua",1301176431917208088ULL},
 {"modules/graphics/luashader.lua",15173167305575988042ULL},
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL},
 {"LuaUI/Shaders/sensor_ranges_los.vert.glsl",4476039510287419255ULL},
 {"LuaUI/Shaders/sensor_ranges_los.frag.glsl",6432919883938138873ULL},
 };
struct Program;struct Buffer;
struct Context {
 std::shared_ptr<Program> active;
 std::map<std::string,std::weak_ptr<Buffer>> buffers;
 std::map<unsigned,std::weak_ptr<Program>> programs;
 std::vector<std::string> groups;
 unsigned next=1,error=0;
};
struct Buffer:std::enable_shared_from_this<Buffer> {
 std::shared_ptr<LuaVBOImpl> value;std::weak_ptr<Context> context;std::string label;GLenum target;bool deleted=false;
 Buffer(lua_State* L,GLenum t,bool frequent):target(t){Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER,"LOS buffer target differs");value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
 std::string Identity(){Check(!deleted,"LOS buffer deleted");auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
 void Delete(){value->Delete();deleted=true;}
};
struct Program {
 NativeSensorLOSProgramState state;std::shared_ptr<const void> compiledFamily;unsigned identity;std::string label;bool deleted=false;
 Program(unsigned id,std::string_view v,std::string_view f):state(v,f),compiledFamily(NativeSensorLOSConsumerLifetime()),identity(id){Check(bool(compiledFamily),"LOS compiled family unavailable");}
};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted&&c->active->compiledFamily==NativeSensorLOSConsumerLifetime(),"LOS active program has no current compiled family");return c->active;}
struct VertexArray {
 std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
 void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> n,sol::optional<int> base){
  const auto p=Active(context);auto program=p->state.Capture();const bool mask=program.defines->stencilPass;
  Check(!deleted&&vertices&&instances&&!vertices->deleted&&!instances->deleted,"LOS draw lost its actual vertex/instance buffers");
  Check(mode==(mask?GL_TRIANGLE_FAN:GL_LINE_LOOP)&&count.value_or(0)>=0&&first.value_or(0)>=0&&n.value_or(1)>=0&&base.value_or(0)>=0,"LOS original fan/line interval differs");
  auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();
  Check(!common.IsCompiling()&&common.IsActive()&&(mask?(common.HasActiveTarget()&&!common.IsWorldStage()&&!common.IsScreenStage()):(common.IsWorldStage()&&stage&&*stage==NativeWorldDrawStage::World&&!common.HasActiveTarget())),"LOS draw is outside its original Genesis RTT/World callback");
  auto d=std::make_shared<NativeSensorLOSDraw>();d->sourceDrawFrame=globalRendering->drawFrame;d->program=std::move(program);d->consumerLifetime=p->compiledFamily;
  d->vertices=vertices->value->CaptureNativeSnapshot();d->instances=instances->value->CaptureNativeSnapshot();d->original=OriginalCUSUniformCapture().Current();d->models=OriginalCUSModelPublication().ModelUniforms();
  d->first=first.value_or(0);d->count=count.value_or(d->vertices->layout.elements);d->instanceCount=n.value_or(1);d->firstInstance=base.value_or(0);
  d->heightImage=OriginalNativeTextureContext().History().Binding(d->program.heightUnit,NativeCUSTextureTarget::Image2D);
  auto map=d->heightImage.object?std::dynamic_pointer_cast<const NativeMapTextureBacking>(d->heightImage.object->backing):nullptr;
  Check(map&&map->role==NativeMapTextureRole::Height&&map->heights,"LOS requires the retained actual engine height publication");
  if(!mask){d->stencilImage=OriginalNativeTextureContext().History().Binding(d->program.stencilUnit,NativeCUSTextureTarget::Image2D);d->stencilDependency=OriginalNativeTextureContext().UI().DescribeBinding(d->stencilImage).image;}
  d->surface=common.CurrentState().draw;d->raster=common.CurrentWorldState();d->sequence=NextNativeWorldDrawSequence();ValidateNativeSensorLOSDraw(*d);
  if(mask)common.AppendSensorLOS(std::move(d));else GetWorldDrawRecorder().RecordSensorLOS(std::move(d));
 }

};
sol::table CreateContext(lua_State* L){
 Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeSensorLOSConsumerAvailable(),"LOSs require the unsynced compiled family");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();
 Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=c;return result;});
 Bind(table,"GetVAO",[c](){auto result=std::make_shared<VertexArray>();result->context=c;return result;});
 Bind(table,"CreateShader",[c](sol::table source){
  for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","31657 resource spots shader option differs");}
  auto result=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
  if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){std::array<int32_t,1> v{item.second.as<int32_t>()};if(!result->state.Integers(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){std::vector<float> v;if(item.second.is<sol::table>()){auto value=item.second.as<sol::table>();for(unsigned i=1;i<=value.size();++i)v.push_back(value[i].get<float>());}else v.push_back(item.second.as<float>());if(!result->state.Floats(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  c->programs[result->identity]=result;return std::tuple{result,result->identity};
 });
 Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted&&p->compiledFamily==NativeSensorLOSConsumerLifetime(),"31657 resource spots program has no live compiled family");c->active=std::move(p);return true;});
 Bind(table,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
 Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"31657 resource spots uniform program deleted");return p->state.Location(name);});
 Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"31657 resource spots reflection program deleted");sol::state_view lua(L);auto result=lua.create_table();unsigned i=0;for(auto name:p->state.ActiveUniforms()){auto row=lua.create_table();row["name"]=std::string(name);row["size"]=1;row["type"]=name=="rangeColor"?GL_FLOAT_VEC4:(name=="teamColorMix"||name=="lineWidthPx"||name=="fadeStrength")?GL_FLOAT:GL_SAMPLER_2D;result[++i]=row;}return result;});
 Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"GetError",[c](){return std::exchange(c->error,0);});
 Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"31657 resource spots label exceeds bound");if(type==0x82e0){auto it=c->buffers.find(id.as<std::string>());Check(it!=c->buffers.end(),"31657 resource labelled buffer owner missing");auto buffer=it->second.lock();Check(bool(buffer),"31657 resource labelled buffer deleted");buffer->label=label;}else{Check(type==GL_PROGRAM,"31657 label object kind differs");auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"31657 resource labelled program missing");auto p=it->second.lock();Check(bool(p),"31657 resource labelled program deleted");p->label=label;}});
 Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"31657 resource spots debug group underflow");c->groups.pop_back();});
 Bind(table,"Texture",[L](unsigned unit,sol::object value){
  Check(unit<2&&!CLuaHandle::GetHandleSynced(L)&&IsNativeSensorLOSConsumerAvailable(),"LOS texture unit/consumer differs");
  auto& recorder=ui::GetRecorder();auto& textures=OriginalNativeTextureContext();const auto stage=recorder.CurrentWorldStage();
  Check(textures.Initialized()&&textures.UIAttached()&&recorder.TextureObserver()==&textures.UI()&&recorder.IsActive()&&!recorder.IsCompiling()&&((recorder.IsWorldStage()&&stage&&*stage==NativeWorldDrawStage::World&&!recorder.HasActiveTarget())||(!recorder.IsWorldStage()&&!recorder.IsScreenStage()&&recorder.HasActiveTarget())),"LOS Texture lost its actual world/shared owner");
  unsigned observed;bool parsed=true;
  if(value.is<bool>())observed=textures.UI().LuaEnable(unit,value.as<bool>());
  else{const auto name=value.as<std::string>();observed=textures.UI().LuaLookup(unit,[&]{auto lookup=LuaNativeCUSTextures::ParseTexture(L,name);parsed=lookup.parsed;return lookup;});}
  recorder.SyncObservedTextureBinding(observed);return parsed;
 });
return table;
}
}
bool PushEntries(lua_State* L){
 sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Check(!b.deleted,"LOS buffer deleted");b.value->Define(count,layout);});
 Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!b.deleted,"LOS buffer deleted");return b.value->Upload(data,attribute,offset,first,last);});
 Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetBufferSize",[](Buffer& b){Check(!b.deleted,"LOS buffer deleted");return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
 family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"LOS vertex attachment differs");a.vertices=std::move(b);});
 Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"LOS instance attachment differs");a.instances=std::move(b);});
 Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ELEMENT_ARRAY_BUFFER,"LOS index attachment differs");a.indices=std::move(b);});
 Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});
 Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
 Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){Check(!b.deleted,"LOS instance buffer deleted");if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});

 family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&IsNativeSensorLOSConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){
  Check(!CLuaHandle::GetHandleSynced(L),"LOS archive is unsynced-only");const auto& allowed=ArchiveSources;
  const auto entry=std::find_if(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.path==path;});Check(entry!=std::end(allowed),"Unknown original resource archive source");
  CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeSensorLOSSourceHash(source)==entry->hash,"Original resource source identity differs");return source;
 });
 Bind(family,"PrepareSource",[](std::string source){const auto h=NativeSensorLOSSourceHash(source);const auto& allowed=ArchiveSources;Check(std::any_of(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.hash==h;}),"Resource source preprocessing requires exact original bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 gl.set("NativeSensorLOS",family);return true;
}
}
#else
namespace LuaNativeSensorLOS {bool PushEntries(lua_State*){return true;}}
#endif
