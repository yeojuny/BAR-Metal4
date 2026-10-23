/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeShield.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaUtils.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeShieldPrograms.h"
#include "Rendering/Backend/NativeShieldRecorder.h"
#include "Rendering/Backend/NativeShieldTextureState.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace LuaNativeShield {
namespace {
using namespace rendering;
NativeShieldRecorder recorder;
const NativeShieldTextureContext* textureContext=nullptr;
bool ConnectedConsumerAvailable(){return IsNativeShieldConsumerAvailable()&&textureContext&&bool(textureContext->syncObservedBinding);}
void Check(bool valid,const char* message) { if(!valid)throw std::runtime_error(message); }
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{
 template<typename F>static auto Wrap(F function){return [function=std::move(function)](sol::this_state lua,Args...args)->R{
  try{return function(std::forward<Args>(args)...);}catch(const std::exception& error){lua_pushstring(lua,error.what());}
  lua_error(lua);std::terminate();
 };}
};
template<typename T,typename F>void Bind(T& table,const char* name,F function){table.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(function)));}
std::uint64_t Hash(std::string_view source)
{
 std::uint64_t h=14695981039346656037ULL;
 for(unsigned char byte:source)h=(h^byte)*1099511628211ULL;
 return h;
}
const NativeShieldArchiveSource& Archive(const std::string& path)
{
 const auto found=std::find_if(NativeShieldArchiveSources.begin(),NativeShieldArchiveSources.end(),[&](const auto& entry){return entry.path==path;});
 Check(found!=NativeShieldArchiveSources.end(),"Unknown current31657 Shield archive source");return *found;
}
struct Program;struct Buffer;
struct StorageBinding {std::shared_ptr<Buffer> owner;NativeBufferRange range;};
struct Context {
 std::weak_ptr<Program> active;
 std::optional<StorageBinding> impacts;
 void Reset() noexcept {active.reset();impacts.reset();}
};
std::vector<std::weak_ptr<Context>> contexts;
struct Program:std::enable_shared_from_this<Program> {
 std::shared_ptr<Context> context;
 bool clipZeroToOne=false,deleted=false;
 bool Initialize(){Check(!deleted&&ConnectedConsumerAvailable(),"Original current31657 Shield consumer unavailable");return true;}
 void Activate(){Check(!deleted&&ConnectedConsumerAvailable(),"Original Shield program unavailable");context->active=shared_from_this();}
 void Deactivate(){if(context->active.lock().get()==this)context->active.reset();}
 void Finalize(){Deactivate();deleted=true;}
};
struct Buffer:std::enable_shared_from_this<Buffer> {
 std::shared_ptr<LuaVBOImpl> value;
 std::weak_ptr<Context> context;
 GLenum target;
 bool deleted=false;
 std::size_t uploadedBytes=0;
 std::uint64_t uploadedRevision=0;
 NativeBufferSnapshotRef geometrySource;
 std::shared_ptr<const NativeShieldGeometry> geometry;
 Buffer(lua_State* L,GLenum type,bool frequent):target(type){
  Check(type==GL_ARRAY_BUFFER||type==GL_ELEMENT_ARRAY_BUFFER||type==GL_SHADER_STORAGE_BUFFER,"Original Shield buffer target differs");
  value=std::make_shared<LuaVBOImpl>(type,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);
 }
 void Define(int count,sol::optional<sol::object> layout){
  Check(!deleted,"Original Shield buffer was deleted");value->Define(count,layout);uploadedBytes=0;uploadedRevision=0;geometrySource.reset();geometry.reset();
 }
 std::size_t Upload(const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){
  Check(!deleted&&attribute.value_or(-1)==-1&&offset.value_or(0)==0&&first.value_or(1)==1,"Original current31657 Shield upload is a complete attribute prefix");
  const auto bytes=value->Upload(data,attribute,offset,first,last);
  Check(bytes>0,"Original Shield VBO upload failed");
  const auto actual=value->CaptureNativeSnapshot();Check(bytes<=actual->bytes.size(),"Original Shield upload exceeds backing");
  uploadedBytes=bytes;uploadedRevision=actual->revision;geometrySource.reset();geometry.reset();return bytes;
 }
 NativeBufferSnapshotRef Capture(){
  Check(!deleted,"Original Shield buffer was deleted");return value->CaptureNativeSnapshot();
 }
 void Delete(){
  value->Delete();deleted=true;uploadedBytes=0;uploadedRevision=0;geometrySource.reset();geometry.reset();
  if(auto c=context.lock();c&&c->impacts&&c->impacts->owner.get()==this)c->impacts.reset();
 }
 int Range(bool bind,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> type){
  Check(ConnectedConsumerAvailable()&&!deleted&&target==GL_SHADER_STORAGE_BUFFER&&type.value_or(target)==GL_SHADER_STORAGE_BUFFER&&binding==6,"Original Shield requires actual compiled storage binding6");
  const auto actual=Capture();const auto elements=actual->layout.elements;
  Check(actual->layout.kind==NativeBufferKind::Storage&&elements>0&&elements<=std::size_t(std::numeric_limits<int>::max()),"Original Shield storage element count differs");
  const auto offset=std::size_t(std::max(first.value_or(0),0));
  const auto used=std::size_t(std::clamp(count.value_or(int(elements)),1,int(elements)));
  Check(offset<=elements&&used<=elements-offset,"Original Shield storage interval exceeds capacity");
  const auto c=context.lock();Check(bool(c),"Original Shield storage lost its context");
  if(!bind){c->impacts.reset();return int(binding);}
  // Preserve LuaVBOImpl's allocation-tail binding after its count validation.
  // Seven is this compiled family's exclusive binding limit, not a global GL cap.
  c->impacts=StorageBinding{shared_from_this(),value->CaptureNativeRange(binding,offset,elements-offset,7)};
  return int(binding);
 }
};
void Attribute(const NativeBufferAttribute& a,unsigned id,std::string_view name,unsigned count,std::size_t offset,std::size_t bytes,NativeValueShape shape=NativeValueShape::Scalar)
{
 Check(a.id==id&&a.name==name&&a.scalarType==NativeScalarType::Float32&&a.valueShape==shape&&a.declaredCount==count&&a.offset==offset&&a.byteSize==bytes&&!a.normalized,"Original Shield actual VBO attribute layout differs");
}
void Snapshot(const NativeBufferSnapshotRef& source)
{
 Check(source&&source->object&&source->revision,"Original Shield buffer generation missing");
 Check(source->bytes.size()==CheckedNativeBufferByteSize(source->layout),"Original Shield buffer backing size differs");
}
std::shared_ptr<const NativeShieldGeometry> Geometry(Buffer& buffer,const NativeBufferSnapshotRef& source)
{
 Snapshot(source);const auto& layout=source->layout;
 Check(layout.kind==NativeBufferKind::Vertex&&layout.stride==12&&layout.attributes.size()==1,"Original Shield vertex layout differs");
 Attribute(layout.attributes[0],0,"vertexPos",3,0,12);
 Check(buffer.uploadedRevision==source->revision&&buffer.uploadedBytes==source->bytes.size(),"Original Shield mesh does not have its complete actual upload");
 if(buffer.geometrySource==source&&buffer.geometry)return buffer.geometry;
 auto result=std::make_shared<NativeShieldGeometry>();result->identity=source->object;result->vertices.resize(layout.elements);
 for(std::size_t i=0;i<layout.elements;++i)std::memcpy(result->vertices[i].position.data(),source->bytes.data()+i*12,12);
 // No generated replacement sphere or winding adjustment: use original bytes.
 ValidateNativeShieldGeometry(*result);buffer.geometrySource=source;buffer.geometry=result;return result;
}
void CaptureUniforms(NativeShieldDraw& draw,bool clipZeroToOne)
{
 draw.original=OriginalCUSUniformCapture().Current();Check(bool(draw.original),"Original Shield engine UBO publication missing");
 ValidateNativeCUSUniformPublication(*draw.original);
 Check(draw.original->matrices->sourceDrawFrame<=globalRendering->drawFrame&&draw.original->params->sourceDrawFrame<=globalRendering->drawFrame,"Original Shield engine UBO publication is from a future frame");
 const auto& matrices=draw.original->matrices->bytes;const auto& params=draw.original->params->bytes;auto& u=draw.uniforms;
 std::memcpy(u.view.data(),matrices.data()+3*64,64);
 std::memcpy(u.projection.data(),matrices.data()+4*64,64);
 std::memcpy(u.cameraPosition.data(),matrices.data()+7*64+12*4,16);
 std::memcpy(u.timeInfo.data(),params.data()+16,16);
 std::uint32_t caps=0;std::memcpy(&caps,params.data()+12,4);
 Check(bool(caps&1u)==clipZeroToOne,"Original Shield shader clip define differs from actual engine UBO");
 u.depthClipZeroToOne=clipZeroToOne;u.gameFrame=u.timeInfo[0]+u.timeInfo[3];
}
struct VertexArray {
 std::shared_ptr<Context> context;
 std::shared_ptr<Buffer> vertices,instances,indices;
 bool deleted=false;
 void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
  Check(ConnectedConsumerAvailable()&&!deleted&&vertices&&instances&&recorder.Active(),"Original Shield DrawArrays requires its actual VAO and world scope");
  const auto program=context->active.lock();Check(program&&!program->deleted,"Original Shield DrawArrays has no actual active program");
  auto& common=ui::GetRecorder();
  Check(common.IsWorldStage()&&!common.IsCompiling()&&!common.HasActiveTarget(),"Original Shield DrawArrays requires the actual world target");
  const auto vertex=vertices->Capture(),instance=instances->Capture();Snapshot(instance);
  Check(mode==GL_TRIANGLES&&first.value_or(0)==0&&base.value_or(0)==0&&count.value_or(int(vertex->layout.elements))==int(vertex->layout.elements),"Original current31657 Shield DrawArrays geometry interval differs");
  const int used=instanceCount.value_or(1);
  const auto& layout=instance->layout;
  Check(layout.kind==NativeBufferKind::Vertex&&layout.stride==sizeof(NativeShieldInstance)&&layout.attributes.size()==5&&used>0&&std::size_t(used)<=layout.elements,"Original Shield instance draw/layout differs");
  constexpr std::array<std::string_view,5> names{"instPosRadius","instRotMargin","instColor1","instColor2","instParams"};
  for(unsigned i=0;i<5;++i)Attribute(layout.attributes[i],i+1,names[i],4,i*16,16);
  Check(instances->uploadedRevision==instance->revision&&std::size_t(used)*sizeof(NativeShieldInstance)<=instances->uploadedBytes,"Original Shield instance count exceeds its actual uploaded prefix");
  NativeShieldDraw draw;draw.vertexSource=vertex;draw.instanceSource=instance;draw.geometry=Geometry(*vertices,vertex);
  draw.instances.resize(used);std::memcpy(draw.instances.data(),instance->bytes.data(),used*sizeof(NativeShieldInstance));
  CaptureUniforms(draw,program->clipZeroToOne);
  for(const auto& item:draw.instances){
   const float bits=item.params[0];Check(std::isfinite(bits)&&bits>=0&&bits<=1022&&std::floor(bits)==bits,"Original Shield instance effect bits differ");
   draw.uniforms.effects|=unsigned(bits);
  }
  if(context->impacts){
   const auto& binding=*context->impacts;const auto actual=binding.owner->Capture();Snapshot(actual);
   Check(actual->object==binding.range.snapshot->object&&actual->layout==binding.range.snapshot->layout,"Original Shield bound storage was redefined");
   const auto& storage=actual->layout;
   Check(storage.kind==NativeBufferKind::Storage&&storage.stride==64&&storage.attributes.size()==1,"Original Shield impact SSBO layout differs");
   Attribute(storage.attributes[0],0,"impacts",4,0,64,NativeValueShape::Vector4);
   const auto offset=binding.range.offset,bytes=binding.range.size;
   Check(offset<=actual->bytes.size()&&bytes<=actual->bytes.size()-offset&&binding.owner->uploadedRevision==actual->revision&&offset<=binding.owner->uploadedBytes,"Original Shield impact range lacks its actual upload");
   draw.impactSource={actual,6,offset,bytes};
   draw.impactUploadedBytes=std::min(bytes,binding.owner->uploadedBytes-offset);
   draw.impactPoints.resize(bytes/16);std::memcpy(draw.impactPoints.data(),actual->bytes.data()+offset,bytes);
  }
  draw.raster=common.CurrentWorldState();draw.surface=common.CurrentState().draw;
  Check(textureContext!=nullptr,"Original Shield shared texture context is not connected");draw.textures=textureContext->Capture(draw.uniforms.effects);
  // Exactly one ordinal for each original instanced DrawArrays, including its
  // original small/large iteration order and every instance within that batch.
  recorder.Record(std::move(draw));
 }
};
std::shared_ptr<Program> CreateProgram(const std::shared_ptr<Context>& context,const sol::table& specification,const std::string& name)
{
 Check(name=="ShieldSphereColorGL4"&&ConnectedConsumerAvailable(),"Original Shield requires its connected current31657 program");
 Check(Hash(specification.get<std::string>("vertex"))==NativeShieldVertexHash,"Original current31657 Shield expanded vertex identity differs");
 const auto hash=Hash(specification.get<std::string>("fragment"));const auto variant=std::find(NativeShieldFragmentHashes.begin(),NativeShieldFragmentHashes.end(),hash);
 Check(variant!=NativeShieldFragmentHashes.end(),"Original current31657 Shield expanded fragment/binding6 identity differs");
 for(const auto& item:specification){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt","Original Shield unexpected shader stage/initializer");}
 bool map=false,models=false;
 for(const auto& item:specification.get<sol::table>("uniformInt")){
  const auto key=item.first.as<std::string>();const int value=item.second.as<int>();
  if(key=="mapDepthTex"){Check(value==0,"Original Shield map sampler unit differs");map=true;}
  else if(key=="modelsDepthTex"){Check(value==1,"Original Shield model sampler unit differs");models=true;}
  else throw std::runtime_error("Original Shield unknown sampler initializer");
 }
 Check(map&&models,"Original Shield actual sampler initialization missing");
 auto result=std::make_shared<Program>();result->context=context;result->clipZeroToOne=variant-NativeShieldFragmentHashes.begin();return result;
}
sol::table CreateContext(lua_State* L)
{
 Check(!CLuaHandle::GetHandleSynced(L)&&ConnectedConsumerAvailable(),"Original Shield requires unsynced connected consumer/shared state");
 sol::state_view lua(L);auto table=lua.create_table();auto context=std::make_shared<Context>();
 std::erase_if(contexts,[](const auto& weak){return weak.expired();});contexts.emplace_back(context);
 Bind(table,"GetVBO",[L,context](sol::optional<GLenum> target,sol::optional<bool> frequent){auto result=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));result->context=context;return result;});
 Bind(table,"GetVAO",[context](){auto result=std::make_shared<VertexArray>();result->context=context;return result;});
 Bind(table,"CreateProgram",[context](const sol::table& specification,const std::string& name){return CreateProgram(context,specification,name);});
 Bind(table,"Blending",[](const std::string& value){Check(value=="alpha","Original Shield alpha blending differs");ui::GetRecorder().SetBlend(ScreenBlendState{});});
 Bind(table,"DepthTest",[](sol::object value){
  bool enabled;if(value.is<bool>())enabled=value.as<bool>();else{Check(value.as<GLenum>()==GL_LEQUAL,"Original Shield depth compare differs");enabled=true;}
  ui::GetRecorder().UpdateWorldState([enabled](auto& state){state.depthTest=enabled;state.depthCompare=ScreenAlphaCompare::LessEqual;});
 });
 Bind(table,"DepthMask",[](bool value){ui::GetRecorder().UpdateWorldState([value](auto& state){state.depthWrite=value;});});
 Bind(table,"Texture",[](int unit,sol::object value){
  Check((unit==0||unit==1)&&textureContext,"Original Shield actual depth texture context/unit differs");
  return value.is<bool>()?textureContext->Texture(unit,value.as<bool>()):textureContext->Texture(unit,value.as<std::string>());
 });
 return table;
}
} // namespace
void SetTextureContext(const rendering::NativeShieldTextureContext* context){if(context)context->Validate();textureContext=context;}
void BeginWorldFrame(){recorder.Begin(globalRendering->drawFrame);}
std::shared_ptr<const rendering::NativeShieldFrame> EndWorldFrame(){return recorder.End();}
void AbortWorldFrame()noexcept{recorder.Abort();for(auto& weak:contexts)if(auto context=weak.lock())context->Reset();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State* L)
{
 sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();
 auto buffer=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(buffer,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){b.Define(count,layout);});
 Bind(buffer,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.Upload(data,attribute,offset,first,last);});
 Bind(buffer,"Delete",[](Buffer& b){b.Delete();});
 Bind(buffer,"BindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(true,binding,first,count,target);});
 Bind(buffer,"UnbindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(false,binding,first,count,target);});
 auto vao=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(vao,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.vertices&&b->target==GL_ARRAY_BUFFER,"Original Shield vertex attachment differs");a.vertices=std::move(b);});
 Bind(vao,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.instances&&b->target==GL_ARRAY_BUFFER,"Original Shield instance attachment differs");a.instances=std::move(b);});
 Bind(vao,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.indices&&b->target==GL_ELEMENT_ARRAY_BUFFER,"Original Shield helper index attachment differs");a.indices=std::move(b);});
 Bind(vao,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
 Bind(vao,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});
 auto program=family.new_usertype<Program>("Program",sol::no_constructor);
 Bind(program,"Initialize",[](Program& p){return p.Initialize();});Bind(program,"Activate",[](Program& p){p.Activate();});Bind(program,"Deactivate",[](Program& p){p.Deactivate();});Bind(program,"Finalize",[](Program& p){p.Finalize();});
 family.set("Buffer",sol::lua_nil,"VertexArray",sol::lua_nil,"Program",sol::lua_nil);
 Bind(family,"Available",&ConnectedConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){
  Check(!CLuaHandle::GetHandleSynced(L),"Original Shield native archive scope is unsynced-only");const auto& entry=Archive(path);
  CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source),"Original current31657 Shield archive source missing");
  Check(Hash(source)==entry.hash,"Original current31657 Shield archive source identity differs");return source;
 });
 Bind(family,"PrepareSource",[L](const std::string& path,std::string source){
  Check(!CLuaHandle::GetHandleSynced(L),"Original Shield native source scope is unsynced-only");const auto& entry=Archive(path);
  Check(entry.lua&&Hash(source)==entry.hash,"Original Shield Lua preprocessing requires exact original bytes");
  if(entry.hash==NativeShieldGadgetHash){
   // The one unsynced availability predicate is backed by the real compiled
   // paired family. Do not alter Platform or the original synced branch.
   constexpr std::string_view before="if Platform.glHaveGL4 ~= true then";
   const auto at=source.find(before);Check(at!=std::string::npos&&source.find(before,at+before.size())==std::string::npos,"Original Shield unsynced availability predicate differs");
   source.replace(at,before.size(),"if not gl.NativeShield.Available() then");
  }
  LuaUtils::TracyRemoveAlsoExtras(source.data());return source;
 });
 gl.set("NativeShield",family);return true;
}
} // namespace LuaNativeShield
#else
namespace LuaNativeShield {
void SetTextureContext(const rendering::NativeShieldTextureContext*){}
bool PushEntries(lua_State*){return true;}
void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeShieldFrame> EndWorldFrame(){return {};}
void AbortWorldFrame()noexcept{}void Reset(){}
}
#endif
