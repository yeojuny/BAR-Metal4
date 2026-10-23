/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeResourceSpots.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaNativeCUSTextures.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeResourceSpotsData.h"
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
namespace LuaNativeResourceSpots {
namespace {
using namespace rendering;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
 try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct ProgramState {
 NativeResourceSpotKind kind;
 uint64_t vertexHash,fragmentHash;
 float drawPass=0,waterLevel=0;
 ProgramState(std::string_view v,std::string_view f):vertexHash(NativeResourceSpotsSourceHash(v)),fragmentHash(NativeResourceSpotsSourceHash(f)){
  if(vertexHash==resourcespots::MetalVertexHash&&fragmentHash==resourcespots::MetalFragmentHash)kind=NativeResourceSpotKind::Metal;
  else {Check(vertexHash==resourcespots::GeoVertexHash&&fragmentHash==resourcespots::GeoFragmentHash,"Resource spots require the exact original expanded shader pair");kind=NativeResourceSpotKind::Geothermal;}
 }
 int Location(std::string_view name)const{
  if(kind==NativeResourceSpotKind::Geothermal)return -1;
  if(name=="heightMap")return 0;if(name=="textAtlas")return 1;if(name=="drawPass")return 2;if(name=="waterLevel")return 3;return -1;
 }
 bool Integers(int location,std::span<const int32_t> v){if(location==-1)return true;return location>=0&&location<2&&v.size()==1&&v[0]==location;}
 bool Floats(int location,std::span<const float> v){if(location==-1)return true;if((location!=2&&location!=3)||v.size()!=1||!std::isfinite(v[0]))return false;(location==2?drawPass:waterLevel)=v[0];return true;}
 std::vector<std::string_view> ActiveUniforms()const{return kind==NativeResourceSpotKind::Metal?std::vector<std::string_view>{"heightMap","textAtlas","drawPass","waterLevel"}:std::vector<std::string_view>{};}
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
 Buffer(lua_State* L,GLenum t,bool frequent):target(t){Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER,"Resource spot buffer target differs");value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
 std::string Identity(){Check(!deleted,"Resource spot buffer deleted");auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
 void Delete(){value->Delete();deleted=true;}
};
struct Program {
 ProgramState state;std::shared_ptr<const void> compiledFamily;unsigned identity;std::string label;bool deleted=false;
 Program(unsigned id,std::string_view v,std::string_view f):state(v,f),compiledFamily(NativeResourceSpotsConsumerLifetime()),identity(id){Check(bool(compiledFamily),"Resource spot compiled family unavailable");}
};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted&&c->active->compiledFamily==NativeResourceSpotsConsumerLifetime(),"Resource spot active program has no current compiled family");return c->active;}
struct VertexArray {
 std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
 void Draw(GLenum mode,int count,int offset,int n,int baseVertex,int baseInstance){
  const auto p=Active(context);
  Check(!deleted&&vertices&&indices&&instances&&!vertices->deleted&&!indices->deleted&&!instances->deleted,"Resource spot indexed draw lost its actual buffers");
  Check(mode==GL_TRIANGLES&&count>=0&&offset>=0&&n>=0&&baseVertex==0&&baseInstance>=0,"Resource spot indexed draw interval differs");if(!count||!n)return;
  auto& common=ui::GetRecorder();const auto stage=common.CurrentWorldStage();
  Check(common.IsWorldStage()&&stage&&(*stage==NativeWorldDrawStage::PreUnit||*stage==NativeWorldDrawStage::World)&&!common.IsCompiling()&&!common.HasActiveTarget(),"Resource spot draw is outside its original main world callback");
  auto d=std::make_shared<NativeResourceSpotsInput>();d->sourceDrawFrame=globalRendering->drawFrame;d->kind=p->state.kind;d->vertexHash=p->state.vertexHash;d->fragmentHash=p->state.fragmentHash;d->compiledFamily=p->compiledFamily;
  d->vertices=vertices->value->CaptureNativeSnapshot();d->indices=indices->value->CaptureNativeSnapshot();d->instances=instances->value->CaptureNativeSnapshot();d->original=OriginalCUSUniformCapture().Current();
  d->drawPass=p->state.drawPass;d->waterLevel=p->state.waterLevel;d->count=count;d->indexOffset=offset;d->instanceCount=n;d->baseVertex=baseVertex;d->baseInstance=baseInstance;
  if(d->kind==NativeResourceSpotKind::Metal)for(unsigned unit=0;unit<2;++unit)d->images[unit]=OriginalNativeTextureContext().History().Binding(unit,NativeCUSTextureTarget::Image2D);
  GetWorldDrawRecorder().RecordResourceSpots(std::move(d),*stage,common.CurrentState().draw,common.CurrentWorldState());
 }
};
sol::table CreateContext(lua_State* L){
 Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeResourceSpotsConsumerAvailable(),"Resource spots require the unsynced compiled family");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();
 Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=c;return result;});
 Bind(table,"GetVAO",[c](){auto result=std::make_shared<VertexArray>();result->context=c;return result;});
 Bind(table,"CreateShader",[c](sol::table source){
  for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","31657 resource spots shader option differs");}
  auto result=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
  if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){std::array<int32_t,1> v{item.second.as<int32_t>()};if(!result->state.Integers(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){std::vector<float> v;if(item.second.is<sol::table>()){auto value=item.second.as<sol::table>();for(unsigned i=1;i<=value.size();++i)v.push_back(value[i].get<float>());}else v.push_back(item.second.as<float>());if(!result->state.Floats(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  c->programs[result->identity]=result;return std::tuple{result,result->identity};
 });
 Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted&&p->compiledFamily==NativeResourceSpotsConsumerLifetime(),"31657 resource spots program has no live compiled family");c->active=std::move(p);return true;});
 Bind(table,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
 Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"31657 resource spots uniform program deleted");return p->state.Location(name);});
 Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"31657 resource spots reflection program deleted");sol::state_view lua(L);auto result=lua.create_table();unsigned i=0;for(auto name:p->state.ActiveUniforms()){auto row=lua.create_table();row["name"]=std::string(name);row["size"]=1;row["type"]=(name=="drawPass"||name=="waterLevel")?GL_FLOAT:GL_SAMPLER_2D;result[++i]=row;}return result;});
 Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"GetError",[c](){return std::exchange(c->error,0);});
 Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"31657 resource spots label exceeds bound");if(type==0x82e0){auto it=c->buffers.find(id.as<std::string>());Check(it!=c->buffers.end(),"31657 resource labelled buffer owner missing");auto buffer=it->second.lock();Check(bool(buffer),"31657 resource labelled buffer deleted");buffer->label=label;}else{Check(type==GL_PROGRAM,"31657 label object kind differs");auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"31657 resource labelled program missing");auto p=it->second.lock();Check(bool(p),"31657 resource labelled program deleted");p->label=label;}});
 Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"31657 resource spots debug group underflow");c->groups.pop_back();});
 Bind(table,"Texture",[L](unsigned unit,sol::object value){
  Check(unit<2&&!CLuaHandle::GetHandleSynced(L)&&IsNativeResourceSpotsConsumerAvailable(),"Resource spot texture unit/consumer differs");
  auto& recorder=ui::GetRecorder();auto& textures=OriginalNativeTextureContext();const auto stage=recorder.CurrentWorldStage();
  Check(textures.Initialized()&&textures.UIAttached()&&recorder.TextureObserver()==&textures.UI()&&!recorder.IsCompiling(),"Resource spot Texture lost its actual shared owner or is compiling a list");
  const bool mainWorld=recorder.IsWorldStage()&&stage&&(*stage==NativeWorldDrawStage::PreUnit||*stage==NativeWorldDrawStage::World)&&!recorder.HasActiveTarget();
  // Original InitializeAtlas queues a blank-image task before the glyphs.
  // DrawGenesis renders both into the owned atlas through common UI RTT.
  // Texture bindings share that owner; indexed resource geometry stays world-only.
  const bool genesisAtlas=recorder.IsGenesisStage()&&recorder.HasActiveTarget()&&!recorder.IsWorldStage()&&!recorder.IsScreenStage();
  Check(mainWorld||genesisAtlas,"Resource spot Texture is outside its original world or DrawGenesis atlas target");
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
 Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Check(!b.deleted,"Resource spot buffer deleted");b.value->Define(count,layout);});
 Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Check(!b.deleted,"Resource spot buffer deleted");return b.value->Upload(data,attribute,offset,first,last);});
 Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetBufferSize",[](Buffer& b){Check(!b.deleted,"Resource spot buffer deleted");return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
 family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Resource spot vertex attachment differs");a.vertices=std::move(b);});
 Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ARRAY_BUFFER,"Resource spot instance attachment differs");a.instances=std::move(b);});
 Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!b->deleted&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Resource spot index attachment differs");a.indices=std::move(b);});
 Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});
 Bind(a,"DrawElements",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> offset,sol::optional<int> instances,sol::optional<int> baseVertex,sol::optional<int> baseInstance){Check(a.indices&&!a.indices->deleted,"Resource spot index owner missing");a.Draw(mode,count.value_or(a.indices->value->CaptureNativeSnapshot()->layout.elements),offset.value_or(0),instances.value_or(1),baseVertex.value_or(0),baseInstance.value_or(0));});
 family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&IsNativeResourceSpotsConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){
  Check(!CLuaHandle::GetHandleSynced(L),"Resource spot archive is unsynced-only");const auto& allowed=resourcespots::ArchiveSources;
  const auto entry=std::find_if(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.path==path;});Check(entry!=std::end(allowed),"Unknown original resource archive source");
  CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeResourceSpotsSourceHash(source)==entry->hash,"Original resource source identity differs");return source;
 });
 Bind(family,"PrepareSource",[](std::string source){const auto h=NativeResourceSpotsSourceHash(source);const auto& allowed=resourcespots::ArchiveSources;Check(std::any_of(std::begin(allowed),std::end(allowed),[&](const auto& item){return item.hash==h;}),"Resource source preprocessing requires exact original bytes");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});
 gl.set("NativeResourceSpots",family);return true;
}
}
#else
namespace LuaNativeResourceSpots {bool PushEntries(lua_State*){return true;}}
#endif
