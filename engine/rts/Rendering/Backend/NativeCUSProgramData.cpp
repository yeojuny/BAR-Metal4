/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSProgramData.h"
#include <algorithm>
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
 std::vector<uint8_t> bytes(NativeCUSProgramUniformBytes(stage));
 PackNativeCUSProgramUniformsInto(stage,draw,source,transport,bytes);
 return bytes;
}
namespace {
void PackFields(std::span<const NativeCUSGeneratedField> fields,
 const NativeCUSSourceDraw& draw,const NativeCUSProgramSource& source,
 const NativeCUSSampleTransport& transport,std::span<uint8_t> bytes)
{
 for(const auto& f:fields){
  if(!f.active)continue;
  const std::string_view name=f.name;
  std::array<float,12> converted;
  std::span<const float> supplied;
  if(name=="NativeCUSImageMasks"){converted={float(transport.emptyMask),float(transport.flipMask),float(transport.depthMask),float(transport.clampDepthCompareMask)};supplied={converted.data(),4};}
  else if(name=="NativeCUSSampleCodes"){for(size_t i=0;i<converted.size();++i)converted[i]=float(transport.sampleCodes[i]);supplied=converted;}
  else if(name=="NativeCUSClipFlags"){converted[0]=float(transport.clipFlags);supplied={converted.data(),1};}
  else if(name.starts_with("NativeCUSWater")){
   Check(name.size()==15 && name.back()>='0' && name.back()<='2',"CUS original map uniform identity differs");
   supplied=source.waterColors[name.back()-'0'];
  }else{
   const auto& values=f.scalar==NativeCUSUniformScalar::Float?draw.FloatUniforms():draw.IntUniforms();
   const auto it=values.find(std::string(name));Check(it!=values.end(),"CUS active uniform lacks its original link/default/write value");supplied=it->second;
  }
  Check(supplied.size()==size_t(f.components)*f.count,"CUS original active uniform component count differs");
  for(unsigned element=0;element<f.count;++element)for(unsigned component=0;component<f.components;++component){
   const size_t offset=f.offset+size_t(element)*f.stride+component*4;
   Check(offset+4<=bytes.size(),"CUS reflected uniform offset exceeds native block");
   const float value=supplied[size_t(element)*f.components+component];Check(std::isfinite(value),"CUS reflected uniform contains non-finite bytes");
   if(f.scalar==NativeCUSUniformScalar::Float)std::memcpy(bytes.data()+offset,&value,4);
   else if(f.scalar==NativeCUSUniformScalar::Int){Check(value>=-2147483648.0 && double(value)<=2147483647.0 && std::trunc(value)==value,"CUS signed original uniform is outside its ABI");const auto i=int32_t(value);std::memcpy(bytes.data()+offset,&i,4);}
   else{Check(value>=0 && double(value)<=4294967295.0 && std::trunc(value)==value,"CUS unsigned transport value is outside its ABI");const auto i=uint32_t(value);std::memcpy(bytes.data()+offset,&i,4);}
  }
 }
}
} // namespace
void PackNativeCUSProgramUniformsInto(const NativeCUSGeneratedStage& stage,
 const NativeCUSSourceDraw& draw,const NativeCUSProgramSource& source,const NativeCUSSampleTransport& transport,std::span<uint8_t> bytes)
{
 Check(bytes.size()==NativeCUSProgramUniformBytes(stage),"CUS uniform destination differs from its original padded block");
 std::fill(bytes.begin(),bytes.end(),uint8_t(0));
 PackFields(stage.fields,draw,source,transport,bytes);
}
NativeCUSProgramUniformPlan::NativeCUSProgramUniformPlan(const NativeCUSGeneratedStage& stage)
 :byteSize(NativeCUSProgramUniformBytes(stage))
{
 names.reserve(stage.fields.size());
 for(const auto& f:stage.fields)names.emplace_back(f.name);
 // Preserve original field order, names and ABI independently of caller storage.
 // Unexpected overlapping reflection falls back to the original ordered pack.
 std::vector<bool> written(byteSize,false);
 for(size_t i=0;i<stage.fields.size();++i){
  auto f=stage.fields[i];f.name=names[i].c_str();allFields.push_back(f);
  if(!f.active)continue;
  for(unsigned element=0;element<f.count;++element)for(unsigned component=0;component<f.components;++component){
   const size_t offset=f.offset+size_t(element)*f.stride+size_t(component)*4;
   if(offset>byteSize||byteSize-offset<4){split=false;continue;}
   for(size_t j=offset;j<offset+4;++j){if(written[j])split=false;written[j]=true;}
  }
  const std::string_view name=f.name;
  const bool dynamic=name=="NativeCUSImageMasks"||name=="NativeCUSSampleCodes"||
   name=="NativeCUSClipFlags"||name.starts_with("NativeCUSWater");
  (dynamic?dynamicFields:staticFields).push_back(f);
 }
}
bool NativeCUSProgramUniformPlan::Pack(const NativeCUSSourceDraw& draw,
 const NativeCUSProgramSource& source,const NativeCUSSampleTransport& transport,std::span<uint8_t> bytes)
{
 Check(bytes.size()==byteSize,"CUS uniform destination differs from its original padded block");
 if(!split){
  std::fill(bytes.begin(),bytes.end(),uint8_t(0));
  PackFields(allFields,draw,source,transport,bytes);return true;
 }
 const auto& current=draw.uniformValues;
 const bool hit=current&&ownerAddress==current.get()&&!owner.owner_before(current)&&!current.owner_before(owner);
 if(!hit){
  std::vector<uint8_t> candidate(byteSize,0);
  PackFields(staticFields,draw,source,transport,candidate);
  // Failed packing must not publish a new key or corrupt the previous template.
  packed=std::move(candidate);owner=current;ownerAddress=current.get();
 }
 std::copy(packed.begin(),packed.end(),bytes.begin());
 PackFields(dynamicFields,draw,source,transport,bytes);
 return !hit;
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
