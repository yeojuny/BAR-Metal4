// Private pure-host admission/payload test. No engine runtime or Metal device.
#include "Rendering/Backend/NativeSepiaPilot.h"
#include <cassert>
#include <functional>
#include <fstream>
#include <iostream>
#include "Rendering/Metal4/Metal4SepiaShaderSource.h"
using namespace rendering;
template<class F>void rejects(F fn){bool failed=false;try{fn();}catch(const std::runtime_error&){failed=true;}assert(failed);}
int main(int argc,char** argv){
 SetNativeSepiaConsumerAvailable(true,std::make_shared<const int>(1));
 auto first=sepiaPilot::Link(true),second=sepiaPilot::Link(true);
 assert(first->interface->generation!=second->interface->generation);
 assert(first->interface->nativeOwner==second->interface->nativeOwner);
 const auto initial=first->uniforms;
 sepiaPilot::SetUniform(*first,"gamma",{1.25},false);
 assert(first->uniforms!=initial);const auto frozen=first->uniforms;
 sepiaPilot::SetUniform(*first,"gamma",{2.0},false);assert(first->uniforms!=frozen);
 const auto& fields=first->interface->fields;
 assert(first->interface->blocks.size()==1 && first->interface->blocks[0].slot==1 && first->interface->blocks[0].bytes==first->uniforms->size());
 for(const auto& field:fields)assert(field.blockSlot==1);
 auto gamma=std::find_if(fields.begin(),fields.end(),[](const auto& f){return f.name=="gamma";});
 float before=0;std::memcpy(&before,frozen->data()+gamma->offset,4);assert(before==1.25f);
 rejects([&]{sepiaPilot::SetUniform(*first,"gamma",{1,2},false);});
 rejects([&]{sepiaPilot::SetUniform(*first,"gamma",{1},true);});
 rejects([&]{sepiaPilot::SetUniform(*first,"undeclared",{1},false);});
 rejects([&]{sepiaPilot::SetUniform(*first,"gamma",{std::numeric_limits<double>::infinity()},false);});
 rejects([&]{sepiaPilot::SetUniform(*first,"viewPos",{0.5,1},true);});
 auto bindingA=std::make_shared<const int>(1),bindingB=std::make_shared<const int>(2);
 sharedpilot::MaterialContent material;material.program=first->interface;material.uniformBytes=frozen;
 sharedpilot::LogicalBinding a;a.versionOwner=bindingA;material.bindings.push_back(a);sepiaPilot::ValidateMaterial(material);
 const auto retained=material;material.bindings[0].versionOwner=bindingB;
 assert(retained.bindings[0].versionOwner!=material.bindings[0].versionOwner);sepiaPilot::ValidateMaterial(material);
 auto wrong=material;wrong.uniformBytes=std::make_shared<const std::vector<std::uint8_t>>(4);rejects([&]{sepiaPilot::ValidateMaterial(wrong);});
 // Equal owner and byte size must not admit a mismatched generated ABI.
 const auto mismatched=[&](auto mutate){
  auto iface=std::make_shared<sharedpilot::ProgramInterface>(*material.program);mutate(*iface);
  auto bad=material;bad.program=iface;rejects([&]{sepiaPilot::ValidateMaterial(bad);});
  auto badProgram=*first;badProgram.interface=iface;rejects([&]{sepiaPilot::SetUniform(badProgram,"gamma",{1},false);});
 };
 mismatched([](auto& p){p.sourceIdentity+="wrong";});
 mismatched([](auto& p){p.fields[0].offset=UINT32_MAX;});
 mismatched([](auto& p){p.fields[0].scalar=sharedpilot::Scalar::Float;});
 mismatched([](auto& p){p.fields[0].blockSlot=0;});
 mismatched([](auto& p){p.blocks[0].slot=0;});
 mismatched([](auto& p){p.resources[0].logicalSlot=1;});
 mismatched([](auto& p){p.resources[0].textureSlot=1;});
 mismatched([](auto& p){p.generation=0;});
 auto shortProgram=*first;shortProgram.uniforms=std::make_shared<const std::vector<std::uint8_t>>(4);
 rejects([&]{sepiaPilot::SetUniform(shortProgram,"gamma",{1},false);});
 first->deleted=true;rejects([&]{sepiaPilot::SetUniform(*first,"gamma",{1},false);});sepiaPilot::ValidateMaterial(retained);
 auto third=sepiaPilot::Link(true);assert(third->interface->generation!=first->interface->generation);
 SetNativeSepiaConsumerAvailable(false);rejects([&]{sepiaPilot::ValidateMaterial(retained);});
 SetNativeSepiaConsumerAvailable(true,std::make_shared<const int>(2));rejects([&]{sepiaPilot::Live(*third);});
 assert(sepiaPilot::Link(true)->interface->nativeOwner!=third->interface->nativeOwner);
 if(argc==2){std::ofstream out(argv[1]);out<<metal4::NATIVE_SEPIA_SHADER<<sepiaGenerated::MSL;}
 std::cout<<"payload ownership, relink, deletion, typed updates and exact binding-version tests passed\n";
}
