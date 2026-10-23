/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "LuaHandle.h"
#include "LuaInclude.h"
#include "LuaVBOImpl.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeWaterOverlayProgram.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include <cstring>
#include <map>

namespace LuaNativeWaterOverlay {
using namespace rendering;
inline void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<typename Signature>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return [f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename T,typename F>void Bind(T& t,const char* key,F f){t.set_function(key,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Buffer;
struct Program {NativeWaterOverlayInput input;bool deleted=false;};
struct Owner {std::shared_ptr<Program> active;std::map<std::string,std::weak_ptr<Buffer>> buffers;};
struct Buffer:std::enable_shared_from_this<Buffer> {std::shared_ptr<LuaVBOImpl> value;std::shared_ptr<Owner> owner;bool deleted=false;GLenum target=0;std::string label;};
inline sol::table Create(lua_State* L)
{
	Require(!CLuaHandle::GetHandleSynced(L),"Water Overlay context must be unsynced");
	sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
	auto type=context.new_usertype<Buffer>("WaterOverlayBuffer",sol::no_constructor);
	Bind(type,"Define",[](Buffer& b,int count,sol::optional<sol::object> layout){Require(!b.deleted,"Water Overlay buffer deleted");b.value->Define(count,layout);});
	Bind(type,"Upload",[](Buffer& b,const sol::stack_table& values,sol::optional<int> attribute,sol::optional<int> offset,sol::optional<int> first,sol::optional<int> last){Require(!b.deleted,"Water Overlay buffer deleted");return b.value->Upload(values,attribute,offset,first,last);});
	Bind(type,"Delete",[](Buffer& b){b.value->Delete();b.deleted=true;});context.set("WaterOverlayBuffer",sol::lua_nil);
	Bind(type,"GetID",[](Buffer& b){Require(!b.deleted,"Water Overlay buffer deleted");auto id=std::to_string(b.value->CaptureNativeSnapshot()->object);b.owner->buffers[id]=b.shared_from_this();return id;});
	Bind(context,"ObjectLabel",[owner](GLenum kind,const std::string& id,const std::string& label){Require(kind==0x82e0,"Water Overlay label must name a buffer");const auto it=owner->buffers.find(id);Require(it!=owner->buffers.end(),"Water Overlay unknown buffer identity");auto b=it->second.lock();Require(b&&!b->deleted,"Water Overlay deleted buffer label");b->label=label;});
	Bind(context,"GetVBO",[L,owner](GLenum target,bool frequent){Require((target==GL_ARRAY_BUFFER||target==GL_ELEMENT_ARRAY_BUFFER)&&!frequent,"Water Overlay original buffer target/usage differs");auto b=std::make_shared<Buffer>();b->owner=owner;b->target=target;b->value=std::make_shared<LuaVBOImpl>(target,frequent);CLuaHandle::GetActiveVBOs(L).luaVBOs.emplace_back(b->value);return b;});

 auto programType=context.new_usertype<Program>("WaterOverlayProgram",sol::no_constructor);context.set("WaterOverlayProgram",sol::lua_nil);
 Bind(context,"Compile",[](const sol::table& cache,const std::string& vertex,const std::string& fragment,const std::string& engine)->std::shared_ptr<Program>{
  unsigned count=0;for(const auto& e:cache){const auto k=e.first.as<std::string>();Require(k=="vssrcpath"||k=="fssrcpath"||k=="shaderName"||k=="uniformInt"||k=="uniformFloat"||k=="shaderConfig","Water Overlay unknown shader cache field");++count;}Require(count==6,"Water Overlay shader cache incomplete");
  const auto name=cache["shaderName"].get<std::string>();
  const bool fog=name=="Water Overlay Fog (lava)"||name=="Water Overlay Fog (acid)";
  const bool acid=name=="Water Overlay Fog (acid)"||name=="Water Overlay Surface (acid)";
  Require(fog||name=="Water Overlay Surface (lava)"||name=="Water Overlay Surface (acid)","Water Overlay unknown source program");
  const std::string prefix="shaders/GLSL/lava/";
  Require(cache["vssrcpath"].get<std::string>()==prefix+(fog?"lava_fog_light.vert.glsl":"lava.vert.glsl")&&cache["fssrcpath"].get<std::string>()==prefix+(fog?"lava_fog_light.frag.glsl":"lava.frag.glsl"),"Water Overlay shader path differs");
  Require(targetPriorityProgram::Hash(vertex)==waterOverlayProgram::SourceHashes[fog?2:0]&&targetPriorityProgram::Hash(fragment)==waterOverlayProgram::SourceHashes[fog?3:1]&&targetPriorityProgram::Hash(engine)==targetPriorityProgram::EngineDefinitionsHash,"Water Overlay original shader/engine source changed");
  const std::map<std::string,double> defines={
   {"HEIGHTOFFSET",2},{"COASTWIDTH",acid?16.:25.},{"WORLDUVSCALE",acid?3.:2.},{"SPECULAREXPONENT",acid?12.:64.},{"SPECULARSTRENGTH",1},{"LOSDARKNESS",.5},{"SHADOWSTRENGTH",.4},{"OUTOFMAPHEIGHT",-100},
   {"SWIRLFREQUENCY",acid?.008:.025},{"SWIRLAMPLITUDE",acid?.01:.003},{"PARALLAXDEPTH",acid?24.:16.},{"PARALLAXOFFSET",acid?.15:.5},{"GLOBALROTATEFREQUENCY",.0001},{"GLOBALROTATEAMPLIDUE",.05},{"FOGHEIGHTABOVELAVA",20},{"FOGFACTOR",acid?.1:.06},{"EXTRALIGHTCOAST",acid?.5:.6},{"FOGLIGHTDISTORTION",acid?1.:4.},{"FOGABOVELAVA",acid?.1:1.}};
  const std::map<std::string,std::string> textDefines={
   {"COASTCOLOR",acid?"vec3(0.3, 1.5, 0.2)":"vec3(2.0, 0.5, 0.0)"},
   {"FOGCOLOR",acid?"vec3(0.3, 1.2, 0.2)":"vec3(2.0, 0.5, 0.0)"},
   {"SWIZZLECOLORS",acid?"fragColor.rgb = (fragColor.rgb * vec3(0.15, 1.0, 0.45)).rgb;":"fragColor.rgb = (fragColor.rgb * vec3(1.0, 1.0, 1.0)).rgb;"}};
  count=0;for(const auto& e:cache["shaderConfig"].get<sol::table>()){const auto k=e.first.as<std::string>();auto n=defines.find(k);auto t=textDefines.find(k);Require(n!=defines.end()||t!=textDefines.end(),"Water Overlay unknown shader define");if(n!=defines.end())Require(e.second.as<double>()==n->second,"Water Overlay numeric define changed");else Require(e.second.as<std::string>()==t->second,"Water Overlay color define changed");++count;}Require(count==defines.size()+textDefines.size(),"Water Overlay defines incomplete");
  const std::vector<std::string> samplers=fog?std::vector<std::string>{"mapDepths","modelDepths","lavaDistortion"}:std::vector<std::string>{"heightmapTex","lavaDiffuseEmit","lavaNormalHeight","lavaDistortion","shadowTex","infoTex"};
  count=0;for(const auto& e:cache["uniformInt"].get<sol::table>()){const int slot=e.second.as<int>();Require(slot>=0&&size_t(slot)<samplers.size()&&e.first.as<std::string>()==samplers[slot],"Water Overlay original sampler differs");++count;}Require(count==samplers.size(),"Water Overlay sampler missing");
  count=0;for(const auto& e:cache["uniformFloat"].get<sol::table>()){const auto k=e.first.as<std::string>();Require((k=="lavaHeight"||k=="heatdistortx"||k=="heatdistortz")&&e.second.as<double>()==1.,"Water Overlay original uniform differs");++count;}Require(count==3,"Water Overlay uniforms missing");
  auto compiled=NativeWaterOverlayConsumer(fog);if(!compiled)return {};
  auto p=std::make_shared<Program>();p->input.compiled=std::move(compiled);p->input.acid=acid;p->input.fog=fog;return p;
 });
 Bind(context,"Activate",[owner](std::shared_ptr<Program> p){Require(p&&!p->deleted&&p->input.compiled==NativeWaterOverlayConsumer(p->input.fog),"Water Overlay original program unavailable");owner->active=p;});
 Bind(context,"Deactivate",[owner](std::shared_ptr<Program> p){Require(owner->active==p,"Water Overlay deactivation owner differs");owner->active.reset();});
 Bind(context,"Finalize",[owner](std::shared_ptr<Program> p){if(!p)return;if(owner->active==p)owner->active.reset();p->deleted=true;p->input.compiled.reset();});
 Bind(context,"SetUniform",[owner](std::shared_ptr<Program> p,const std::string& name,float value){Require(p&&owner->active==p&&!p->deleted&&std::isfinite(value),"Water Overlay active uniform missing/nonfinite");const int slot=name=="lavaHeight"?0:name=="heatdistortx"?1:name=="heatdistortz"?2:-1;Require(slot>=0,"Water Overlay unknown uniform");p->input.parameters[slot]=value;});
 Bind(context,"Draw",[owner](std::shared_ptr<Buffer> vertex,std::shared_ptr<Buffer> index,GLenum primitive,sol::optional<int> count,int first,int instances,int base){
  const auto p=owner->active;Require(p&&!p->deleted,"Water Overlay active program missing");
  Require(vertex&&index&&vertex->owner==owner&&index->owner==owner&&!vertex->deleted&&!index->deleted&&vertex->target==GL_ARRAY_BUFFER&&index->target==GL_ELEMENT_ARRAY_BUFFER,"Water Overlay original VAO buffers missing/deleted");
  Require(primitive==GL_TRIANGLES&&first==0&&instances==1&&base==0,"Water Overlay original draw invocation differs");
  const auto v=vertex->value->CaptureNativeSnapshot(),ix=index->value->CaptureNativeSnapshot();const auto& l=v->layout;
  Require(l.kind==NativeBufferKind::Vertex&&l.stride==8&&l.attributes.size()==1,"Water Overlay original vec2 plane layout differs");const auto& a=l.attributes[0];Require(a.id==0&&a.offset==0&&a.byteSize==8&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Water Overlay plane attribute differs");
  const auto& il=ix->layout;Require(il.kind==NativeBufferKind::Index&&il.stride==4&&il.attributes.size()==1&&il.attributes[0].scalarType==NativeScalarType::UInt32,"Water Overlay original indices differ");
  const auto n=count?size_t(*count):il.elements;Require((!count||*count>=0)&&n<=il.elements&&n%3==0&&n<=4*1024*1024,"Water Overlay index range invalid");
  std::vector<std::array<float,2>> positions;positions.reserve(n);for(size_t j=0;j<n;++j){uint32_t id;std::memcpy(&id,ix->bytes.data()+j*4,4);Require(id<l.elements,"Water Overlay index outside original plane");std::array<float,2> xy;std::memcpy(xy.data(),v->bytes.data()+size_t(id)*8,8);positions.push_back(xy);}
  auto input=std::make_shared<NativeWaterOverlayInput>(p->input);input->engine=OriginalCUSUniformCapture().Current();
  auto& textures=OriginalNativeTextureContext();Require(ui::GetRecorder().TextureObserver()==&textures.UI(),"Water Overlay actual texture observer missing");
  for(unsigned slot=0;slot<(input->fog?3u:4u);++slot){const auto image=textures.UI().BoundImage(slot);input->images[slot]={image.image,ScreenUVMode::Normalized,image.sampler};}
  if(!input->fog){
   input->typedImages[0]=textures.History().Binding(4,NativeCUSTextureTarget::Image2D);
   input->typedImages[1]=textures.History().Binding(5,NativeCUSTextureTarget::Image2D);
   const auto& b=input->typedImages[0];Require(b.object&&b.object->sampling.compareMode==0x884E&&b.object->sampling.compareFunction==0x0203,"Water Overlay actual shadow comparison differs");
   const auto f=b.object->allocation.originalInternalFormat;input->clampShadowReference=f==0x81A5||f==0x81A6;
   // Actual shadow resolver retains all original sampling/compare state;
   // its sampler does not pass through the ordinary non-comparison UI cache.
   input->typedSamplers[1]=ui::NativeUITextureHistory::DescribeSampler(input->typedImages[1]);
  }
  ValidateNativeWaterOverlay(*input);Require(globalRendering&&input->engine->matrices->sourceDrawFrame==uint64_t(globalRendering->drawFrame)&&input->engine->params->sourceDrawFrame==uint64_t(globalRendering->drawFrame),"Water Overlay current engine uniforms missing");
  ui::GetRecorder().AppendWaterOverlay(positions,std::move(input));
 });
 return context;
}
} // namespace LuaNativeWaterOverlay
