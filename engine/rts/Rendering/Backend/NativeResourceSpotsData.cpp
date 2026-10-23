/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeResourceSpotsData.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace rendering {
namespace {
std::shared_ptr<const void> consumerLifetime;
void Check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void Buffer(const NativeBufferSnapshotRef& b,NativeBufferKind kind){Check(b&&b->object&&b->revision&&b->layout.kind==kind&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"Original resource spots buffer generation/layout/storage differs");}
void Attribute(const NativeBufferLayout& l,unsigned id,size_t offset,unsigned n){
	auto a=std::find_if(l.attributes.begin(),l.attributes.end(),[&](const auto& v){return v.id==id;});
	Check(a!=l.attributes.end()&&a->offset==offset&&a->scalarType==NativeScalarType::Float32&&a->valueShape==NativeValueShape::Scalar&&a->declaredCount==n&&a->byteSize==4*n&&!a->normalized,"Original resource spots actual vertex attribute ABI differs");
}
}
uint64_t NativeResourceSpotsSourceHash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
void SetNativeResourceSpotsConsumerLifetime(std::shared_ptr<const void> v){std::atomic_store(&consumerLifetime,std::move(v));}
std::shared_ptr<const void> NativeResourceSpotsConsumerLifetime(){return std::atomic_load(&consumerLifetime);}
bool IsNativeResourceSpotsConsumerAvailable(){return bool(NativeResourceSpotsConsumerLifetime());}
void ValidateNativeResourceSpotsInput(const NativeResourceSpotsInput& d){
 Check(d.kind==NativeResourceSpotKind::Metal||d.kind==NativeResourceSpotKind::Geothermal,"Unknown original resource spot family");
 const bool metal=d.kind==NativeResourceSpotKind::Metal;
 Check(d.vertexHash==(metal?resourcespots::MetalVertexHash:resourcespots::GeoVertexHash)&&d.fragmentHash==(metal?resourcespots::MetalFragmentHash:resourcespots::GeoFragmentHash),"Original31657 resource shader identity differs");
 Check(d.compiledFamily&&d.compiledFamily==NativeResourceSpotsConsumerLifetime(),"Resource spots lost the actual compiled family");
 Buffer(d.vertices,NativeBufferKind::Vertex);Buffer(d.indices,NativeBufferKind::Index);Buffer(d.instances,NativeBufferKind::Vertex);
 const auto& v=d.vertices->layout;Check(v.stride==16&&v.attributes.size()==1,"Original resource spot mesh layout differs");Attribute(v,0,0,4);
 const auto& instances=d.instances->layout;Check(instances.stride==(metal?48u:32u)&&instances.attributes.size()==(metal?3u:2u),"Original resource instance layout differs");
 Attribute(instances,1,0,4);Attribute(instances,2,16,4);if(metal)Attribute(instances,3,32,4);
 const auto& index=d.indices->layout;Check(index.stride==2&&index.attributes.size()==1&&index.attributes[0].scalarType==NativeScalarType::UInt16,"Original resource default unsigned-short index storage differs");
 Check(d.count&&d.count%3==0&&d.indexOffset<=index.elements&&d.count<=index.elements-d.indexOffset&&d.baseVertex==0,"Original indexed resource triangle interval differs");
 Check(d.instanceCount&&d.baseInstance<=instances.elements&&d.instanceCount<=instances.elements-d.baseInstance,"Original resource instances exceed actual buffer");
 for(size_t i=d.indexOffset;i<size_t(d.indexOffset)+d.count;++i){uint16_t n;std::memcpy(&n,d.indices->bytes.data()+2*i,2);Check(n<v.elements,"Original resource index exceeds actual mesh");}
 Check(d.original!=nullptr,"Resource spots lost actual engine UBO publication");ValidateNativeCUSUniformPublication(*d.original);
 Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame,"Resource spots captured a future engine UBO");
 Check(std::isfinite(d.drawPass)&&std::isfinite(d.waterLevel),"Resource spot uniform is nonfinite");
 if(metal)for(const auto& image:d.images)Check(image.knowledge==NativeCUSBindingKnowledge::BoundObject&&image.object&&image.object->target==NativeCUSTextureTarget::Image2D&&image.object->sampling.compareMode==0,"Original metal spot height/atlas sampler has no retained color owner");
 else for(const auto& image:d.images)Check(!image.object,"Original geothermal shader has no sampled image");
}
}
