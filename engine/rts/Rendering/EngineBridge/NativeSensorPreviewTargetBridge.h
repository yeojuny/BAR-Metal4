/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Lua/LuaNativeSensorPreview.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <algorithm>
#include <map>
#include <stdexcept>
#include <utility>

namespace rendering {
// Main-thread Session owner. Shared recorder/UI/renderer services outlive it;
// exported callbacks borrow this owner weakly and fail after its destruction.
// No GPU allocation, UI texture alias, recorder or submission is owned here.
class NativeSensorPreviewTargetBridge {
 struct Allocation {
  std::weak_ptr<NativePostprocessBinding> binding;
  std::shared_ptr<const NativePostprocessImage> image;
 };
 struct State {
  ui::NativeUIRecorder& recorder;
  NativePostprocessRecorder& postprocess;
  NativeSensorPreviewCompileFunction compiler;
  std::uint32_t maximumDimension;
  bool live=true;
  std::map<std::uint64_t,Allocation> allocations;
  std::vector<std::shared_ptr<NativePostprocessBinding>> targets;
  State(ui::NativeUIRecorder& u,NativePostprocessRecorder& p,
   NativeSensorPreviewCompileFunction c,std::uint32_t maximum):
   recorder(u),postprocess(p),compiler(std::move(c)),maximumDimension(maximum){}
 };
 std::shared_ptr<State> state;
 static void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
 static std::shared_ptr<State> Current(const std::weak_ptr<State>& weak){
  auto result=weak.lock();Check(result&&result->live,"Sensor Preview target service outlived its actual Session owner");return result;
 }
 static void Owned(State& s,const std::shared_ptr<NativePostprocessBinding>& binding){
  Check(binding&&binding->image&&!binding->deleted,"Sensor Preview target binding is missing/deleted");
  const auto at=s.allocations.find(binding->image->object);
  Check(at!=s.allocations.end()&&at->second.binding.lock()==binding&&at->second.image==binding->image,
   "Sensor Preview target belongs to another actual allocation owner");
  ValidateNativePostprocessImage(*binding->image);
 }
 static void World(State& s){
  Check(s.postprocess.IsActive()&&s.recorder.IsWorldStage()&&
   s.recorder.CurrentWorldStage()==NativeWorldDrawStage::World&&!s.recorder.IsCompiling(),
   "Sensor Preview target producer is outside actual DrawWorld/shared image recording");
 }
public:
 NativeSensorPreviewTargetBridge(ui::NativeUIRecorder& recorder,NativePostprocessRecorder& postprocess,
  NativeSensorPreviewCompileFunction actualCompiler,std::uint32_t actualMaximumDimension){
  Check(bool(actualCompiler)&&actualMaximumDimension,"Sensor Preview target requires actual compiler/device extent services");
  state=std::make_shared<State>(recorder,postprocess,std::move(actualCompiler),actualMaximumDimension);
 }
 ~NativeSensorPreviewTargetBridge(){state->live=false;}
 NativeSensorPreviewTargetBridge(const NativeSensorPreviewTargetBridge&)=delete;
 NativeSensorPreviewTargetBridge& operator=(const NativeSensorPreviewTargetBridge&)=delete;
 NativeSensorPreviewTargetBridge(NativeSensorPreviewTargetBridge&&)=delete;
 NativeSensorPreviewTargetBridge& operator=(NativeSensorPreviewTargetBridge&&)=delete;
 LuaNativeSensorPreview::Services MakeServices()const{
  const std::weak_ptr<State> weak=state;
  LuaNativeSensorPreview::Services services;
  services.compile=[weak](const NativeSensorPreviewProgramSource& source){
   auto s=Current(weak);return s->compiler(source); // Actual renderer-created program, never a token substitute.
  };
  services.createImage=[weak](std::uint32_t width,std::uint32_t height,
   LuaNativeSensorPreview::ImageFormat format,const NativePostprocessSampler& sampler){
   auto s=Current(weak);NativePostprocessFormat actual;
   switch(format){
    case LuaNativeSensorPreview::ImageFormat::R16Float:actual=NativePostprocessFormat::R16Float;break;
    case LuaNativeSensorPreview::ImageFormat::R32Float:actual=NativePostprocessFormat::R32Float;break;
    case LuaNativeSensorPreview::ImageFormat::R8Unorm:actual=NativePostprocessFormat::R8Unorm;break;
    default:throw std::runtime_error("Sensor Preview original scalar target format differs");
   }
   // Original allocation has undefined contents: current remains null. The
   // existing PostprocessGPU fulfills storage for this SAME image identity.
   auto binding=CreateNativePostprocessBinding(width,height,actual,sampler,true,s->maximumDimension);
   std::erase_if(s->allocations,[](const auto& item){return item.second.binding.expired();});
   Check(s->allocations.emplace(binding->image->object,Allocation{binding,binding->image}).second,
    "Sensor Preview canonical allocation identity was reused");
   return binding;
  };
  services.captureTarget=[weak](const std::shared_ptr<NativePostprocessBinding>& binding,
   const std::function<void()>& callback){
   auto s=Current(weak);Owned(*s,binding);World(*s);
   Check(callback&&!s->recorder.HasActiveTarget()&&s->targets.size()<64,
    "Sensor Preview typed target overlaps an unowned UI target or lacks its original callback");
   Check(std::find(s->targets.begin(),s->targets.end(),binding)==s->targets.end(),
    "Sensor Preview cannot recursively attach an active parent/self target");
   const auto depth=s->targets.size();s->targets.push_back(binding);
   try{
    s->recorder.RunTypedTargetState(binding->image->width,binding->image->height,[&]{
     callback();Check(s->live,"Sensor Preview Session ended during its actual target callback");Owned(*s,binding);World(*s);
    });
   }catch(...){s->targets.resize(depth);throw;}
   s->targets.resize(depth);
  };
  services.publishTarget=[weak](const std::shared_ptr<NativePostprocessBinding>& binding,
   std::uint64_t actualSequence,std::span<const std::shared_ptr<const NativePostprocessVersion>> inputs,
   std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitActualProducer){
   auto s=Current(weak);Owned(*s,binding);World(*s);
   Check(!s->targets.empty()&&s->targets.back()==binding&&s->recorder.HasActiveTypedTarget()&&actualSequence&&commitActualProducer,
    "Sensor Preview publication lacks its actual current typed target/DrawWorld ordinal/append");
   for(const auto& input:inputs){
    Check(input&&input->image&&input->generation,"Sensor Preview content read has no actual produced version");
    for(const auto& active:s->targets)Check(input->image!=active->image,
     "Sensor Preview content read feeds back from an active parent/self target");
   }
   // Dimension-only inputs are deliberately absent from this contents span.
   // The existing strong transaction appends the family command before
   // publishing current; rollback remains the actual common frame's owner.
   return s->postprocess.PublishExternalOutput(binding,NativePostprocessStage::World,actualSequence,
    NativeExternalImageFamily::SensorPreview,inputs,std::move(commitActualProducer));
  };
  return services;
 }
};
} // namespace rendering
