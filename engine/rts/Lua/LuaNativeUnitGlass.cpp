/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeUnitGlass.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "NativeUnitGlassOriginal.h"
#include "LuaInclude.h"
#include "LuaHandle.h"
#include "LuaOpenGLUtils.h"
#include "LuaNativeCUSTextures.h"
#include "lib/sol2/sol.hpp"
#include "Rendering/Backend/NativeUnitGlassData.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeSourceProgramBinding.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/WorldFrameCapture.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/LocalModelPiece.hpp"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "System/Matrix44f.h"
#include <algorithm>
#include <map>
#include <unordered_map>
namespace LuaNativeUnitGlass { namespace {
using namespace rendering;
void Check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<typename>struct Boundary;
template<typename R,typename... A>struct Boundary<R(A...)>{template<typename F>static auto Wrap(F f){return[f=std::move(f)](sol::this_state L,A... a)->R{try{return f(std::forward<A>(a)...);}catch(const std::exception& e){lua_pushstring(L,e.what());}lua_error(L);std::terminate();};}};
template<typename F>void Bind(sol::table& t,const char* n,F f){t.set_function(n,Boundary<sol::meta::function_signature_t<F>>::Wrap(std::move(f)));}
struct Context;
struct Program {std::weak_ptr<Context> context;std::shared_ptr<const void> compiled;NativeUnitGlassInput state;bool deleted=false;};
struct Context:std::enable_shared_from_this<Context>{
 lua_State* lua=nullptr;uint64_t generation=0;WorldFrameCapture models;bool closed=false;
 std::vector<std::shared_ptr<Program>> programs;
 void Retire(){if(closed)return;closed=true;auto active=OriginalNativeSourceProgramBinding().Current();for(auto& p:programs){if(active.owner==p)OriginalNativeSourceProgramBinding().Unbind();p->deleted=true;p->compiled.reset();p->state.compiled.reset();}programs.clear();models.Reset();}
 std::shared_ptr<Program> Get(unsigned h){Check(!closed,"Glass source context is closed");Check(h&&h<=programs.size()&&!programs[h-1]->deleted,"Unknown/deleted glass program");return programs[h-1];}
 NativeTextureContext& Actual(){Check(!closed,"Glass source context is closed");auto& t=OriginalNativeTextureContext();Check(t.Initialized()&&t.UIAttached()&&t.Registry().ContextGeneration()==generation,"Glass texture context lifetime changed");return t;}
 std::shared_ptr<Program> Active(){Actual();auto s=OriginalNativeSourceProgramBinding().Current();Check(s.kind==NativeSourceProgramBinding::Kind::UnitGlass&&s.owner,"Glass uniform has no active source program");auto p=std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(s.owner));Check(!p->deleted&&p->context.lock().get()==this,"Glass program belongs to another/deleted source context");return p;}
};
std::unordered_map<lua_State*,std::vector<std::shared_ptr<Context>>> contexts;
void DestroyContext(const std::shared_ptr<Context>& c){c->Retire();auto i=contexts.find(c->lua);if(i==contexts.end())return;std::erase(i->second,c);if(i->second.empty())contexts.erase(i);}
void RestoreGlassSelection(NativeSourceProgramBinding::Selection saved){
 if(saved.kind==NativeSourceProgramBinding::Kind::UnitGlass){
  auto p=std::static_pointer_cast<const Program>(saved.owner);auto c=p?p->context.lock():nullptr;
  if(!p||p->deleted||!c||c->closed){OriginalNativeSourceProgramBinding().Unbind();return;}
 }
 OriginalNativeSourceProgramBinding().Restore(std::move(saved));
}
constexpr const char* names[]={"sunPos","viewInvMat","tex1","tex2","normalTex","reflectTex","teamColor","sunSpecular","pbrParams"};
bool Available(){auto& t=OriginalNativeTextureContext();return bool(UnitGlassConsumerOwner())&&t.Initialized()&&t.UIAttached();}
sol::table Create(lua_State* L){
 Check(Available()&&!CLuaHandle::GetHandleSynced(L),"Glass requires actual compiled native consumer and unsynced source");
 auto c=std::make_shared<Context>();c->lua=L;c->generation=OriginalNativeTextureContext().Registry().ContextGeneration();contexts[L].push_back(c);
 sol::state_view lua(L);auto t=lua.create_table();
 Bind(t,"DestroyContext",[c]{DestroyContext(c);});
 Bind(t,"CreateShader",[c](sol::table p){
  c->Actual();
  for(const auto& e:p){const auto k=e.first.as<std::string>();Check(k=="vertex"||k=="fragment"||k=="uniformInt"||k=="uniformFloat","Glass source shader option differs");}
  Check(p.get<std::string>("vertex")==glassoriginal::vertex&&p.get<std::string>("fragment")==glassoriginal::fragment,"Glass assembled shader identity differs");
  auto ints=p.get<sol::table>("uniformInt");Check(ints.get<int>("tex1")==0&&ints.get<int>("tex2")==1&&ints.get<int>("normalTex")==2&&ints.get<int>("reflectTex")==3,"Glass sampler layout differs");
  size_t count=0;for(auto e:ints){(void)e;++count;}Check(count==4,"Glass has extra initial sampler uniforms");
  auto floats=p.get<sol::table>("uniformFloat");for(auto e:floats){(void)e;Check(false,"Glass initial float defaults differ");}
  Check(Available(),"Glass native consumer retired");auto out=std::make_shared<Program>();out->context=c;out->compiled=UnitGlassConsumerOwner();out->state.compiled=out->compiled;c->programs.push_back(out);return unsigned(c->programs.size());
 });
 Bind(t,"GetShaderLog",[]{return std::string{};});
 Bind(t,"GetActiveUniforms",[L,c](unsigned h){c->Get(h);sol::state_view lua(L);auto out=lua.create_table();for(unsigned i=0;i<9;++i){auto x=lua.create_table();x["name"]=names[i];x["size"]=i==8?8:1;x["type"]=(i>=2&&i<=5)?(i==5?0x8B60:0x8B5E):(i==1?0x8B5C:i==6?0x8B52:i==8?0x1406:0x8B51);out[i+1]=x;}return out;});
 Bind(t,"GetUniformLocation",[L,c](unsigned h,std::string name){c->Get(h);if(name=="pbrParams[0]")name="pbrParams";for(unsigned i=0;i<9;++i)if(name==names[i])return sol::make_object(L,i+1);return sol::make_object(L,sol::lua_nil);});
 Bind(t,"UseShader",[c](unsigned h){c->Actual();if(h)OriginalNativeSourceProgramBinding().BindUnitGlass(c->Get(h));else OriginalNativeSourceProgramBinding().Unbind();});
 Bind(t,"ActiveShader",[c](unsigned h,sol::protected_function f){c->Actual();auto saved=OriginalNativeSourceProgramBinding().Current();OriginalNativeSourceProgramBinding().BindUnitGlass(c->Get(h));try{auto r=f();if(!r.valid()){sol::error e=r;throw std::runtime_error(e.what());}}catch(...){RestoreGlassSelection(std::move(saved));throw;}RestoreGlassSelection(std::move(saved));});
 Bind(t,"DeleteShader",[c](unsigned h){auto p=c->Get(h);auto s=OriginalNativeSourceProgramBinding().Current();if(s.owner==p)OriginalNativeSourceProgramBinding().Unbind();p->deleted=true;p->compiled.reset();p->state.compiled.reset();});
 Bind(t,"Uniform",[c](unsigned location,sol::variadic_args values){auto p=c->Active();Check((location==1||location==8||location==7)&&values.size()==(location==7?4u:3u),"Glass float uniform type/arity differs");for(unsigned i=0;i<values.size();++i){const float v=values[i].as<float>();if(location==1)p->state.sun[i]=v;else if(location==8)p->state.specular[i]=v;else p->state.team[i]=v;}});
 Bind(t,"UniformArray",[c](unsigned location,unsigned type,sol::table a){auto p=c->Active();Check(location==9&&type==2&&a.size()==8,"Glass pbr uniform array differs");for(unsigned i=0;i<8;++i)p->state.pbr[i]=a.get<float>(i+1);});
 Bind(t,"UniformMatrix",[c](unsigned location,std::string name){auto p=c->Active();Check(location==2&&name=="viewinverse","Glass actual matrix uniform differs");auto* m=LuaOpenGLUtils::GetNamedMatrix(name.c_str());Check(m,"Glass viewinverse unavailable");p->state.viewInverse=std::to_array(m->m);});
 Bind(t,"Texture",[c](unsigned unit,sol::object value){auto& a=c->Actual();Check(unit<4,"Glass source texture unit differs");auto active=a.History().ActiveUnit();Check(bool(active),"Glass active texture unit unobserved");const auto affected=unit?unit:*active;const bool ok=value.is<bool>()?a.History().LuaTexture(unit,value.as<bool>()):a.Registry().LuaTexture(unit,[&]{return LuaNativeCUSTextures::ParseTexture(c->lua,value.as<std::string>());});ui::GetRecorder().SyncObservedTextureBinding(affected);return ok;});
 Bind(t,"UnitShapeTextures",[c](int id,bool push){
  auto* def=unitDefHandler->GetUnitDefByID(id);if(!def||!def->LoadModel())return;const auto* model=def->model;
  Check(model->type==MODELTYPE_S3O||model->type==MODELTYPE_ASS,"Glass native selected model texture type needs its real owner");if(!push)return; // original S3O/ASS PopRenderState is a no-op
  const auto* pair=textureHandlerS3O.GetTexture(model->textureType);Check(pair,"Glass original model texture bin missing");auto& a=c->Actual();
  a.History().ActiveTexture(1);a.Registry().Bind(1,pair->tex2,NativeCUSTextureTarget::Image2D);ui::GetRecorder().SyncObservedTextureBinding(1);
  a.History().ActiveTexture(0);a.Registry().Bind(0,pair->tex1,NativeCUSTextureTarget::Image2D);ui::GetRecorder().SyncObservedTextureBinding(0);
 });
 return t;
}
}
bool PushEntries(lua_State* L){sol::state_view lua(L);auto gl=sol::stack::get<sol::table>(L,-1);auto family=lua.create_table();Bind(family,"Available",Available);Bind(family,"CreateContext",[L]{return Create(L);});gl["NativeUnitGlass"]=family;return true;}
void ReleaseLuaState(lua_State* L){auto i=contexts.find(L);if(i==contexts.end())return;for(auto& c:i->second)c->Retire();contexts.erase(i);}
bool DrawPiece(const CUnit& unit,const LocalModelPiece& piece){
 auto selection=OriginalNativeSourceProgramBinding().Current();if(selection.kind!=NativeSourceProgramBinding::Kind::UnitGlass)return false;
 auto p=std::const_pointer_cast<Program>(std::static_pointer_cast<const Program>(selection.owner));Check(p&&!p->deleted,"Glass source program retired");auto c=p->context.lock();Check(bool(c),"Glass source context retired");c->Actual();
 auto& recorder=ui::GetRecorder();Check(recorder.IsWorldStage()&&recorder.CurrentWorldStage()==NativeWorldDrawStage::World&&!recorder.IsCompiling(),"Glass selected-piece source must be the immediate original DrawWorld call");
 Check(unit.model&&piece.original,"Glass source model piece missing");const auto& pieces=unit.model->pieceObjects;auto found=std::find(pieces.begin(),pieces.end(),piece.original);Check(found!=pieces.end(),"Glass piece does not belong to actual parsed unit model");
 auto out=std::make_shared<NativeUnitGlassInput>(p->state);out->model=c->models.CaptureModel(unit.model);out->piece=uint32_t(found-pieces.begin());out->modelView=recorder.GetMatrix(ui::NativeUIMatrixMode::ModelView);out->projection=recorder.GetMatrix(ui::NativeUIMatrixMode::Projection);out->zeroToOne=recorder.WorldProjectionZeroToOne();
 for(unsigned n=0;n<4;++n)out->images[n]=c->Actual().History().Binding(n,n==3?NativeCUSTextureTarget::Cube:NativeCUSTextureTarget::Image2D);
 ValidateNativeUnitGlass(*out);GetWorldDrawRecorder().RecordUnitGlass(std::move(out),recorder.CurrentState().draw,recorder.CurrentWorldState());return true;
}
}
#endif
