/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// OpenAI Codex: scoped to the pinned original Terraform Brush Capture.
#include "NativeDeferredVisualizerLuaContext.h"
#include "Rendering/Backend/NativeTerraformCaptureProgram.h"
#include "Rendering/Backend/NativeTargetPriorityProgram.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Models/3DModel.hpp"
#include "Rendering/Models/ModelsLock.h"
#include "Rendering/Textures/3DOTextureHandler.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Sim/Units/UnitDef.h"
#include "Sim/Units/UnitDefHandler.h"
#include "Sim/Features/FeatureDef.h"
#include "Sim/Features/FeatureDefHandler.h"
#include <cstring>
#include <algorithm>
namespace LuaNativeTerraformCapture {
using namespace rendering;
using LuaNativeDeferredVisualizer::Require;
using LuaNativeDeferredVisualizer::Bind;
struct Owner;
struct Program {
 std::weak_ptr<Owner> owner;
 NativeTerraformCaptureProgram kind=NativeTerraformCaptureProgram::Reproject;
 NativeTerraformCaptureUniforms uniforms;
 std::shared_ptr<const void> compiled;
 bool deleted=false;
};
struct Owner {std::shared_ptr<Program> active;};
inline int Location(const Program& p,std::string_view name){
 for(unsigned i=0;i<NativeTerraformCaptureFields.size();++i){const auto& f=NativeTerraformCaptureFields[i];if(name==f.name&&(f.programs&(1u<<unsigned(p.kind))))return int(i+1);}return -1;
}
inline void SetValues(Program& p,int location,const std::vector<float>& values,bool matrix){
 if(location==-1)return;
 Require(!p.deleted&&location>=1&&unsigned(location)<=NativeTerraformCaptureFields.size(),"Terraform Capture uniform requires its original program");
 const auto& field=NativeTerraformCaptureFields[location-1];
 Require((field.programs&(1u<<unsigned(p.kind)))&&field.components==values.size()&&matrix==(field.components==16),"Terraform Capture uniform type/dimension changed");
 for(float v:values)Require(std::isfinite(v),"Terraform Capture nonfinite uniform");
 // Scalar/vector slots are padded to16, followed by the column-major matrix.
 std::memcpy(reinterpret_cast<std::byte*>(&p.uniforms)+(location-1)*16,values.data(),values.size()*sizeof(float));
}
inline std::shared_ptr<NativeTerraformCaptureInput> Input(const std::shared_ptr<Owner>& owner){
 Require(owner->active&&!owner->active->deleted,"Terraform Capture active program missing");
 auto input=std::make_shared<NativeTerraformCaptureInput>();input->program=owner->active->kind;input->uniforms=owner->active->uniforms;input->compiled=owner->active->compiled;
 auto& recorder=ui::GetRecorder();Require(recorder.TextureObserver()==&OriginalNativeTextureContext().UI(),"Terraform Capture actual image owner missing");
 for(unsigned slot=0;slot<(input->program==NativeTerraformCaptureProgram::Reproject?2u:1u);++slot){const auto bound=recorder.TextureObserver()->BoundImage(slot);input->images[slot]={bound.image,ScreenUVMode::Normalized,bound.sampler};}
 return input;
}
inline const S3DModel* DefinitionModel(int id,bool feature){
 if(feature){const auto* def=featureDefHandler?featureDefHandler->GetFeatureDefByID(id):nullptr;return def?def->LoadModel():nullptr;}
 const auto* def=unitDefHandler?unitDefHandler->GetUnitDefByID(id):nullptr;return def?def->LoadModel():nullptr;
}
inline void ShapeTextures(int id,bool enabled,bool feature){
 const auto* model=DefinitionModel(id,feature);if(!model)return;
 auto lock=CModelsLock::GetScopedLock();
 Require(model->loadStatus==S3DModel::LOADED,"Terraform Capture actual loaded model missing");
 Require(model->type==MODELTYPE_3DO||model->type==MODELTYPE_S3O||model->type==MODELTYPE_ASS,"Terraform Capture original model texture class unsupported");
 // Match original Push/PopModelRenderState: 3DO toggles culling, S3O/ASS
 // leave it alone. Pop does not unbind either image or restore earlier images.
 if(!enabled){if(model->type==MODELTYPE_3DO)ui::GetRecorder().UpdateWorldState([](auto& state){state.cullEnabled=true;},false);return;}
 std::array<std::uint64_t,2> names{};
 if(model->type==MODELTYPE_3DO){names={textureHandler3DO.GetAtlasTex1ID(),textureHandler3DO.GetAtlasTex2ID()};}
 else {const auto* material=textureHandlerS3O.GetTexture(model->textureType);Require(material!=nullptr,"Terraform Capture actual handler material missing");names={material->tex1,material->tex2};}
 auto& actual=OriginalNativeTextureContext();Require(actual.Initialized(),"Terraform Capture actual texture context missing");
 for(auto name:names){const auto object=actual.Registry().Find(name);Require(object&&object->target==NativeCUSTextureTarget::Image2D,"Terraform Capture handler image not owned by current registry");
  const auto backing=std::dynamic_pointer_cast<const NativeModelTextureBacking>(object->backing);Require(backing&&backing->pixels&&!backing->pixels->mips.empty(),"Terraform Capture handler image has no actual model pixels");}
 if(model->type==MODELTYPE_3DO)ui::GetRecorder().UpdateWorldState([](auto& state){state.cullEnabled=false;},false);
 for(unsigned unit:{1u,0u}){actual.History().ActiveTexture(unit);actual.Registry().Bind(unit,names[unit],NativeCUSTextureTarget::Image2D);ui::GetRecorder().SyncObservedTextureBinding(unit);}
}
inline void Shape(const std::shared_ptr<Owner>& owner,int id,int,bool raw,bool screen,bool opaque,bool feature){
 const auto* model=DefinitionModel(id,feature);if(!model)return;
 Require(raw&&!screen&&opaque&&owner->active&&owner->active->kind==NativeTerraformCaptureProgram::Model,"Terraform Capture original raw opaque static Shape call differs");
 Require(bool(NativeTerraformCaptureModels()),"Terraform Capture actual static model provider missing");
 auto captured=NativeTerraformCaptureModels()(model);Require(bool(captured),"Terraform Capture model provider lost actual geometry");
 ui::GetRecorder().AppendTerraformCapture(Input(owner),{},{},std::move(captured));
}
inline sol::table Create(lua_State* L){
 Require(!CLuaHandle::GetHandleSynced(L),"Terraform Capture must be unsynced");
 sol::state_view lua(L);auto context=lua.create_table();auto owner=std::make_shared<Owner>();
 context.new_usertype<Program>("TerraformCaptureProgram",sol::no_constructor);context.set("TerraformCaptureProgram",sol::lua_nil);
 Bind(context,"CreateShader",[owner](const sol::table& source){
  unsigned count=0;for(const auto& entry:source){const auto key=entry.first.as<std::string>();Require(key=="vertex"||key=="fragment"||key=="uniformInt"||key=="uniformFloat","Terraform Capture unknown shader field");++count;}Require(count==4,"Terraform Capture original program field missing");
  const auto hash=targetPriorityProgram::Hash(source["fragment"].get<std::string>());const auto found=std::find(NativeTerraformCaptureFragmentHashes.begin(),NativeTerraformCaptureFragmentHashes.end(),hash);
  Require(found!=NativeTerraformCaptureFragmentHashes.end(),"Terraform Capture original fragment source changed");const auto index=unsigned(found-NativeTerraformCaptureFragmentHashes.begin());
  Require(targetPriorityProgram::Hash(source["vertex"].get<std::string>())==NativeTerraformCaptureVertexHashes[index],"Terraform Capture original paired vertex source changed");
  auto p=std::make_shared<Program>();p->owner=owner;p->kind=NativeTerraformCaptureProgram(index);
  const std::array<std::array<std::string_view,2>,3> samplers{{{"screenTex","heightMap"},{"pic",""},{"tex0",""}}};
  const unsigned needed=index==0?2:1;count=0;
  for(const auto& entry:source["uniformInt"].get<sol::table>()){
   const auto key=entry.first.as<std::string>();const int value=entry.second.as<int>();Require(value>=0&&unsigned(value)<needed&&key==samplers[index][value],"Terraform Capture original sampler mapping changed");++count;
  }Require(count==needed,"Terraform Capture sampler missing");count=0;
  for(const auto& entry:source["uniformFloat"].get<sol::table>()){
   const int location=Location(*p,entry.first.as<std::string>());Require(location>0&&location!=10,"Terraform Capture original initial uniform differs");std::vector<float> values;
   if(entry.second.is<sol::table>()){const auto table=entry.second.as<sol::table>();for(unsigned n=1;n<=table.size();++n)values.push_back(table.get<float>(n));}
   else values.push_back(entry.second.as<float>());
   SetValues(*p,location,values,false);++count;
  }Require(count==(index==0?4u:index==1?5u:1u),"Terraform Capture initial uniform missing");
  p->compiled=NativeTerraformCaptureOwner().lock();Require(bool(p->compiled),"Terraform Capture actual compiled consumer missing");return p;
 });
 Bind(context,"GetUniformLocation",[owner](const std::shared_ptr<Program>& p,const std::string& name){Require(p&&p->owner.lock()==owner&&!p->deleted,"Terraform Capture deleted/foreign program");return Location(*p,name);});
 Bind(context,"UseShader",[owner](sol::object value){
  if(value.is<int>()){Require(value.as<int>()==0,"Terraform Capture has no numeric program names");owner->active.reset();return;}
  auto p=value.as<std::shared_ptr<Program>>();Require(p&&p->owner.lock()==owner&&!p->deleted&&p->compiled,"Terraform Capture active program missing");owner->active=std::move(p);
 });
 Bind(context,"DeleteShader",[owner](const std::shared_ptr<Program>& p){Require(p&&p->owner.lock()==owner,"Terraform Capture foreign program");if(owner->active==p)owner->active.reset();p->deleted=true;p->compiled.reset();});
 Bind(context,"Uniform",[owner](int location,sol::variadic_args args){if(location==-1)return;Require(bool(owner->active),"Terraform Capture active uniform owner missing");std::vector<float> values;for(auto value:args)values.push_back(value.as<float>());SetValues(*owner->active,location,values,false);});
 Bind(context,"UniformMatrix",[owner](int location,sol::variadic_args args){if(location==-1)return;Require(bool(owner->active),"Terraform Capture active matrix owner missing");std::vector<float> values;for(auto value:args)values.push_back(value.as<float>());SetValues(*owner->active,location,values,true);});
 Bind(context,"DrawQuad",[owner](const sol::table& rows){
  Require(rows.size()==4,"Terraform Capture original quad stream missing");auto input=Input(owner);Require(input->program!=NativeTerraformCaptureProgram::Model,"Terraform Capture model requires actual static geometry");
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;
  for(unsigned i=1;i<=rows.size();++i){const auto row=rows.get<sol::table>(i);Require(row.size()==5,"Terraform Capture original vertex/UV changed");std::array<float,5> v{};for(unsigned j=0;j<5;++j){v[j]=row.get<float>(j+1);Require(std::isfinite(v[j]),"Terraform Capture nonfinite vertex");}vertices.push_back({v[0],v[1],v[2],{1,1,1,1}});uv.push_back({v[3],v[4],0,0});}
  ui::GetRecorder().AppendTerraformCapture(std::move(input),std::move(vertices),std::move(uv));
 });
 Bind(context,"UnitShapeTextures",[](int id,bool enabled){ShapeTextures(id,enabled,false);});
 Bind(context,"FeatureShapeTextures",[](int id,bool enabled){ShapeTextures(id,enabled,true);});
 Bind(context,"UnitShape",[owner](int id,int team,bool raw,bool screen,bool opaque){Shape(owner,id,team,raw,screen,opaque,false);});
 Bind(context,"FeatureShape",[owner](int id,int team,bool raw,bool screen,bool opaque){Shape(owner,id,team,raw,screen,opaque,true);});
 return context;
}
} // namespace LuaNativeTerraformCapture
