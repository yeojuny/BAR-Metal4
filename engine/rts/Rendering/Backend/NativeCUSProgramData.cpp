/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSProgramData.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace rendering {
namespace {void Check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
uint32_t NativeCUSSampleCode(const NativeTextureSampling& s)
{
 uint32_t code=0;
 for(unsigned c=0;c<4;++c){unsigned value;
  switch(s.swizzle[c]){case 0x1903:value=0;break;case 0x1904:value=1;break;case 0x1905:value=2;break;case 0x1906:value=3;break;case 0:value=4;break;case 1:value=5;break;default:throw std::runtime_error("CUS original texture swizzle is unknown");}
  code|=value<<(c*4);
 }
 switch(s.depthMode){case 0x1909:break;case 0x8049:code|=1u<<16;break;case 0x1906:code|=2u<<16;break;default:throw std::runtime_error("CUS original depth sampling mode is unknown");}
 return code;
}
std::vector<uint8_t> PackNativeCUSProgramUniforms(const NativeCUSGeneratedStage& stage,
 const NativeCUSSourceDraw& draw,const NativeCUSProgramSource& source,const NativeCUSSampleTransport& transport)
{
 std::vector<uint8_t> bytes((stage.uniformBytes+15u)&~15u);
 for(const auto& f:stage.fields){
  if(!f.active)continue;
  const std::string_view name=f.name;
  std::vector<float> supplied;
  if(name=="NativeCUSImageMasks")supplied={float(transport.emptyMask),float(transport.flipMask),float(transport.depthMask),float(transport.clampDepthCompareMask)};
  else if(name=="NativeCUSSampleCodes")for(auto code:transport.sampleCodes)supplied.push_back(float(code));
  else if(name=="NativeCUSClipFlags")supplied={float(transport.clipFlags)};
  else if(name.starts_with("NativeCUSWater")){
   Check(name.size()==15 && name.back()>='0' && name.back()<='2',"CUS original map uniform identity differs");
   const auto& c=source.waterColors[name.back()-'0'];supplied.assign(c.begin(),c.end());
  }else{
   const auto& values=f.scalar==NativeCUSUniformScalar::Float?draw.floatUniforms:draw.intUniforms;
   const auto it=values.find(std::string(name));Check(it!=values.end(),"CUS active uniform lacks its original link/default/write value");supplied=it->second;
  }
  Check(supplied.size()==f.components*f.count,"CUS original active uniform component count differs");
  for(unsigned element=0;element<f.count;++element)for(unsigned component=0;component<f.components;++component){
   const size_t offset=f.offset+size_t(element)*f.stride+component*4;
   Check(offset+4<=bytes.size(),"CUS reflected uniform offset exceeds native block");
   const float value=supplied[element*f.components+component];Check(std::isfinite(value),"CUS reflected uniform contains non-finite bytes");
   if(f.scalar==NativeCUSUniformScalar::Float)std::memcpy(bytes.data()+offset,&value,4);
   else if(f.scalar==NativeCUSUniformScalar::Int){Check(value>=-2147483648.0 && double(value)<=2147483647.0 && std::trunc(value)==value,"CUS signed original uniform is outside its ABI");const auto i=int32_t(value);std::memcpy(bytes.data()+offset,&i,4);}
   else{Check(value>=0 && double(value)<=4294967295.0 && std::trunc(value)==value,"CUS unsigned transport value is outside its ABI");const auto i=uint32_t(value);std::memcpy(bytes.data()+offset,&i,4);}
  }
 }
 return bytes;
}
void PopulateNativeCUSProgramUniformMetadata(const NativeCUSGeneratedProgram& program,NativeCUSProgramConsumer& out)
{
 for(const auto& d:program.defaults){
  out.uniforms.push_back({d.name,d.glType,1,d.integer});
  auto& values=d.integer?out.defaultInt:out.defaultFloat;
  values[d.name]=std::vector<float>(d.values.begin(),d.values.begin()+d.components);
 }
}
} // namespace rendering
