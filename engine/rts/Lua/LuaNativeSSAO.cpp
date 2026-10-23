/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeSSAO.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Lua/LuaHandle.h"
#include "Lua/LuaUtils.h"
#include "LuaInclude.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeSSAOContext.h"
#include "Rendering/Backend/NativeSSAOPrograms.h"
#include "Rendering/Backend/NativeInheritedClipPlane.h"
#include "Rendering/Backend/NativeCUSUniformCapture.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/GlobalRendering.h"
#include "Game/Camera.h"
#include "System/FileSystem/FileHandler.h"
#include <cmath>
#include <set>
namespace LuaNativeSSAO {
namespace {
using namespace rendering;
NativeSSAORecorder recorder;
void Check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
uint64_t Hash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
template<typename S>struct Boundary;
template<typename R,typename...A>struct Boundary<R(A...)>{template<class F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,A...a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception&e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<class T,class F>void Bind(T&t,const char*n,F f){t.set_function(n,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Owner {
 uint64_t context=0;
 std::map<std::string,uint64_t,std::less<>>names;
 std::shared_ptr<NativeSSAOContext>core;
 ~Owner(){auto&c=OriginalNativeTextureContext();if(c.Initialized()&&c.Registry().ContextGeneration()==context)for(auto&[_,name]:names)c.Registry().Delete(name);}
 NativeTextureContext&Actual(){auto&c=OriginalNativeTextureContext();Check(c.Initialized()&&c.Registry().ContextGeneration()==context,"SSAO crossed actual texture context lifetime");return c;}
};
std::vector<std::weak_ptr<Owner>>owners;
std::vector<float>Values(sol::variadic_args a){std::vector<float>v;if(a.size()==1&&a[0].is<sol::table>()){auto t=a[0].as<sol::table>();for(unsigned i=1;i<=t.size();++i)v.push_back(t.get<float>(i));}else for(auto x:a)v.push_back(x.as<float>());return v;}
NativeSSAOProgram Kind(const std::string&name,uint64_t hash){using namespace ssaoProgram;if(name=="SSAO G-buffer Fuse"&&hash==FuseHash)return NativeSSAOProgram::Fuse;if(name=="SSAO SSAO"&&hash==SampleHash)return NativeSSAOProgram::Sample;if(name=="SSAO gaussianBlur"&&hash==BlurHash)return NativeSSAOProgram::Blur;if(name=="SSAO: blur composite"&&hash==BlurHash)return NativeSSAOProgram::BlurComposite;if(name=="SSAO: texrect"&&hash==TexRectHash)return NativeSSAOProgram::TexRect;if(name=="SSAO: SSAO composite"&&hash==CompositeHash)return NativeSSAOProgram::Composite;throw std::runtime_error("Original SSAO shader source/name identity differs");}
sol::table CreateContext(lua_State*L){
 Check(!CLuaHandle::GetHandleSynced(L)&&IsNativeSSAOConsumerAvailable(),"SSAO requires unsynced genuine paired deferred producer/consumer");Check(globalRendering&&globalRendering->maxTextureSize>0,"SSAO requires actual native extent limit");auto o=std::make_shared<Owner>();auto&actual=OriginalNativeTextureContext();Check(actual.Initialized(),"SSAO has no original texture namespace");o->context=actual.Registry().ContextGeneration();sol::state_view host(L);sol::protected_function publicTexture=host["gl"]["Texture"];std::weak_ptr<Owner>weak=o;NativeSSAOEvents events;
 events.created=[weak](const std::string&name,const auto&cell){auto o=weak.lock();Check(bool(o),"SSAO context owner disappeared");auto&r=o->Actual().Registry();auto object=r.Create(NativeCUSTextureTarget::Image2D,"OriginalSSAOFBO:"+name);NativeTextureAllocation a;a.width=cell->image->width;a.height=cell->image->height;a.depth=a.levels=a.samples=1;a.originalInternalFormat=cell->image->format==NativeSSAOFormat::RGBA8?0x8058:0x881b;r.Allocate(object->name,a);r.SetSampling(object->name,cell->image->sampling);try{o->names.emplace(name,object->name);}catch(...){r.Delete(object->name);throw;}};
 events.deleted=[weak](const std::string&name){if(auto o=weak.lock()){auto&context=OriginalNativeTextureContext();if(context.Initialized()&&context.Registry().ContextGeneration()==o->context)if(auto i=o->names.find(name);i!=o->names.end()){context.Registry().Delete(i->second);o->names.erase(i);}}};
 events.bind=[weak,publicTexture](unsigned unit,const std::string&name){auto o=weak.lock();Check(bool(o),"SSAO context owner disappeared");auto&i=o->Actual();auto it=o->names.find(name);if(it!=o->names.end()){const auto u=i.UI().LuaBind(unit,i.Registry().Find(it->second));ui::GetRecorder().SyncObservedTextureBinding(u);return;}
  auto role=name.starts_with("$model_gbuffer_")?NativeGeometryTextureRole::Models:NativeGeometryTextureRole::Map;unsigned attachment=name.ends_with("normtex")?0:5;if(name!="$model_gbuffer_normtex"&&name!="$model_gbuffer_zvaltex"&&name!="$map_gbuffer_zvaltex"){
   Check(publicTexture.valid(),"SSAO original external image parser unavailable");auto&r=ui::GetRecorder();r.LuaActiveTexture(unit);try{auto result=publicTexture(0,name);r.LuaActiveTexture(0);if(!result.valid()){sol::error e=result;throw std::runtime_error(e.what());}}catch(...){r.LuaActiveTexture(0);throw;}return;
  }auto id=i.Geometry(role).Name(attachment);const auto u=i.UI().LuaLookup(unit,[&]{return NativeTextureLookup{true,id?i.Registry().Find(id):nullptr,false};});ui::GetRecorder().SyncObservedTextureBinding(u);
 };
 events.disable=[weak](unsigned unit){auto o=weak.lock();Check(bool(o),"SSAO context owner disappeared");const auto u=o->Actual().UI().LuaEnable(unit,false);ui::GetRecorder().SyncObservedTextureBinding(u);};
 events.binding=[weak](unsigned unit){auto o=weak.lock();Check(bool(o),"SSAO context owner disappeared");return o->Actual().History().Binding(unit,NativeCUSTextureTarget::Image2D);};
 events.publish=[weak](const std::string&name,std::shared_ptr<const NativeSSAOImageVersion>v)->std::function<void()>{auto o=weak.lock();Check(bool(o),"SSAO publication context disappeared");auto&reg=o->Actual().Registry();auto i=o->names.find(name);Check(i!=o->names.end(),"SSAO output logical name missing");auto prior=reg.Find(i->second);auto backing=std::make_shared<NativeSSAOImageBacking>();backing->version=v;backing->storageGeneration=v->image->identity;backing->contentVersion=v->generation;backing->sourceDrawFrame=v->sourceDrawFrame;backing->content=NativeTextureContent::ProducerVersion;auto expected=std::make_shared<std::shared_ptr<const NativeCUSTextureObject>>();std::function<void()>cancel=[weak,prior,expected]()noexcept{if(auto o=weak.lock()){auto&ctx=OriginalNativeTextureContext();if(ctx.Initialized()&&ctx.Registry().ContextGeneration()==o->context)ctx.Registry().CancelContentPublication(*expected,prior);}};reg.PublishContent(i->second,{},backing);*expected=reg.Find(i->second);return cancel;};
 events.state=[](){auto&r=ui::GetRecorder();auto stage=r.CurrentWorldStage();Check(stage&&(*stage==NativeWorldDrawStage::AlphaAboveWater||*stage==NativeWorldDrawStage::AlphaAll),"SSAO must run original above-water DrawWorldPreParticles stage");return std::pair{r.CurrentState().draw,r.CurrentWorldState()};};events.sourceDrawFrame=[](){return uint64_t(globalRendering->drawFrame);};
 o->core=std::make_shared<NativeSSAOContext>(recorder,uint32_t(globalRendering->maxTextureSize),std::move(events));std::erase_if(owners,[](auto&w){return w.expired();});owners.emplace_back(o);sol::state_view lua(L);auto table=lua.create_table();
 Bind(table,"CreateTexture",[o](float width,float height,const sol::table&opts){Check(std::isfinite(width)&&std::isfinite(height)&&width>0&&height>0&&width<=float(globalRendering->maxTextureSize)&&height<=float(globalRendering->maxTextureSize),"Invalid SSAO original GLsizei extent");for(auto&e:opts){auto key=e.first.as<std::string>();Check(key=="target"||key=="border"||key=="min_filter"||key=="mag_filter"||key=="wrap_s"||key=="wrap_t"||key=="format","Unknown original SSAO texture option");}auto format=opts.get<unsigned>("format");Check(format==0x8058||format==0x881b,"Unknown original SSAO FBO format");Check(!opts["target"].valid()||opts.get<unsigned>("target")==0x0de1,"Original SSAO texture target differs");Check(!opts.get_or("border",false),"SSAO source requested a border image");NativeTextureSampling s;s.minFilter=opts.get<unsigned>("min_filter");s.magFilter=opts.get<unsigned>("mag_filter");s.wrapS=opts.get<unsigned>("wrap_s");s.wrapT=opts.get<unsigned>("wrap_t");return o->core->CreateTexture(uint32_t(std::trunc(width)),uint32_t(std::trunc(height)),format==0x8058?NativeSSAOFormat::RGBA8:NativeSSAOFormat::RGB16Float,s);});
 Bind(table,"DeleteTexture",[o](sol::object name){if(name.is<std::string>())o->core->DeleteTexture(name.as<std::string>());});
 Bind(table,"CreateFBO",[o](const sol::table&opts){for(auto&e:opts){auto k=e.first.as<std::string>();Check(k=="color0"||k=="drawbuffers","Unknown SSAO original FBO attachment");}auto d=opts.get<sol::table>("drawbuffers");Check(d.size()==1&&d.get<unsigned>(1)==0x8ce0,"Original SSAO FBO draw buffer differs");return o->core->CreateFBO(opts.get<std::string>("color0"));});
 Bind(table,"DeleteFBO",[o](std::shared_ptr<NativeSSAOFBO>f){o->core->DeleteFBO(f);});Bind(table,"IsValidFBO",[](std::shared_ptr<NativeSSAOFBO>f){return f&&!f->deleted&&f->color&&!f->color->deleted;});
 Bind(table,"RawBindFBO",[o](sol::object f,sol::optional<unsigned>target,sol::optional<uint64_t>raw)->sol::object{Check(!target||*target==0x8d40,"Original SSAO FBO target differs");if(!f.valid()||f.is<sol::lua_nil_t>()){o->core->RestoreFBO(raw.value_or(0));return sol::make_object(f.lua_state(),sol::lua_nil);}return sol::make_object(f.lua_state(),o->core->BindFBO(f.as<std::shared_ptr<NativeSSAOFBO>>()));});
 Bind(table,"Texture",[o](unsigned unit,sol::object value){if(value.is<bool>()){Check(!value.as<bool>(),"Original SSAO only disables texture2D enables");o->core->DisableTexture(unit);}else o->core->Texture(unit,value.as<std::string>());return true;});
 Bind(table,"Clear",[o](unsigned mask,float r,float g,float b,float a){Check(mask==0x4000,"Original SSAO only clears its color attachment");o->core->Clear({r,g,b,a});});
 Bind(table,"Draw",[o](unsigned mode,sol::table values){Check(mode==0x0004&&values.size()==24,"Original SSAO texrect vertex topology differs");std::array<std::array<float,4>,6>v;for(unsigned i=0;i<24;++i)v[i/4][i%4]=values.get<float>(i+1);o->core->Draw(v);});
 // Copy the original VAO uploads once. Frame draws retain this immutable geometry
 // even when the widget subsequently deletes or rebuilds its Lua buffers.
 Bind(table,"CaptureTiles",[](sol::table corners,sol::table indices,sol::table instances){
  Check(corners.size()==8&&indices.size()==6&&instances.size()>0&&instances.size()%2==0,"Original31657 SSAO tile upload shape differs");
  const auto limit=(uint64_t(globalRendering->maxTextureSize)+31)/32;
  Check(instances.size()/2<=limit*limit,"SSAO tile upload exceeds actual native extent");
  auto tiles=std::make_shared<NativeSSAOTileGeometry>();
  for(unsigned i=0;i<8;++i){float v=corners.get<float>(i+1);Check(std::isfinite(v),"SSAO tile corner is nonfinite");tiles->corners[i/2][i%2]=v;}
  for(unsigned i=0;i<6;++i){double v=indices.get<double>(i+1);Check(std::isfinite(v)&&v>=0&&v<4&&std::floor(v)==v,"SSAO tile index is outside captured corner data");tiles->indices[i]=uint32_t(v);}
  tiles->instances.resize(instances.size()/2);
  for(size_t i=0;i<instances.size();++i){float v=instances.get<float>(i+1);Check(std::isfinite(v)&&v>=0&&v<float(limit)&&std::floor(v)==v,"SSAO tile coordinate is outside actual extent");tiles->instances[i/2][i%2]=v;}
  return tiles;
 });
 Bind(table,"DrawTiles",[o](std::shared_ptr<NativeSSAOTileGeometry>tiles){
  Check(bool(tiles),"SSAO indexed draw lacks captured geometry");
  std::array<std::array<float,4>,6>vertices{};
  for(unsigned i=0;i<4;++i)vertices[i]={tiles->corners[i][0],tiles->corners[i][1],0,1};
  o->core->Draw(vertices,std::move(tiles));
 });
 Bind(table,"CreateProgram",[o](const std::string&name,const std::string&vertex,const std::string&fragment,const sol::table&config,const sol::table&floats,const sol::table&samplers){Check(Hash(vertex)==ssaoProgram::VertexHash,"Original SSAO vertex source differs");NativeSSAODefines d;for(auto&e:config){auto value=e.second;float v;if(value.is<std::string>()){const auto text=value.as<std::string>();Check(e.first.as<std::string>()=="DEPTH_CLIP01"&&(text=="0"||text=="1"),"Original SSAO string define differs");v=text=="1"?1:0;}else v=value.as<float>();d.emplace(e.first.as<std::string>(),v);}auto p=o->core->CreateProgram(Kind(name,Hash(fragment)),std::move(d));for(auto&e:floats){auto key=e.first.as<std::string>();std::vector<float>v;if(e.second.is<sol::table>()){auto t=e.second.as<sol::table>();for(unsigned i=1;i<=t.size();++i)v.push_back(t.get<float>(i));}else v.push_back(e.second.as<float>());o->core->Uniform(p,key,v);}for(auto&e:samplers){const std::map<std::string,unsigned>expected{{"modelDepthTex",1},{"mapDepthTex",4},{"unitStencilTex",7},{"viewPosTex",5},{"viewNormalTex",6},{"miscTex",2},{"modelNormalTex",0},{"mapNormalTex",3},{"tex",0}};auto it=expected.find(e.first.as<std::string>());Check(it!=expected.end()&&e.second.as<unsigned>()==it->second,"Original SSAO sampler mapping differs");}return p;});
 Bind(table,"Activate",[o](std::shared_ptr<NativeSSAOProgramState>p){o->core->Activate(std::move(p));});Bind(table,"Deactivate",[o](std::shared_ptr<NativeSSAOProgramState>p){o->core->Deactivate(p);});Bind(table,"Finalize",[o](std::shared_ptr<NativeSSAOProgramState>p){o->core->Finalize(p);});Bind(table,"Uniform",[o](std::shared_ptr<NativeSSAOProgramState>p,const std::string&name,sol::variadic_args a){auto v=Values(a);o->core->Uniform(p,name,v);return true;});
 Bind(table,"UniformMatrix",[o](std::shared_ptr<NativeSSAOProgramState>p,const std::string&name,const std::string&matrix){Check(matrix=="projectioninverse","Unknown original SSAO named matrix");o->core->Uniform(p,name,std::span<const float>(CCamera::GetActive()->GetProjectionMatrixInverse().m,16));return true;});
 Bind(table,"ClipDistance",[](unsigned index,bool enabled){Check(index<3&&!enabled,"SSAO source clip-distance exit operation differs");recorder.DisableClipDistance(index);auto&r=ui::GetRecorder();auto stage=r.CurrentWorldStage();Check(bool(stage),"SSAO clip-distance state lacks actual source stage");r.UpdateWorldState([&](NativeWorldDrawState&s){DisableNativeInheritedWaterClipPlane(s,*stage,index);});});
 return table;
}
}
void BeginWorldFrame(){recorder.Begin(globalRendering->drawFrame,OriginalCUSUniformCapture().Current());}
std::shared_ptr<const NativeSSAOFrame>EndWorldFrame(){return recorder.End(ui::GetRecorder().CurrentWorldState());}
void AbortWorldFrame()noexcept{recorder.Abort();for(auto&w:owners)if(auto o=w.lock())o->core->Abort();}
void Reset(){AbortWorldFrame();}
bool PushEntries(lua_State*L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();family.new_usertype<NativeSSAOFBO>("PrivateFBO",sol::no_constructor);family.new_usertype<NativeSSAOProgramState>("PrivateProgram",sol::no_constructor);family.new_usertype<NativeSSAOTileGeometry>("PrivateTiles",sol::no_constructor);family.set("PrivateTiles",sol::lua_nil);family.set("PrivateFBO",sol::lua_nil);family.set("PrivateProgram",sol::lua_nil);Bind(family,"Available",&IsNativeSSAOConsumerAvailable);Bind(family,"CreateContext",[L](){return CreateContext(L);});Bind(family,"LoadArchiveSource",[L](const std::string&path){Check(!CLuaHandle::GetHandleSynced(L),"SSAO archive source is unsynced only");const std::set<std::string>allowed{"luaui/widgets/gfx_ssao.lua","modules/graphics/luashader.lua","modules/graphics/instancevbotable.lua"};Check(allowed.contains(path),"Unknown original SSAO Lua source");CFileHandler file(path,SPRING_VFS_ZIP);std::string text;Check(file.FileExists()&&file.LoadStringData(text),"Original SSAO archive source missing");return text;});Bind(family,"PrepareSource",[](std::string text){auto h=Hash(text);Check(h==ssaoProgram::WidgetHash||h==ssaoProgram::ShaderHelperHash||h==ssaoProgram::GeometryHelperHash,"Original SSAO Lua source identity differs");return text;});gl.set("NativeSSAO",family);return true;}
}
#else
namespace LuaNativeSSAO {void BeginWorldFrame(){}std::shared_ptr<const rendering::NativeSSAOFrame>EndWorldFrame(){return{};}void AbortWorldFrame()noexcept{}void Reset(){}bool PushEntries(lua_State*){return true;}}
#endif
