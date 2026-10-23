/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeSensorPreview.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "LuaInclude.h"
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "Lua/LuaVBOImpl.h"
#include "Game/UI/MiniMap.h"
#include "Rendering/Backend/NativeMiniMapCapture.h"
#include "Rendering/Backend/NativeMapTexturePublication.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeSensorPreviewPrograms.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include "System/FileSystem/FileHandler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
namespace LuaNativeSensorPreview {
namespace {
using namespace rendering;
Services services;
MinimapSink minimapSink;
std::shared_ptr<NativeSensorPreviewFrame> recording;
NativeSensorPreviewCallin currentCallin=NativeSensorPreviewCallin::World;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<typename Signature>struct LuaBoundary;
template<typename R,typename...Args>struct LuaBoundary<R(Args...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,Args...args)->R{
 try{return f(std::forward<Args>(args)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* name,F f){t.set_function(name,LuaBoundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Program;struct Buffer;struct Image;
struct StorageBinding {std::shared_ptr<Buffer> owner;NativeBufferRange range;};
struct Context {
 bool jammer=false;
 std::vector<std::weak_ptr<Buffer>> ownedBuffers;
 std::shared_ptr<Program> active;
 std::map<std::string,std::weak_ptr<Buffer>> buffers;
 std::map<unsigned,std::weak_ptr<Program>> programs;
 std::map<unsigned,StorageBinding> storage;
 std::map<uint64_t,std::shared_ptr<Image>> images;
 std::map<unsigned,std::pair<uint64_t,std::weak_ptr<Image>>> boundImages;
 std::array<bool,NativeCUSTextureHistory::Units> radarUnits{};
 NativeMiniMapCapture radarCapture;
 std::vector<std::shared_ptr<Image>> targets;
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
 // Source uploads define only these original bytes; unused capacity remains
 // unusable as proof of ray/circle data, even though it is retained in snapshots.
 size_t uploadedPrefix=0;
 uint64_t uploadedRevision=0;
 Buffer(lua_State* L,GLenum t,bool frequent):target(t){Check(t==GL_ARRAY_BUFFER||t==GL_ELEMENT_ARRAY_BUFFER||t==GL_SHADER_STORAGE_BUFFER,"31657 Preview original buffer target differs");value=std::make_shared<LuaVBOImpl>(t,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(value);}
 std::string Identity(){auto id=std::to_string(value->CaptureNativeSnapshot()->object);if(auto c=context.lock())c->buffers[id]=shared_from_this();return id;}
 void Delete(){value->Delete();deleted=true;uploadedPrefix=0;uploadedRevision=0;if(auto c=context.lock())std::erase_if(c->storage,[this](const auto& e){return e.second.owner.get()==this;});}
 size_t Upload(const sol::stack_table& data,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){
  // The original AlliedCircles upload uses -1 for all fields. Validate the
  // selected prefix policy before the real producer can mutate any byte.
  if(target==GL_SHADER_STORAGE_BUFFER)Check(offset.value_or(0)==0&&(attribute.value_or(-1)==-1||attribute.value_or(-1)==0),"31657 Preview selected storage uploads are whole-field prefixes");
  const auto result=value->Upload(data,attribute,offset,first,last);
  if(result&&target==GL_SHADER_STORAGE_BUFFER){uploadedPrefix=result;uploadedRevision=value->CaptureNativeSnapshot()->revision;}
  return result;
 }
 int Range(bool bind,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> t){
  Check(Available()&&!deleted&&target==GL_SHADER_STORAGE_BUFFER&&t.value_or(target)==GL_SHADER_STORAGE_BUFFER&&binding==5,"31657 Preview actual native binding5 differs");
  const auto snapshot=value->CaptureNativeSnapshot();const auto elements=snapshot->layout.elements;
  Check(snapshot->layout.kind==NativeBufferKind::Storage&&elements>0&&elements<=size_t(std::numeric_limits<int>::max()),"31657 Preview native storage elements invalid");
  const size_t offset=size_t(std::max(first.value_or(0),0)),used=size_t(std::clamp(count.value_or(int(elements)),1,int(elements)));
  Check(offset<=elements&&used<=elements-offset,"31657 Preview original storage offset/count out of range");
  auto c=context.lock();Check(bool(c),"31657 Preview bound storage lost context");if(!bind){c->storage.erase(binding);return int(binding);}
  auto range=value->CaptureNativeRange(binding,offset,elements-offset,6);c->storage[binding]={shared_from_this(),std::move(range)};return int(binding);
 }
};
struct Program {
 NativeSensorPreviewProgramState state;unsigned identity;std::string label;bool deleted=false;
 Program(unsigned id,std::string_view v,std::string_view f):state(v,f,services.compile),identity(id){}
};
struct Image {std::shared_ptr<NativePostprocessBinding> binding;std::weak_ptr<Context> context;};
std::shared_ptr<Program> Active(const std::shared_ptr<Context>& c){Check(c->active&&!c->active->deleted,"31657 Preview active original shader missing");return c->active;}
void Capturing(){Check(recording&&recording->sourceDrawFrame==globalRendering->drawFrame,"31657 Preview requires its actual original callin capture");}
NativeBufferRange CaptureStorage(const std::shared_ptr<Context>& c,const NativeSensorPreviewProgramSnapshot& p,size_t& definedBytes)
{
 const auto at=c->storage.find(5);Check(at!=c->storage.end()&&at->second.owner,"31657 Preview required original binding5 unobserved");const auto& b=*at->second.owner;const auto actual=b.value->CaptureNativeSnapshot();const auto& r=at->second.range;
 Check(!b.deleted&&r.snapshot&&r.snapshot->object==actual->object&&r.snapshot->revision==actual->revision,"31657 Preview binding5 must retain its actual bound generation");
 const bool uploaded=b.uploadedRevision==actual->revision;
 const bool allocationOnly=b.uploadedRevision==0&&b.uploadedPrefix==0&&actual->revision==1;
 Check(uploaded||allocationOnly,"31657 Preview bound storage changed without tracked original upload");
 definedBytes=uploaded&&r.offset<=b.uploadedPrefix?std::min(r.size,b.uploadedPrefix-r.offset):0;
 if(p.source->kind==NativeSensorPreviewProgramKind::Coverage){const float radius=p.uniforms.losParams[2];Check(uploaded&&std::isfinite(radius)&&radius>=1&&radius<=256&&std::floor(radius)==radius,"31657 Preview original ray upload/radius invalid");const size_t side=size_t(radius)+1;Check(side*side<=definedBytes/16,"31657 Preview actual ray upload lacks used header range");}
 // reserveAlliedCircles(0) defines storage but skips Upload. Actual zero winner
 // textures make the original shader return before any AlliedCircles read.
 // Preserve that allocation without manufacturing an upload or defined bytes.
 return r;
}
void AppendCommand(const std::shared_ptr<Context>& c,NativeSensorPreviewCommand command)
{
 Capturing();command.sourceDrawFrame=recording->sourceDrawFrame;command.callin=currentCallin;
 if(c->targets.empty()){Check(command.kind==NativeSensorPreviewCommandKind::Draw,"31657 Preview clear requires its original RTT");recording->commands.push_back(std::move(command));return;}
 Check(currentCallin==NativeSensorPreviewCallin::World,"31657 Preview RTT producer is outside actual DrawWorld");
 auto destination=c->targets.back()->binding;Check(destination&&!destination->deleted,"31657 Preview RTT destination deleted");command.previous=destination->current;
 std::vector<std::shared_ptr<const NativePostprocessVersion>> inputs;
 if(command.draw){const auto mask=NativeSensorPreviewRequiredContentsMask(command.draw->program);for(unsigned i=0;i<7;++i)if(mask&(1u<<i)){const auto& input=command.draw->images[i].version;if(input&&std::find(inputs.begin(),inputs.end(),input)==inputs.end())inputs.push_back(input);}}
 // The shared bridge must call the existing strong PublishExternalOutput
 // transaction with explicit Sensor family. Append precedes its publication.
 const auto published=services.publishTarget(destination,command.sequence,inputs,[command=std::move(command)](std::shared_ptr<const NativePostprocessVersion> proposed)mutable{
  Check(proposed&&proposed->image&&proposed->generation,"31657 Preview proposed target generation missing");command.output=std::move(proposed);recording->commands.push_back(std::move(command));
 });Check(published&&destination->current==published,"31657 Preview shared target transaction failed to publish actual output");
}
struct VertexArray {
 std::shared_ptr<Context> context;std::shared_ptr<Buffer> vertices,instances,indices;bool deleted=false;
 void Draw(GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instanceCount,sol::optional<int> base){
  Check(Available()&&!deleted&&vertices&&mode==GL_TRIANGLES&&count.value_or(6)>=0&&first.value_or(0)>=0&&instanceCount.value_or(1)==1&&base.value_or(0)==0,"31657 Preview original quad draw differs");auto p=Active(context);const bool minimap=p->state.Capture().source->kind==NativeSensorPreviewProgramKind::Minimap;if(!minimap)Capturing();auto draw=std::make_shared<NativeSensorPreviewDraw>();
  draw->sourceDrawFrame=globalRendering->drawFrame;draw->sequence=NextNativeWorldDrawSequence();draw->callin=minimap?NativeSensorPreviewCallin::Minimap:currentCallin;draw->program=p->state.Capture();draw->vertices=vertices->value->CaptureNativeSnapshot();draw->first=first.value_or(0);draw->count=count.value_or(draw->vertices->layout.elements);
  auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"31657 Preview shader quad during list compilation");draw->surface=common.CurrentState().draw;draw->raster=common.CurrentWorldState();draw->original=OriginalCUSUniformCapture().Current();
  const auto kind=draw->program.source->kind;using K=NativeSensorPreviewProgramKind;
  Check((kind==K::Mip||kind==K::Coverage||kind==K::Smooth||kind==K::JammerDisc)==!context->targets.empty(),"31657 Preview pass original RTT/final attachment differs");
  if(NativeSensorPreviewRequiresStorage(draw->program))draw->storage=CaptureStorage(context,draw->program,draw->storageDefinedBytes);
  const auto mask=NativeSensorPreviewRequiredTextureMask(draw->program);
  for(unsigned i=0;i<7;++i)if(mask&(1u<<i)){auto& image=draw->images[i];image.effectiveUnit=draw->program.samplerUnits[i];Check(image.effectiveUnit>=0&&image.effectiveUnit<int(NativeCUSTextureHistory::Units),"31657 Preview original effective sampler unit invalid");if(context->radarUnits[image.effectiveUnit]){Check(::minimap!=nullptr,"31657 Preview radar suffix requires its actual minimap/viewer owner");image.radarSource=context->radarCapture.CapturePiP(::minimap->GetNativeState());continue;}image.binding=OriginalNativeTextureContext().History().Binding(image.effectiveUnit,NativeCUSTextureTarget::Image2D);image.dependency=OriginalNativeTextureContext().UI().DescribeBinding(image.binding).image;if(image.dependency)image.version=image.dependency->postprocessVersion;const auto known=context->boundImages.find(image.effectiveUnit);if(known!=context->boundImages.end()&&image.binding.object&&known->second.first==image.binding.object->identity)if(auto owner=known->second.second.lock()){Check(owner->binding&&!owner->binding->deleted,"31657 Preview actual allocation owner deleted");image.allocation=owner->binding->image;if(image.version)Check(image.version->image==image.allocation,"31657 Preview actual version/allocation ownership differs");}}
  ValidateNativeSensorPreviewDraw(*draw);if(minimap){Check(bool(minimapSink)&&context->targets.empty(),"31657 Preview actual cached minimap marker sink unavailable");minimapSink(common,std::move(draw));return;}NativeSensorPreviewCommand command;command.kind=NativeSensorPreviewCommandKind::Draw;command.sequence=draw->sequence;command.draw=std::move(draw);command.surface=command.draw->surface;command.raster=command.draw->raster;AppendCommand(context,std::move(command));
 }
};
void Clear(const std::shared_ptr<Context>& c,unsigned bits,sol::variadic_args args)
{
 Capturing();Check(bits==GL_COLOR_BUFFER_BIT&&args.size()==4&&!c->targets.empty(),"31657 Preview original RTT clear differs");NativeSensorPreviewCommand command;command.kind=NativeSensorPreviewCommandKind::Clear;command.sequence=NextNativeWorldDrawSequence();unsigned i=0;for(auto v:args)command.clearColor[i++]=v.as<float>();auto& common=ui::GetRecorder();command.surface=common.CurrentState().draw;command.raster=common.CurrentWorldState();AppendCommand(c,std::move(command));
}
std::shared_ptr<Image> CreateImage(const std::shared_ptr<Context>& c,double width,double height,sol::table options)
{
 Check(Available()&&std::isfinite(width)&&std::isfinite(height)&&width>=1&&height>=1&&std::floor(width)==width&&std::floor(height)==height&&width<=uint32_t(-1)&&height<=uint32_t(-1),"31657 Preview original image dimensions invalid");
 for(const auto& item:options){const auto key=item.first.as<std::string>();Check(key=="format"||key=="fbo"||key=="min_filter"||key=="mag_filter"||key=="wrap_s"||key=="wrap_t","31657 Preview original image option differs");}
 const unsigned format=options["format"].get<unsigned>(),minFilter=options["min_filter"].get<unsigned>(),magFilter=options["mag_filter"].get<unsigned>();
 const bool nearest=minFilter==GL_NEAREST&&magFilter==GL_NEAREST;
 const bool jammerFormat=(format==GL_R8&&nearest)||(format==0x822D&&(nearest||(minFilter==GL_LINEAR&&magFilter==GL_LINEAR)));
 Check((c->jammer?jammerFormat:((format==0x822D||format==0x822E)&&nearest))&&options["fbo"].get<bool>()&&options["wrap_s"].get<unsigned>()==GL_CLAMP_TO_EDGE&&options["wrap_t"].get<unsigned>()==GL_CLAMP_TO_EDGE,"31657 Preview original family format/filter/CLAMP FBO contract differs");
 NativePostprocessSampler sampler{minFilter==GL_LINEAR?NativePostprocessFilter::Linear:NativePostprocessFilter::Nearest,magFilter==GL_LINEAR?NativePostprocessFilter::Linear:NativePostprocessFilter::Nearest,NativePostprocessWrap::ClampEdge,NativePostprocessWrap::ClampEdge};
 auto result=std::make_shared<Image>();result->context=c;result->binding=services.createImage(uint32_t(width),uint32_t(height),format==GL_R8?ImageFormat::R8Unorm:format==0x822D?ImageFormat::R16Float:ImageFormat::R32Float,sampler);
 Check(result->binding&&result->binding->image&&!result->binding->current&&result->binding->image->width==uint32_t(width)&&result->binding->image->height==uint32_t(height)&&result->binding->image->renderToTexture&&result->binding->image->sampler==sampler,"31657 Preview actual allocation service returned different storage or invented contents");
 OriginalNativeTextureContext().UI().CreatePostprocess(result->binding,"Current31657 Sensor Preview original float RTT");c->images.emplace(result->binding->image->object,result);return result;
}
void DeleteImage(const std::shared_ptr<Context>& c,const std::shared_ptr<Image>& image)
{
 if(!image||!image->binding||image->binding->deleted)return;OriginalNativeTextureContext().UI().DeletePostprocess(image->binding);image->binding->deleted=true;c->images.erase(image->binding->image->object);
}
void BindImage(unsigned unit,const std::shared_ptr<Image>& image)
{
 Check(image&&image->binding&&!image->binding->deleted,"31657 Preview actual image owner deleted");auto& common=ui::GetRecorder();const auto actual=OriginalNativeTextureContext().UI().BindPostprocess(unit,image->binding);auto c=image->context.lock();Check(bool(c),"31657 Preview bound image lost original context");const auto observed=OriginalNativeTextureContext().History().Binding(actual,NativeCUSTextureTarget::Image2D);Check(bool(observed.object),"31657 Preview native image bind did not install actual registry object");c->radarUnits[actual]=false;c->boundImages[actual]={observed.object->identity,image};if(actual<2)common.SyncObservedTextureBinding(actual);
}
bool BindOriginalTexture(const std::shared_ptr<Context>& c,unsigned unit,sol::object value)
{
 Check(unit<NativeCUSTextureHistory::Units,"31657 Preview original texture unit exceeds its real family");
 auto& common=ui::GetRecorder();Check(!common.IsCompiling(),"31657 Preview original image binding during list compilation");
 auto& textures=OriginalNativeTextureContext();
 if(value.is<bool>()){
  const auto actual=textures.UI().LuaEnable(unit,value.as<bool>());if(actual<2)common.SyncObservedTextureBinding(actual);
  return true; // Original false disables the target, retaining its shader binding.
 }
 const auto name=value.as<std::string>();const auto active=textures.History().ActiveUnit();
 Check(bool(active),"31657 Preview original active texture unit unobserved");const auto actual=unit?unit:*active;
 Check(actual<c->radarUnits.size(),"31657 Preview actual texture unit exceeds its family");
 if(name=="$info:radar"){
  // The native handler has no global suffix object. This private binding uses
  // the SAME real LOS/radar/jammer source and existing MiniMapGPU producer as PiP.
  // Capture immutable viewer inputs at the actual shader draw, like a live GL image.
  c->radarUnits[actual]=true;c->boundImages.erase(actual);
  textures.UI().LuaEnable(unit,true);if(actual<2)common.SyncObservedTextureBinding(actual);return true;
 }
 NativeTextureLookup lookup;
 if(name=="$heightmap")lookup=OriginalNativeMapTextures().Lookup(name);
 else {
  Check(name=="$map_gbuffer_zvaltex"||name=="$model_gbuffer_zvaltex","31657 Preview original engine image name differs");
  const auto role=name=="$map_gbuffer_zvaltex"?NativeGeometryTextureRole::Map:NativeGeometryTextureRole::Models;
  const auto logical=textures.Geometry(role).Name(NativeGeometryTextureNames::DepthAttachment);
  lookup={true,textures.Registry().Find(logical),false};
 }
 Check(lookup.parsed&&lookup.object,"31657 Preview original engine image producer is unavailable");
 textures.Registry().LuaTexture(unit,lookup);c->radarUnits[actual]=false;c->boundImages.erase(actual);
 if(actual<2)common.SyncObservedTextureBinding(actual);return true;
}
void RenderToTexture(const std::shared_ptr<Context>& c,const std::shared_ptr<Image>& image,const sol::protected_function& callback,sol::variadic_args args)
{
 Capturing();Check(Available()&&currentCallin==NativeSensorPreviewCallin::World&&image&&image->binding&&!image->binding->deleted,"31657 Preview original RTT scope unavailable");
 c->targets.push_back(image);unsigned callbackCount=0;
 try{services.captureTarget(image->binding,[&]{Check(++callbackCount==1,"31657 Preview target bridge invoked original callback more than once");sol::protected_function_result result=callback(sol::as_args(args));if(!result.valid()){sol::error error=result;throw std::runtime_error(error.what());}});Check(callbackCount==1,"31657 Preview target bridge did not invoke original callback");}catch(...){c->targets.pop_back();throw;}
 c->targets.pop_back();
}
sol::table CreateContext(lua_State* L,sol::optional<bool> jammer)
{
 Check(!CLuaHandle::GetHandleSynced(L)&&Available(),"31657 Preview requires installed actual unsynced services");sol::state_view lua(L);auto table=lua.create_table();auto c=std::make_shared<Context>();c->jammer=jammer.value_or(false);std::erase_if(contexts,[](const auto& value){return value.expired();});contexts.push_back(c);
 Bind(table,"GetVBO",[L,c](sol::optional<GLenum> target,sol::optional<bool> frequent){auto b=std::make_shared<Buffer>(L,target.value_or(GL_ARRAY_BUFFER),frequent.value_or(false));b->context=c;c->ownedBuffers.push_back(b);return b;});Bind(table,"GetVAO",[c](){auto a=std::make_shared<VertexArray>();a->context=c;return a;});
 // Covers a real partial initialization too: these are only this private
 // context's observed allocations, never fabricated shader/image placeholders.
 Bind(table,"ReleaseContext",[c](){Check(c->jammer&&c->targets.empty(),"31657 Jammer cleanup overlaps an active target or another family");
  while(!c->images.empty()){auto image=c->images.begin()->second;c->images.erase(c->images.begin());DeleteImage(c,image);}
  for(const auto& weak:c->ownedBuffers)if(auto b=weak.lock();b&&!b->deleted)b->Delete();
  for(const auto& [id,weak]:c->programs)if(auto p=weak.lock())p->deleted=true;
  c->active.reset();c->programs.clear();c->buffers.clear();c->ownedBuffers.clear();c->storage.clear();c->boundImages.clear();c->radarUnits={};
 });
 Bind(table,"CreateShader",[c](sol::table source){
  for(const auto& item:source){const auto key=item.first.as<std::string>();Check(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","31657 Preview original shader option differs");}
  auto p=std::make_shared<Program>(c->next++,source["vertex"].get<std::string>(),source["fragment"].get<std::string>());
  if(sol::optional<sol::table> values=source["uniformInt"])for(const auto& item:*values){const std::array<int32_t,1> v{item.second.as<int32_t>()};if(!p->state.Integers(p->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  if(sol::optional<sol::table> values=source["uniformFloat"])for(const auto& item:*values){std::vector<float> v;if(item.second.is<sol::table>()){auto a=item.second.as<sol::table>();for(unsigned i=1;i<=a.size();++i)v.push_back(a[i].get<float>());}else v.push_back(item.second.as<float>());if(!p->state.Floats(p->state.Location(item.first.as<std::string>()),v)&&!c->error)c->error=GL_INVALID_OPERATION;}
  Check(p->state.Capture().source->jammer==c->jammer,"31657 Preview original program belongs to another private family");c->programs[p->identity]=p;return std::tuple{p,p->identity};
 });
 Bind(table,"UseShader",[c](sol::object value){if(value.is<int>()&&value.as<int>()==0){c->active.reset();return true;}auto p=value.as<std::shared_ptr<Program>>();Check(p&&!p->deleted,"31657 Preview original shader deleted");c->active=std::move(p);return true;});
 Bind(table,"DeleteShader",[c](std::shared_ptr<Program> p){if(!p)return false;p->deleted=true;if(c->active==p)c->active.reset();return true;});Bind(table,"GetShaderLog",[](){return std::string{};});
 Bind(table,"GetUniformLocation",[](std::shared_ptr<Program> p,const std::string& name){Check(p&&!p->deleted,"31657 Preview uniform program deleted");return p->state.Location(name);});
 Bind(table,"GetActiveUniforms",[L](std::shared_ptr<Program> p){Check(p&&!p->deleted,"31657 Preview reflection program deleted");sol::state_view lua(L);auto out=lua.create_table();unsigned i=0;for(auto name:p->state.ActiveUniforms()){auto row=lua.create_table();row["name"]=name=="alliedCircles"?std::string("alliedCircles[0]"):std::string(name);row["size"]=name=="alliedCircles"?192:1;row["type"]=name.ends_with("Tex")||name=="mapDepths"||name=="modelDepths"?GL_SAMPLER_2D:name=="coverageAbsolute"||name=="alliedCirclePass"||name=="alliedCircles"||name=="previewInactive"||name=="stippleOffset"||name=="circleMode"||name=="alliedCircleCount"?GL_FLOAT:name=="emitterXZ"||name=="alliedParams"?GL_FLOAT_VEC2:name=="inactiveParams"?GL_FLOAT_VEC3:GL_FLOAT_VEC4;out[++i]=row;}return out;});
 Bind(table,"Uniform",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<float> v;for(auto item:args)v.push_back(item.as<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformArray",[c](int location,unsigned type,const sol::stack_table& values){auto p=Active(c);Check(c->jammer&&type==2&&location==p->state.Location("alliedCircles"),"31657 Jammer original float-array call differs");std::vector<float> v;v.reserve(values.size());for(unsigned i=1;i<=values.size();++i)v.push_back(values[i].get<float>());if(!p->state.Floats(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});
 Bind(table,"UniformInt",[c](int location,sol::variadic_args args){auto p=Active(c);if(location==-1)return;std::vector<int32_t> v;for(auto item:args)v.push_back(item.as<int32_t>());if(!p->state.Integers(location,v)&&!c->error)c->error=GL_INVALID_OPERATION;});Bind(table,"GetError",[c](){return std::exchange(c->error,0);});
 Bind(table,"CreateTexture",[c](double w,double h,sol::table options){return CreateImage(c,w,h,options);});Bind(table,"DeleteTexture",[c](const std::shared_ptr<Image>& image){DeleteImage(c,image);return true;});Bind(table,"BindImage",&BindImage);Bind(table,"BindOriginalTexture",[c](unsigned unit,sol::object value){return BindOriginalTexture(c,unit,std::move(value));});
 Bind(table,"RenderToTexture",[c](const std::shared_ptr<Image>& image,const sol::protected_function& callback,sol::variadic_args args){RenderToTexture(c,image,callback,args);});Bind(table,"Clear",[c](unsigned bits,sol::variadic_args args){Clear(c,bits,args);});
 Bind(table,"ObjectLabel",[c](GLenum type,sol::object id,const std::string& label){Check(label.size()<=1024,"31657 Preview label exceeds bound");if(type==0x82e0){auto at=c->buffers.find(id.as<std::string>());Check(at!=c->buffers.end(),"31657 Preview labelled buffer missing");auto b=at->second.lock();Check(bool(b),"31657 Preview labelled buffer deleted");b->label=label;}else{Check(type==GL_PROGRAM,"31657 Preview label kind differs");auto at=c->programs.find(id.as<unsigned>());Check(at!=c->programs.end(),"31657 Preview labelled program missing");auto p=at->second.lock();Check(bool(p),"31657 Preview labelled program deleted");p->label=label;}});
 Bind(table,"PushDebugGroup",[c](unsigned,const std::string& label){c->groups.push_back(label);});Bind(table,"PopDebugGroup",[c](){Check(!c->groups.empty(),"31657 Preview debug group underflow");c->groups.pop_back();});return table;
}
}
void SetMinimapSink(MinimapSink actual){minimapSink=std::move(actual);}
void SetServices(Services actual){Reset();services=std::move(actual);}
bool Available(){return bool(services.compile)&&bool(services.createImage)&&bool(services.captureTarget)&&bool(services.publishTarget)&&bool(minimapSink);}
void BeginCapture(NativeSensorPreviewCallin callin){Check(Available()&&!recording,"31657 Preview duplicate capture or actual services unavailable");recording=std::make_shared<NativeSensorPreviewFrame>();recording->sourceDrawFrame=globalRendering->drawFrame;currentCallin=callin;}
std::shared_ptr<const NativeSensorPreviewFrame> EndCapture(){Capturing();return std::exchange(recording,{});}
void AbortCapture()noexcept{recording.reset();for(auto& weak:contexts)if(auto c=weak.lock()){c->active.reset();c->targets.clear();}}
void Reset(){AbortCapture();for(auto& weak:contexts)if(auto c=weak.lock()){c->storage.clear();c->boundImages.clear();c->radarUnits={};c->radarCapture.Reset();for(auto& [id,wp]:c->programs)if(auto p=wp.lock())p->deleted=true;}}
bool PushEntries(lua_State* L)
{
 sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();auto b=family.new_usertype<Buffer>("Buffer",sol::no_constructor);
 Bind(b,"Define",[](Buffer& b,int n,sol::optional<sol::object> layout){b.value->Define(n,layout);b.uploadedPrefix=0;b.uploadedRevision=0;});Bind(b,"Upload",[](Buffer& b,const sol::stack_table& data,sol::optional<int> attr,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){return b.Upload(data,attr,offset,first,last);});Bind(b,"Delete",[](Buffer& b){b.Delete();});Bind(b,"GetBufferSize",[](Buffer& b){return b.value->GetBufferSize();});Bind(b,"GetID",[](Buffer& b){return b.Identity();});Bind(b,"GetLabel",[](Buffer& b){return b.label;});
 Bind(b,"BindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(true,binding,first,count,target);});Bind(b,"UnbindBufferRange",[](Buffer& b,GLuint binding,sol::optional<int> first,sol::optional<int> count,sol::optional<GLenum> target){return b.Range(false,binding,first,count,target);});
 family.new_usertype<Program>("Program",sol::no_constructor);family.new_usertype<Image>("Image",sol::no_constructor);auto a=family.new_usertype<VertexArray>("VertexArray",sol::no_constructor);
 Bind(a,"AttachVertexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.vertices,"31657 Preview vertex attachment differs");a.vertices=std::move(b);});Bind(a,"AttachInstanceBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.instances,"31657 Preview placeholder instance attachment differs");a.instances=std::move(b);});Bind(a,"AttachIndexBuffer",[](VertexArray& a,std::shared_ptr<Buffer> b){Check(!a.deleted&&b&&!a.indices,"31657 Preview placeholder index attachment differs");a.indices=std::move(b);});Bind(a,"Delete",[](VertexArray& a){a.deleted=true;a.vertices.reset();a.instances.reset();a.indices.reset();});Bind(a,"DrawArrays",[](VertexArray& a,GLenum mode,sol::optional<int> count,sol::optional<int> first,sol::optional<int> instances,sol::optional<int> base){a.Draw(mode,count,first,instances,base);});
 family.set("Buffer",sol::lua_nil,"Program",sol::lua_nil,"Image",sol::lua_nil,"VertexArray",sol::lua_nil);Bind(family,"Available",&Available);Bind(family,"CreateContext",[L](sol::optional<bool> jammer){return CreateContext(L,jammer);});Bind(family,"IsImage",[](sol::object value){return value.is<std::shared_ptr<Image>>();});
 Bind(family,"LoadArchiveSource",[L](const std::string& path){Check(!CLuaHandle::GetHandleSynced(L),"31657 Preview is unsynced-only");const auto& allowed=sensorPreviewProgram::ArchiveSources;const auto at=std::find_if(allowed.begin(),allowed.end(),[&](const auto& item){return item.path==path;});Check(at!=allowed.end(),"Unknown current31657 Preview original path");CFileHandler file(path,SPRING_VFS_ZIP);std::string source;Check(file.FileExists()&&file.LoadStringData(source)&&NativeSensorPreviewSourceHash(source)==at->hash,"31657 Preview original archive source identity differs");return source;});
 Bind(family,"PrepareSource",[](std::string source){const auto hash=NativeSensorPreviewSourceHash(source);const auto& allowed=sensorPreviewProgram::ArchiveSources;Check(std::any_of(allowed.begin(),allowed.end(),[&](const auto& item){return item.hash==hash;}),"31657 Preview preprocessing requires exact original source");LuaUtils::TracyRemoveAlsoExtras(source.data());return source;});gl.set("NativeSensorPreview",family);return true;
}
}
#else
namespace LuaNativeSensorPreview {
void SetMinimapSink(MinimapSink){}void SetServices(Services){}bool Available(){return false;}void BeginCapture(rendering::NativeSensorPreviewCallin){}
std::shared_ptr<const rendering::NativeSensorPreviewFrame> EndCapture(){return {};}
void AbortCapture()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}
}
#endif
