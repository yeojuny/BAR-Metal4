/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeUnitEffectsData.h"
#include <atomic>
#include <cmath>
#include <cstring>
namespace rendering {namespace {
std::shared_ptr<const void> consumer;
void Check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void Snapshot(const NativeBufferSnapshotRef& b){Check(b&&b->object&&b->revision&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"Airjets actual immutable buffer missing");}
void Attribute(const NativeBufferLayout& l,unsigned n,unsigned count,unsigned offset,NativeScalarType type){const auto& a=l.attributes.at(n);Check(a.id==n+(l.stride==68?1:0)&&a.offset==offset&&a.declaredCount==count&&a.byteSize==count*4&&a.scalarType==type&&a.valueShape==NativeValueShape::Scalar&&!a.normalized,"Airjets original attribute ABI differs");}
}
std::shared_ptr<const void> NativeUnitEffectsConsumer(){return std::atomic_load(&consumer);}
void SetNativeUnitEffectsConsumer(std::shared_ptr<const void> v){std::atomic_store(&consumer,std::move(v));}
void ValidateNativeUnitEffect(const NativeUnitEffectInput& d){
 Check(d.compiled&&d.compiled==NativeUnitEffectsConsumer(),"Airjets compiled source lifetime differs");Check(!(d.orb&&d.paralyze),"Unit effect program family overlaps");
 if(d.paralyze){
  Snapshot(d.instances);const auto& l=d.instances->layout;Check(l.kind==NativeBufferKind::Vertex&&l.stride==16&&l.attributes.size()==1,"Paralyze original instance layout differs");const auto& a=l.attributes[0];Check(a.id==6&&a.offset==0&&a.declaredCount==4&&a.byteSize==16&&a.scalarType==NativeScalarType::UInt32&&a.valueShape==NativeValueShape::Scalar&&!a.normalized,"Paralyze original instData ABI differs");
  Check(!d.vertices&&!d.indices&&d.count==0&&d.indexOffset==0&&d.baseVertex==0&&d.baseInstance==0&&d.instanceCount&&d.instanceCount==d.submissions.size()&&d.instanceCount<=l.elements,"Paralyze original ModelsVBO/submission extent differs");
  Check(d.original&&d.transforms&&d.models&&d.transforms->kind==NativeCUSPublishedKind::Transforms&&d.transforms->stride==32&&d.models->kind==NativeCUSPublishedKind::ModelUniforms&&d.models->stride==128&&d.transforms->contextGeneration&&d.transforms->contextGeneration==d.models->contextGeneration,"Paralyze actual model publications missing");ValidateNativeCUSUniformPublication(*d.original);
  Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame&&d.transforms->publishedDrawFrame<=d.sourceDrawFrame&&d.models->publishedDrawFrame<=d.sourceDrawFrame,"Paralyze publication from future frame");
  Check(d.reflectionPass==0&&!d.images[0]&&!d.images[1],"Paralyze original world shader has no sampled images/reflection");for(const auto& model:d.submissions)Check(model.unitID>=0&&model.model!=nullptr,"Paralyze original unit model provider missing");return;
 }
 Check(!d.empRework&&d.submissions.empty(),"Non-paralyze source contains model submissions");Snapshot(d.vertices);Snapshot(d.indices);Snapshot(d.instances);
 const auto& v=d.vertices->layout;const auto& i=d.instances->layout;const auto& e=d.indices->layout;
 if(d.orb){
  Check(v.kind==NativeBufferKind::Vertex&&v.stride==36&&v.elements==825&&v.attributes.size()==3,"Orb original32x24 sphere vertex stream differs");
  auto attr=[](const NativeBufferLayout& l,unsigned n,unsigned id,unsigned count,unsigned offset,NativeScalarType type){const auto& a=l.attributes.at(n);Check(a.id==id&&a.offset==offset&&a.declaredCount==count&&a.byteSize==count*4&&a.scalarType==type&&a.valueShape==NativeValueShape::Scalar&&!a.normalized,"Orb original mixed attribute ABI differs");};
  attr(v,0,0,4,0,NativeScalarType::Float32);attr(v,1,1,3,16,NativeScalarType::Float32);attr(v,2,2,2,28,NativeScalarType::Float32);
  Check(i.kind==NativeBufferKind::Vertex&&i.stride==80&&i.attributes.size()==5,"Orb original80-byte instance stream differs");for(unsigned n=0;n<5;++n)attr(i,n,n+3,4,n*16,n==4?NativeScalarType::UInt32:NativeScalarType::Float32);
  Check(e.kind==NativeBufferKind::Index&&e.stride==2&&e.attributes.size()==1&&e.attributes[0].scalarType==NativeScalarType::UInt16&&e.elements==4416&&d.count==4416&&d.indexOffset==0&&d.baseVertex==0&&d.baseInstance==0&&d.instanceCount&&d.instanceCount<=i.elements&&!d.transforms,"Orb original indexed sphere/model ownership differs");
  for(size_t p=0;p<d.indices->bytes.size();p+=2){uint16_t index;std::memcpy(&index,d.indices->bytes.data()+p,2);Check(index<v.elements,"Orb actual index exceeds vertex stream");}
 }else{
 Check(v.kind==NativeBufferKind::Vertex&&v.stride==16&&v.elements==4&&v.attributes.size()==1,"Airjets original quad stream differs");Attribute(v,0,4,0,NativeScalarType::Float32);
 Check(i.kind==NativeBufferKind::Vertex&&i.stride==68&&i.attributes.size()==6,"Airjets original instance stream differs");constexpr unsigned counts[]{3,3,3,1,4,3},offsets[]{0,12,24,36,40,56};
 for(unsigned n=0;n<6;++n)Attribute(i,n,counts[n],offsets[n],n==3||n==4?NativeScalarType::UInt32:NativeScalarType::Float32);
 Check(e.kind==NativeBufferKind::Index&&e.stride==2&&e.attributes.size()==1&&e.attributes[0].scalarType==NativeScalarType::UInt16&&e.elements==6&&d.count==6&&d.indexOffset==0&&d.baseVertex==0&&d.baseInstance==0&&d.instanceCount&&d.instanceCount<=i.elements,"Airjets actual indexed interval differs");
 for(size_t p=0;p<d.indices->bytes.size();p+=2){uint16_t x;std::memcpy(&x,d.indices->bytes.data()+p,2);Check(x<v.elements,"Airjets source index outside vertex stream");}
 }
 Check(d.original&&d.models&&(d.orb||d.transforms),"Airjets actual UBO/SSBO publication missing");ValidateNativeCUSUniformPublication(*d.original);
 Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame&&(d.orb||d.transforms->publishedDrawFrame<=d.sourceDrawFrame)&&d.models->publishedDrawFrame<=d.sourceDrawFrame,"Airjets publication from a future frame");
 if(d.orb)Check(d.models->kind==NativeCUSPublishedKind::ModelUniforms&&d.models->stride==128&&d.models->contextGeneration,"Orb actual model row ABI/context differs");else {
 Check(d.transforms->kind==NativeCUSPublishedKind::Transforms&&d.transforms->stride==32&&d.models->kind==NativeCUSPublishedKind::ModelUniforms&&d.models->stride==128&&d.transforms->contextGeneration&&d.transforms->contextGeneration==d.models->contextGeneration,"Airjets original model publication ABI/context differs");
 }
 Check(d.reflectionPass==0||d.reflectionPass==1,"Airjets reflection uniform differs");
 if(d.orb){Check(!d.images[0]&&!d.images[1],"Orb unused sampler declarations must not acquire invented textures");return;}
 for(unsigned n=0;n<2;++n){const auto& image=d.images[n];Check(image&&image->format==ScreenTextureFormat::RGBA8Unorm&&!image->incomplete&&!image->renderTarget&&!image->postprocessVersion&&image->sourceName==(n?"luaui/images/jet_atlas.tga":"bitmaps/GPL/perlin_noise.jpg"),"Airjets real named texture differs");ValidateScreenSampler(d.samplers[n]);}
}
std::vector<NativeAirjetPublished> ReadNativeAirjetPublished(const NativeUnitEffectInput& d){
 ValidateNativeUnitEffect(d);Check(!d.orb&&!d.paralyze,"Only Airjets may acquire Airjets piece transforms");std::vector<NativeAirjetPublished> out(d.instanceCount);
 for(uint32_t n=0;n<d.instanceCount;++n){auto& p=out[n];const auto* row=d.instances->bytes.data()+size_t(n)*68;uint32_t piece,base,model;std::memcpy(&piece,row+36,4);std::memcpy(&base,row+40,4);std::memcpy(&model,row+44,4);
  auto bytes=[](auto& v){return std::span(reinterpret_cast<uint8_t*>(&v),sizeof(v));};
  ReadNativeCUSPublishedElement(*d.transforms,base,bytes(p.root0));ReadNativeCUSPublishedElement(*d.transforms,size_t(base)+1,bytes(p.root1));
  const size_t offset=size_t(base)+2*(1+size_t(piece));ReadNativeCUSPublishedElement(*d.transforms,offset,bytes(p.piece0));ReadNativeCUSPublishedElement(*d.transforms,offset+1,bytes(p.piece1));ReadNativeCUSPublishedElement(*d.models,model,p.model);
 }
 return out;
}
}

