/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeBloom.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Lua/LuaHandle.h"
#include "LuaInclude.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeBloomContext.h"
#include "Rendering/Backend/NativeBloomPrograms.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Game/Camera.h"
#include "System/FileSystem/FileHandler.h"
#include <cmath>
#include <set>
namespace LuaNativeBloom {
namespace {
using namespace rendering;
NativeBloomRecorder recorder;
void Check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
template<typename S>struct Boundary;
template<typename R,typename...A>struct Boundary<R(A...)>{template<class F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,A...a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception&e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<class T,class F>void Bind(T&t,const char*n,F f){t.set_function(n,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner {
 uint64_t context=0;
 std::map<std::string,uint64_t,std::less<>>names;
 std::shared_ptr<NativeBloomContext>core;
 ~Owner(){auto&c=OriginalNativeTextureContext();if(c.Initialized()&&c.Registry().ContextGeneration()==context)for(auto&[_,name]:names)c.Registry().Delete(name);}
 NativeTextureContext&Actual(){auto&c=OriginalNativeTextureContext();Check(c.Initialized()&&c.Registry().ContextGeneration()==context,"Bloom crossed actual texture context lifetime");return c;}
};
std::vector<std::weak_ptr<Owner>>owners;
std::vector<float>Values(sol::variadic_args a){std::vector<float>v;if(a.size()==1&&a[0].is<sol::table>()){auto t=a[0].as<sol::table>();for(unsigned i=1;i<=t.size();++i)v.push_back(t.get<float>(i));}else for(auto x:a)v.push_back(x.as<float>());return v;}
sol::table CreateContext(lua_State*L){
 Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeBloomConsumerAvailable(),"Bloom requires unsynced genuine paired deferred producer/consumer");Check(globalRendering&&globalRendering->maxTextureSize>0,"Bloom requires actual native extent limit");auto o=std::make_shared<Owner>();auto&actual=OriginalNativeTextureContext();Check(actual.Initialized(),"Bloom has no original texture namespace");o->context=actual.Registry().ContextGeneration();sol::state_view host(L);sol::protected_function publicTexture=host["gl"]["Texture"];std::weak_ptr<Owner>weak=o;NativeBloomEvents events;
 events.created=[weak](const std::string&name,const auto&cell){auto o=weak.lock();Check(bool(o),"Bloom context owner disappeared");auto&r=o->Actual().Registry();auto object=r.Create(NativeCUSTextureTarget::Image2D,"OriginalBloomFBO:"+name);NativeTextureAllocation a;a.width=cell->image->width;a.height=cell->image->height;a.depth=a.levels=a.samples=1;a.originalInternalFormat=cell->image->format==NativeBloomFormat::RG11B10Float?0x8c3a:0x881a;r.Allocate(object->name,a);r.SetSampling(object->name,cell->image->sampling);try{o->names.emplace(name,object->name);}catch(...){r.Delete(object->name);throw;}};
 events.deleted=[weak](const std::string&name){if(auto o=weak.lock()){auto&context=OriginalNativeTextureContext();if(context.Initialized()&&context.Registry().ContextGeneration()==o->context)if(auto i=o->names.find(name);i!=o->names.end()){context.Registry().Delete(i->second);o->names.erase(i);}}};
 events.bind=[weak,publicTexture](unsigned unit,const std::string&name){auto o=weak.lock();Check(bool(o),"Bloom context owner disappeared");auto&i=o->Actual();auto it=o->names.find(name);if(it!=o->names.end()){const auto u=i.UI().LuaBind(unit,i.Registry().Find(it->second));ui::GetRecorder().SyncObservedTextureBinding(u);return;}
  auto role=name.starts_with("$model_gbuffer_")?NativeGeometryTextureRole::Models:NativeGeometryTextureRole::Map;unsigned attachment=name.ends_with("difftex")?1:name.ends_with("emittex")?3:5;if(name!="$model_gbuffer_difftex"&&name!="$model_gbuffer_emittex"&&name!="$model_gbuffer_zvaltex"&&name!="$map_gbuffer_zvaltex"){
   Check(publicTexture.valid(),"Bloom original external image parser unavailable");auto&r=ui::GetRecorder();r.LuaActiveTexture(unit);try{auto result=publicTexture(0,name);r.LuaActiveTexture(0);if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}}catch(...){r.LuaActiveTexture(0);throw;}return;
  }auto id=i.Geometry(role).Name(attachment);const auto u=i.UI().LuaLookup(unit,[&]{return NativeTextureLookup{true,id?i.Registry().Find(id):nullptr,false};});ui::GetRecorder().SyncObservedTextureBinding(u);
 };
 events.disable=[weak](unsigned unit){auto o=weak.lock();Check(bool(o),"Bloom context owner disappeared");const auto u=o->Actual().UI().LuaEnable(unit,false);ui::GetRecorder().SyncObservedTextureBinding(u);};
 events.binding=[weak](unsigned unit){auto o=weak.lock();Check(bool(o),"Bloom context owner disappeared");return o->Actual().History().Binding(unit,NativeCUSTextureTarget::Image2D);};
 events.publish=[weak](const std::string&name,std::shared_ptr<const NativeBloomImageVersion>v)->std::function<void()>{auto o=weak.lock();Check(bool(o),"Bloom publication context disappeared");auto&reg=o->Actual().Registry();auto i=o->names.find(name);Check(i!=o->names.end(),"Bloom output logical name missing");auto prior=reg.Find(i->second);auto backing=std::make_shared<NativeBloomImageBacking>();backing->version=v;backing->storageGeneration=v->image->identity;backing->contentVersion=v->generation;backing->sourceDrawFrame=v->sourceDrawFrame;backing->content=NativeTextureContent::ProducerVersion;auto expected=std::make_shared<std::shared_ptr<const NativeCUSTextureObject>>();std::function<void()>cancel=[weak,prior,expected]()noexcept{if(auto o=weak.lock()){auto&ctx=OriginalNativeTextureContext();if(ctx.Initialized()&&ctx.Registry().ContextGeneration()==o->context)ctx.Registry().CancelContentPublication(*expected,prior);}};reg.PublishContent(i->second,{},backing);*expected=reg.Find(i->second);return cancel;};
 events.state=[](){auto&r=ui::GetRecorder();auto stage=r.CurrentWorldStage();Check(stage&&*stage==NativeWorldDrawStage::World,"Bloom must run original later DrawWorld stage");return std::pair{r.CurrentState().draw,r.CurrentWorldState()};};events.sourceDrawFrame=[](){return uint64_t(globalRendering->drawFrame);};
 o->core=std::make_shared<NativeBloomContext>(recorder,uint32_t(globalRendering->maxTextureSize),std::move(events));std::erase_if(owners,[](auto&w){return w.expired();});owners.emplace_back(o);sol::state_view lua(L);auto table=lua.create_table();
 Bind(table,"CreateTexture",[o](float width,float height,const sol::table&opts){Check(std::isfinite(width)&&std::isfinite(height)&&width>0&&height>0&&width<=float(globalRendering->maxTextureSize)&&height<=float(globalRendering->maxTextureSize),"Invalid Bloom original GLsizei extent");for(auto&e:opts){auto key=e.first.as<std::string>();Check(key=="fbo"||key=="target"||key=="border"||key=="min_filter"||key=="mag_filter"||key=="wrap_s"||key=="wrap_t"||key=="format","Unknown original Bloom texture option");}Check(!opts.get_or("fbo",false),"Original31657 Bloom creates its FBO explicitly");auto format=opts.get<unsigned>("format");Check(format==0x8c3a||format==0x881a,"Unknown original31657 Bloom HDR format");Check(!opts["target"].valid()||opts.get<unsigned>("target")==0x0de1,"Original Bloom texture target differs");Check(!opts.get_or("border",false),"Bloom source requested a border image");NativeTextureSampling s;s.minFilter=opts.get<unsigned>("min_filter");s.magFilter=opts.get<unsigned>("mag_filter");s.wrapS=opts.get<unsigned>("wrap_s");s.wrapT=opts.get<unsigned>("wrap_t");return o->core->CreateTexture(uint32_t(std::trunc(width)),uint32_t(std::trunc(height)),format==0x8c3a?NativeBloomFormat::RG11B10Float:NativeBloomFormat::RGBA16Float,s);});
 Bind(table,"DeleteTexture",[o](sol::object name){if(name.is<std::string>())o->core->DeleteTexture(name.as<std::string>());});
 Bind(table,"CreateFBO",[o](const sol::table&opts){for(auto&e:opts){const auto key=e.first.as<std::string>();Check(key=="color0"||key=="drawbuffers","Unknown original31657 Bloom FBO option");}auto drawbuffers=opts.get<sol::table>("drawbuffers");Check(drawbuffers.size()==1&&drawbuffers.get<unsigned>(1)==0x8ce0,"Original31657 Bloom draw attachment differs");return o->core->CreateFBO(opts.get<std::string>("color0"));});
 Bind(table,"DeleteFBO",[o](std::shared_ptr<NativeBloomFBO>f){o->core->DeleteFBO(f);});
 Bind(table,"IsValidFBO",[](std::shared_ptr<NativeBloomFBO>f){return f&&!f->deleted&&f->color&&!f->color->deleted;});
 Bind(table,"RawBindFBO",[o](sol::object f,sol::optional<unsigned>target,sol::optional<uint64_t>raw)->sol::object{Check(!target||*target==0x8d40,"Original Bloom FBO target differs");if(!f.valid()||f.is<sol::lua_nil_t>()){o->core->RestoreFBO(raw.value_or(0));return sol::make_object(f.lua_state(),sol::lua_nil);}return sol::make_object(f.lua_state(),o->core->BindFBO(f.as<std::shared_ptr<NativeBloomFBO>>()));});
 Bind(table,"Texture",[o](unsigned unit,sol::object value){if(value.is<bool>()){Check(!value.as<bool>(),"Original Bloom only disables texture2D enables");o->core->DisableTexture(unit);}else o->core->Texture(unit,value.as<std::string>());return true;});
 Bind(table,"Draw",[o](unsigned mode,sol::table values){Check(mode==0x0004&&values.size()==24,"Original Bloom texrect vertex topology differs");std::array<std::array<float,4>,6>v;for(unsigned i=0;i<24;++i)v[i/4][i%4]=values.get<float>(i+1);o->core->Draw(v);});
 Bind(table,"CreateProgram",[o](const std::string&name,const std::string&vertex,const std::string&fragment,const sol::table&floats,const sol::table&samplers){auto[kind,defines]=bloomProgram::Parse(name,vertex,fragment);auto p=o->core->CreateProgram(kind,std::move(defines));for(auto&e:floats){auto key=e.first.as<std::string>();std::vector<float>v;if(e.second.is<sol::table>()){auto t=e.second.as<sol::table>();for(unsigned i=1;i<=t.size();++i)v.push_back(t.get<float>(i));}else v.push_back(e.second.as<float>());o->core->Uniform(p,key,v);}for(auto&e:samplers){const std::string key=e.first.as<std::string>();const unsigned value=e.second.as<unsigned>();std::map<std::string,unsigned>expected;
 switch(kind){case NativeBloomProgram::Bright:expected={{"modelDiffuseTex",0},{"modelEmitTex",1},{"modelDepthTex",2},{"mapDepthTex",3}};break;
 case NativeBloomProgram::Downsample:case NativeBloomProgram::Upsample:expected={{"source",0}};break;
 case NativeBloomProgram::FinalUpsample:expected={{"source",0},{"brightTex",1},{"historyTex",2}};break;
 case NativeBloomProgram::Combine:expected={{"texture0",0}};break;}
 if(kind==NativeBloomProgram::Combine&&key=="debugDraw"){Check(value<=1,"Original Bloom debugDraw differs");float scalar=float(value);o->core->Uniform(p,key,std::span<const float>(&scalar,1));continue;}
 auto found=expected.find(key);Check(found!=expected.end()&&value==found->second,"Original31657 Bloom sampler mapping differs");}
 return p;});
 Bind(table,"Activate",[o](std::shared_ptr<NativeBloomProgramState>p){o->core->Activate(std::move(p));});Bind(table,"Deactivate",[o](std::shared_ptr<NativeBloomProgramState>p){o->core->Deactivate(p);});Bind(table,"Finalize",[o](std::shared_ptr<NativeBloomProgramState>p){o->core->Finalize(p);});Bind(table,"ProgramUniform",[o](std::shared_ptr<NativeBloomProgramState>p,const std::string&name,sol::variadic_args a){auto v=Values(a);o->core->Uniform(p,name,v);return true;});
 Bind(table,"GetUniformLocation",[o](std::shared_ptr<NativeBloomProgramState>p,const std::string&name){return o->core->GetUniformLocation(p,name);});
 Bind(table,"Uniform",[o](int location,sol::variadic_args a){auto values=Values(a);o->core->UniformCurrent(location,values);});
 return table;
}
}
void BeginWorldFrame(){recorder.Begin(globalRendering->drawFrame,OriginalCUSUniformCapture().Current());}
std::shared_ptr<const NativeBloomFrame>EndWorldFrame(){return recorder.End();}
void AbortWorldFrame()noexcept{recorder.Abort();for(auto&w:owners)if(auto o=w.lock())o->core->Abort();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State*L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();family.new_usertype<NativeBloomFBO>("PrivateFBO",sol::no_constructor);family.new_usertype<NativeBloomProgramState>("PrivateProgram",sol::no_constructor);family.set("PrivateFBO",sol::lua_nil);family.set("PrivateProgram",sol::lua_nil);Bind(family,"Available",&IsNativeBloomConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});Bind(family,"LoadArchiveSource",[L](const std::string&path){Check(!CLuaHandle::GetHandleSynced(L),"Bloom archive source is unsynced only");const std::set<std::string>allowed{"luaui/widgets/gfx_bloom_shader_deferred.lua","modules/graphics/luashader.lua","modules/graphics/instancevbotable.lua"};Check(allowed.contains(path),"Unknown original Bloom Lua source");CFileHandler file(path,SPRING_VFS_ZIP);std::string text;Check(file.FileExists()&&file.LoadStringData(text),"Original Bloom archive source missing");return text;});Bind(family,"PrepareSource",[](std::string text){auto h=Hash(text);Check(h==bloomProgram::WidgetHash||h==bloomProgram::ShaderHelperHash||h==bloomProgram::GeometryHelperHash,"Original Bloom Lua source identity differs");return text;});gl.set("NativeBloom",family);return true;}
}
#else
namespace LuaNativeBloom {void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeBloomFrame>EndWorldFrame(){return{};}void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}}
#endif
