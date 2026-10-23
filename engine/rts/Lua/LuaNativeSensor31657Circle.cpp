/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeSensor31657Circle.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeCUSModelPublication.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "Rendering/Backend/NativeSensor31657CirclePrograms.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <limits>
#include <map>
#include <optional>
namespace LuaNativeSensor31657Circle {
namespace {
using namespace rendering;
std::shared_ptr<const void> consumerLifetime;
std::optional<NativeSensor31657CircleFrame> world;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
 try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program;struct Buffer;
struct StorageBinding {std::shared_ptr<Buffer> owner;NativeBufferRange range;std::shared_ptr<const void> compiledFamily;};
struct Context {
 std::shared_ptr<Program> active;
 std::map<std::string,std::weak_ptr<Buffer>> buffers;
 std::map<unsigned,std::weak_ptr<Program>> programs;
 std::map<unsigned,StorageBinding> storage;
 std::vector<std::string> groups;
 unsigned next=1,error=0;
};
std::vector<std::weak_ptr<Context>> contexts;
struct Buffer:std::enable_shared_from_this<Buffer> {
 std::shared_ptr<LuaVBOImpl> value;
 std::weak_ptr<Context> context;
 std::string label;
 GLenum target;
 bool deleted=false;
 // Successful real CopyTo provenance, used only to limit consumed bytes.
 // Capacity beyond copiedBytes remains undefined and is never a circle.
 NativeBufferSnapshotRef copySource;
 uint64_t copiedRevision=0;
 size_t copiedBytes=0;
 Buffer(lua_State* L,GLenum t,bool frequent):target(t){
  Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER||t==GL_SHADER_STORAGE_BUFFER,"31657 circle buffer target differs");
  value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
 }
 std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto owner=context.lock())owner->buffers[id]=shared_from_this();return id;}
 void ClearCopy(){copySource.reset();copiedRevision=0;copiedBytes=0;}
 void Delete(){
  value->Delete();deleted=true;ClearCopy();
  if(auto c=context.lock())std::erase_if(c->storage,[this](const auto& entry){return entry.second.owner.get()==this;});
 }
 bool CopyTo(const std::shared_ptr<Buffer>& destination,int bytes){
  Check(destination&&!deleted&&!destination->deleted&&bytes>=0,"31657 original CopyTo owner/size invalid");
  const auto source=value->CaptureNativeSnapshot();const auto before=destination->value->CaptureNativeSnapshot();
  Check(size_t(bytes)<=source->bytes.size()&&size_t(bytes)<=before->bytes.size(),"31657 original CopyTo exceeds actual CPU/GPU logical capacity");
  // Native owned-buffer copy changes actual CPU backing. The real Metal
  // consumer later materializes this immutable copied destination generation.
  // The original GL CopyTo operation is unchanged and is never spoofed.
  const bool result=value->CopyNativeSnapshotTo(destination->value,bytes);
  if(result&&bytes>0){const auto actual=destination->value->CaptureNativeSnapshot();
   destination->copySource=source;destination->copiedBytes=bytes;destination->copiedRevision=actual->revision;
  }
  return result;
 }
 int Range(bool bind,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> t){
  Check(Available()&&!deleted&&target==GL_SHADER_STORAGE_BUFFER&&t.value_or(target)==GL_SHADER_STORAGE_BUFFER&&binding==5,"31657 native circle supports actual storage binding5 only");
  const auto snapshot=value->CaptureNativeSnapshot();const auto elements=snapshot->layout.elements;
  Check(snapshot->layout.kind==NativeBufferKind::Storage&&elements>0&&elements<=size_t(std::numeric_limits<int>::max()),"31657 native binding has invalid original element count");
  // Preserve LuaVBOImpl's original offset/count clamping and range validation.
  const size_t offset=size_t(std::max(first.value_or(0),0));
  const size_t used=size_t(std::clamp(count.value_or(int(elements)),1,int(elements)));
  Check(offset<=elements&&used<=elements-offset,"31657 native binding offset/count exceeds original capacity");
  auto c=context.lock();Check(bool(c),"31657 native binding lost compiled context");
  if(!bind){c->storage.erase(binding);return int(binding);}
  // Binding5 is a real fixed resource of the compiled circle family, not a
  // claimed GL capability. The original GL API binds its allocation tail.
  // Retain the full logical tail; GPU padding cannot contain used circles.
  auto range=value->CaptureNativeRange(binding,offset,elements-offset,6);
  c->storage[binding]={shared_from_this(),std::move(range),consumerLifetime};
  return int(binding); // only after installing the actual consumed native range
 }

};
struct Program {
 NativeSensor31657CircleProgramState state;
 std::shared_ptr<const void> compiledFamily;
 unsigned identity;
 std::string label;
 bool deleted=false;
 Program(unsigned id,std::string_view v,std::string_view f):state(v,f),compiledFamily(consumerLifetime),identity(id){Check(bool(compiledFamily),"31657 circle actual Metal family is unavailable");}
};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted&&c->active->compiledFamily==consumerLifetime,"31657 circle active program has no current compiled family");return c->active;}
NativeBufferRange CaptureCircles(const std::shared_ptr<Context>& c,int32_t count)
{
 const auto it=c->storage.find(5);Check(it!=c->storage.end()&&it->second.owner,"31657 original binding5 has not been observed");
 const auto& binding=it->second;const auto& buffer=*binding.owner;const auto actual=buffer.value->CaptureNativeSnapshot();
 Check(!buffer.deleted&&binding.compiledFamily==consumerLifetime,"31657 binding5 owner/compiled family no longer live");
 const auto& range=binding.range;
 Check(range.snapshot&&range.snapshot->object==actual->object&&range.snapshot->revision==actual->revision,"31657 selected original must rebind after copying/mutating binding5");
 Check(count>=0&&size_t(count)<=range.size/32,"31657 circleCount exceeds actual native bound range");
 Check(buffer.copySource&&buffer.copiedRevision==actual->revision&&range.offset<=buffer.copiedBytes&&size_t(count)*32<=buffer.copiedBytes-range.offset,"31657 used circles lack an actual native byte-copy at this destination revision");
 return range;
}
struct VertexArray {
 std::shared_ptr<Context> context;
 std::shared_ptr<Buffer> vertices,instances,indices;
 bool deleted=false;
 void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
  Check(Available()&&!deleted&&vertices&&instances,"31657 circle needs its actual VAO and compiled family");const auto program=Active(context);
  Check(mode==GL_LINE_LOOP&&count.value_or(0)>=0&&first.value_or(0)>=0&&instanceCount.value_or(1)>=0&&base.value_or(0)>=0,"31657 original LINE_LOOP arguments differ");
  auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"31657 circle draw during list compilation");
  const auto stage=common.CurrentWorldStage();Check(world&&world->sourceDrawFrame==globalRendering->drawFrame&&stage&&*stage==NativeWorldDrawStage::World&&!common.HasActiveTarget(),"31657 circle requires the actual DrawWorld callin");
  NativeSensor31657CircleDraw draw;draw.sourceDrawFrame=globalRendering->drawFrame;draw.program=program->state.Capture();draw.consumerLifetime=program->compiledFamily;
  draw.vertices=vertices->value->CaptureNativeSnapshot();draw.instances=instances->value->CaptureNativeSnapshot();
  // The helper's placeholder index attachment is retained by the VAO but is
  // not consumed by this original DrawArrays call. No invented index reads.
  draw.first=first.value_or(0);draw.count=count.value_or(draw.vertices->layout.elements);draw.firstInstance=base.value_or(0);draw.instanceCount=instanceCount.value_or(1);
  draw.circles=CaptureCircles(context,draw.program.circleCount);
  draw.original=OriginalCUSUniformCapture().Current();draw.models=OriginalCUSModelPublication().ModelUniforms();draw.surface=common.CurrentState().draw;draw.raster=common.CurrentWorldState();
  Check(draw.program.heightUnit>=0&&draw.program.heightUnit<int(NativeCUSTextureHistory::Units),"31657 circle height sampler unit differs");
  draw.heightImage=OriginalNativeTextureContext().History().Binding(draw.program.heightUnit,NativeCUSTextureTarget::Image2D);
  auto map=draw.heightImage.object?std::dynamic_pointer_cast<const NativeMapTextureBacking>(draw.heightImage.object->backing):nullptr;
  if(map)Check(map->role==NativeMapTextureRole::Height&&map->heights,"31657 circle actual height publication is unavailable");
  else draw.heightDependency=OriginalNativeTextureContext().UI().DescribeBinding(draw.heightImage).image;
  draw.sequence=NextNativeWorldDrawSequence();ValidateNativeSensor31657CircleDraw(draw);world->draws.push_back(std::move(draw));
 }
};
bool BindHeight(unsigned unit,const std::string& name)
{
 Check(unit<NativeCUSTextureHistory::Units&&name=="$heightmap","31657 circle dedicated map binding differs");auto lookup=OriginalNativeMapTextures().Lookup(name);const bool parsed=lookup.parsed;
 ui::GetRecorder().RecordOperation([unit,lookup=std::move(lookup)](ui::NativeUIRecorder& replay){auto& shared=OriginalNativeTextureContext();const auto actual=unit?unit:shared.History().ActiveUnit().value();shared.Registry().LuaTexture(unit,[&]{auto value=lookup;if(value.object)value.object=shared.Registry().ResolveRecordedName(value.object);return value;});if(actual<2)replay.SyncObservedTextureBinding(actual);});return parsed;
}
sol::table CreateContext(lua_State* L)
{
 Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"31657 circle needs the actual unsynced paired family");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();std::erase_if(contexts,[](const auto& value){return value.expired();});contexts.push_back(c);
 Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=c;return result;});
 Bind(table,"GetVAO",[c](){auto result=std::make_shared<VertexArray>();result->context=c;return result;});
 Bind(table,"CreateShader",[c](sol::table source){
  for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","31657 circle shader option differs");}
  auto result=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
  if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){std::array<int32_t,1> v{item.second.as<int32_t>()};if(!result->state.Integers(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){std::vector<float> v;if(item.second.is<sol::table>()){auto value=item.second.as<sol::table>();for(unsigned i=1;i<=value.size();++i)v.push_back(value[i].get<float>());}else v.push_back(item.second.as<float>());if(!result->state.Floats(result->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  c->programs[result->identity]=result;return std::tuple{result,result->identity};
 });
 Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted&&p->compiledFamily==consumerLifetime,"31657 circle program has no live compiled family");c->active=std::move(p);return true;});
 Bind(table,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});
 Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"31657 circle uniform program deleted");return p->state.Location(name);});
 Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"31657 circle reflection program deleted");sol::state_view lua(L);auto result=lua.create_table();unsigned i=0;for(auto name:p->state.ActiveUniforms()){auto row=lua.create_table();row["name"]=std::string(name);row["size"]=1;row["type"]=name=="rangeColor"?GL_FLOAT_VEC4:name=="circleCount"?GL_INT:GL_SAMPLER_2D;result[++i]=row;}return result;});
 Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"GetError",[c](){return std::exchange(c->error,0);});Bind(table,"BindHeight",&BindHeight);
 Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"31657 circle label exceeds bound");if(type==0x82e0){auto it=c->buffers.find(id.as<std::string>());Check(it!=c->buffers.end(),"31657 labelled buffer owner missing");auto buffer=it->second.lock();Check(bool(buffer),"31657 labelled buffer deleted");buffer->label=label;}else{Check(type==GL_PROGRAM,"31657 label object kind differs");auto it=c->programs.find(id.as<unsigned>());Check(it!=c->programs.end(),"31657 labelled program missing");auto p=it->second.lock();Check(bool(p),"31657 labelled program deleted");p->label=label;}});
 Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"31657 circle debug group underflow");c->groups.pop_back();});return table;
}
}
void SetConsumerLifetime(std::shared_ptr<const void> actual){if(consumerLifetime!=actual)Reset();consumerLifetime=std::move(actual);}
bool Available(){return bool(consumerLifetime);}
void BeginWorldFrame(){Check(Available()&&!world,"31657 circle world begin lacks consumer or was duplicated");world.emplace();world->sourceDrawFrame=globalRendering->drawFrame;}
std::shared_ptr<const NativeSensor31657CircleFrame> EndWorldFrame(){Check(bool(world),"31657 circle world end has no begin");auto result=std::make_shared<NativeSensor31657CircleFrame>(std::move(*world));world.reset();return result;}
void AbortWorldFrame()noexcept{world.reset();for(auto& weak:contexts)if(auto c=weak.lock())c->active.reset();}
void Reset(){AbortWorldFrame();for(auto& weak:contexts)if(auto c=weak.lock()){c->storage.clear();for(auto& [id,weakProgram]:c->programs)if(auto p=weakProgram.lock())p->deleted=true;}}
bool PushEntries(lua_State* L)
{
 sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(b,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.value->Define(count,layout);b.ClearCopy();});
 Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){const auto result=b.value->Upload(data,attribute,offset,first,last);b.ClearCopy();return result;});
 Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
 Bind(b,"CopyTo",[](Buffer& b,const std::shared_ptr<Buffer>& destination,int bytes){return b.CopyTo(destination,bytes);});
 Bind(b,"BindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(true,binding,first,count,target);});
 Bind(b,"UnbindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(false,binding,first,count,target);});
 Bind(b,"InstanceDataFromUnitIDs",[](Buffer& b,sol::stack_object ids,int attribute,sol::optional<int> offset){b.ClearCopy();if(ids.is<int>())return b.value->InstanceDataFromUnitIDs(ids.as<int>(),attribute,offset);return b.value->InstanceDataFromUnitIDs(ids.as<sol::stack_table>(),attribute,offset);});
 family.new_usertype<Program>("Program",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.vertices,"31657 circle vertex attachment differs");a.vertices=std::move(b);});
 Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.instances,"31657 circle instance attachment differs");a.instances=std::move(b);});
 Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.indices,"31657 circle index attachment differs");a.indices=std::move(b);});
 Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});
 Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
 // No selected circle widget issues DrawElements. Do not provide a false path.
 family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Available);Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){
  Check(!CLuaHandle::GetHandleSynced(L),"31657 circles are unsynced-only");
  const auto& allowed=sensor31657CircleProgram::ArchiveSources;const auto entry=std::find_if(allowed.begin(),allowed.end(),[&](const auto& item){return item.path==path;});
  Check(entry!=allowed.end(),"Unknown current31657 circle archive source");
  CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"31657 circle original archive source missing");
  Check(NativeSensor31657SourceHash(source)==entry->hash,"31657 original archive source identity differs");return source;
 });
 Bind(family,"PrepareSource",[](std::string source){
  const auto hash=NativeSensor31657SourceHash(source);const auto& allowed=sensor31657CircleProgram::ArchiveSources;
  Check(std::any_of(allowed.begin(),allowed.end(),[&](const auto& item){return item.hash==hash;}),"31657 source preprocessing requires exact original bytes");
  LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
 });gl.set("NativeSensor31657Circle",family);return true;
}
}
#else
namespace LuaNativeSensor31657Circle {
void SetConsumerLifetime(std::shared_ptr<const void>){}bool Available(){return false;}void BeginWorldFrame(){}
std::shared_ptr<const rendering::NativeSensor31657CircleFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