namespace rendering {
std::vector<NativeOrbPublished> ReadNativeOrbPublished(const NativeUnitEffectInput& d){
 ValidateNativeUnitEffect(d);Check(d.orb,"Airjets cannot acquire Orb rows");std::vector<NativeOrbPublished> out(d.instanceCount);
 for(uint32_t n=0;n<d.instanceCount;++n){const auto* row=d.instances->bytes.data()+size_t(n)*80;uint32_t index;std::memcpy(&index,row+68,4);ReadNativeCUSPublishedElement(*d.models,index,out[n].model);
  float red;std::memcpy(&red,row+32,4);if(red<0){float team;std::memcpy(&team,row+20,4);Check(std::isfinite(team)&&team>=0&&team<255,"Orb original team row invalid");const auto bytes=ReadNativeCUSTeamColor(*d.original,uint32_t(team));std::memcpy(out[n].team.data(),bytes.data(),16);}
 }
 return out;
}
}

namespace rendering {
NativeParalyzePublished ReadNativeParalyzePublished(const NativeUnitEffectInput& d,size_t row){
 ValidateNativeUnitEffect(d);Check(d.paralyze&&row<d.submissions.size(),"Paralyze submission row missing");const auto& item=d.submissions[row];const auto* data=d.instances->bytes.data()+row*16;uint32_t base,model;std::memcpy(&base,data,4);std::memcpy(&model,data+4,4);
 NativeParalyzePublished result;ReadNativeCUSPublishedElement(*d.models,model,result.model);uint32_t composite;std::memcpy(&composite,result.model.data(),4);Check((composite>>16)==uint32_t(item.unitID),"Paralyze model publication belongs to another submitted unit");
 unsigned maxBone=0;for(const auto& piece:item.model->pieces){for(const auto& v:piece.vertices)maxBone=std::max(maxBone,unsigned(v.boneIds[0]&255));for(auto index:piece.indices)Check(index<piece.vertices.size(),"Paralyze source model index invalid");}
 result.transforms.resize(2+2*(1+maxBone));for(size_t n=0;n<result.transforms.size();++n)ReadNativeCUSPublishedElement(*d.transforms,size_t(base)+n,std::span(reinterpret_cast<uint8_t*>(result.transforms[n].data()),32));return result;
}
}
