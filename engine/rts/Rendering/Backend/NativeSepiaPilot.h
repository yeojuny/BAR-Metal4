/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePostprocessData.h"
#include "Rendering/Metal4/NativeSepiaGeneratedProgram.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rendering::sepiaPilot {
// Actual successfully compiled library ownership is published by the renderer.
// Relinking creates a new interface generation; renderer reset replaces admission.
inline std::shared_ptr<const void> AdmissionOwner() {return NativeSepiaCompiledOwner();}
struct Program {
 std::shared_ptr<const sharedpilot::ProgramInterface> interface;
 std::shared_ptr<const std::vector<std::uint8_t>> uniforms;
 bool shared=true,deleted=false;
};
inline void Require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
inline void ValidateInterface(const sharedpilot::ProgramInterface& program) {
 // Generated values define the ABI; a matching byte count alone is not an
 // interface proof. This immutable schema owns no native library or GPU data.
 static const auto expected=sepiaGenerated::Link();
 Require(program.generation && program.sourceIdentity==expected->sourceIdentity &&
  program.uniformBytes==expected->uniformBytes && program.fields==expected->fields &&
  program.blocks==expected->blocks && program.resources==expected->resources,
  "Sepia interface differs from generated source/layout/slots");
}
inline void Live(const Program& program){
 Require(IsNativeSepiaConsumerAvailable() && !program.deleted && program.interface && program.uniforms && program.interface->nativeOwner==AdmissionOwner(),"Sepia linked program is deleted or incomplete");
 ValidateInterface(*program.interface);
 Require(program.uniforms->size()==program.interface->uniformBytes,"Sepia uniform owner differs from linked layout");
}
inline std::shared_ptr<Program> Link(bool shared) {
 Require(IsNativeSepiaConsumerAvailable() && bool(AdmissionOwner()),"Sepia has no actual compiled consumer owner");
 auto result=std::make_shared<Program>();auto interface=sepiaGenerated::Link();
 // Only private link construction mutates this newly created, unpublished owner.
 auto linked=std::make_shared<sharedpilot::ProgramInterface>(*interface);linked->nativeOwner=AdmissionOwner();
 result->interface=std::move(linked);result->uniforms=std::make_shared<const std::vector<std::uint8_t>>(sepiaGenerated::Defaults());result->shared=shared;
 return result;
}
inline void SetUniform(Program& program,const std::string& name,const std::vector<double>& values,bool integer) {
 Live(program);const auto& fields=program.interface->fields;
 const auto found=std::find_if(fields.begin(),fields.end(),[&](const auto& field){return field.name==name;});
 Require(found!=fields.end(),"Uniform is absent from linked generated interface");const auto& field=*found;
 Require(field.count==1 && values.size()==field.components && integer==(field.scalar!=sharedpilot::Scalar::Float),"Uniform scalar kind/count differs from linked interface");
 Require(field.offset<=program.uniforms->size() && field.components<=(program.uniforms->size()-field.offset)/4,
  "Uniform write exceeds the linked immutable byte owner");
 auto next=std::make_shared<std::vector<std::uint8_t>>(*program.uniforms);
 for(size_t i=0;i<values.size();++i){const double value=values[i];Require(std::isfinite(value),"Uniform is not finite");
  if(integer){Require(field.scalar==sharedpilot::Scalar::Int && value>=std::numeric_limits<std::int32_t>::min() && value<=std::numeric_limits<std::int32_t>::max() && std::trunc(value)==value,"Uniform integer outside exact int32 range");const auto typed=std::int32_t(value);std::memcpy(next->data()+field.offset+4*i,&typed,4);}
  else{Require(std::abs(value)<=std::numeric_limits<float>::max(),"Uniform outside float range");const auto typed=float(value);std::memcpy(next->data()+field.offset+4*i,&typed,4);}
 }
 program.uniforms=std::move(next);
}
inline void ValidateMaterial(const sharedpilot::MaterialContent& material) {
 Require(IsNativeSepiaConsumerAvailable() && material.program && material.program->generation && material.program->nativeOwner && material.program->nativeOwner==AdmissionOwner(),"Sepia material has no matching compiled linked owner");
 ValidateInterface(*material.program);
 Require(material.uniformBytes && material.uniformBytes->size()==material.program->uniformBytes && material.program->uniformBytes==sepiaGenerated::UniformBytes,"Sepia immutable uniform block differs from generated interface");
 Require(material.bindings.size()==1 && material.bindings[0].logicalSlot==0 && material.bindings[0].versionOwner,"Sepia immutable actual input missing");
}
inline std::array<std::int32_t,2> ViewPos(const sharedpilot::MaterialContent& material) {
 ValidateMaterial(material);std::array<std::int32_t,2> result{};std::memcpy(result.data(),material.uniformBytes->data()+sepiaGenerated::ViewPosOffset,8);return result;
}
}
