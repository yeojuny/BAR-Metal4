/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeGrassData.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void Buffer(const NativeBufferSnapshotRef& b,NativeBufferKind kind){Check(b&&b->object&&b->revision&&b->layout.kind==kind&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"Original Grass buffer generation/layout/storage differs");}
void Attribute(const NativeBufferLayout& l,unsigned id,size_t offset,unsigned n){
	auto a=std::find_if(l.attributes.begin(),l.attributes.end(),[&](const auto& v){return v.id==id;});
	Check(a!=l.attributes.end()&&a->offset==offset&&a->scalarType==NativeScalarType::Float32&&a->valueShape==NativeValueShape::Scalar&&a->declaredCount==n&&a->byteSize==4*n&&!a->normalized,"Original Grass actual vertex attribute ABI differs");
}
}
uint64_t NativeGrassSourceHash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
void SetNativeGrassConsumerAvailable(bool b){available=b;}
bool IsNativeGrassConsumerAvailable(){return available;}
void ValidateNativeGrassInput(const NativeGrassInput& d){
	Check(d.vertexHash==grass::VertexHash&&d.fragmentHash==grass::FragmentHash,"Original31657 Grass shader source identity differs");
	Buffer(d.vertices,NativeBufferKind::Vertex);Buffer(d.indices,NativeBufferKind::Index);Buffer(d.instances,NativeBufferKind::Vertex);
	const auto& v=d.vertices->layout;
	if(d.defines[12]==1){Check(v.stride==32&&v.attributes.size()==2,"Original Grass compact blade layout differs");Attribute(v,0,0,4);Attribute(v,1,16,4);}
	else{Check(v.stride==68&&v.attributes.size()==7,"Original Grass full blade layout differs");const unsigned sizes[]={3,3,3,3,2,2,1};size_t offset=0;for(unsigned i=0;i<7;++i){Attribute(v,i,offset,sizes[i]);offset+=sizes[i]*4;}}
	const auto& instances=d.instances->layout;Check(instances.stride==16&&instances.attributes.size()==1,"Original Grass instance layout differs");Attribute(instances,7,0,4);
	const auto& index=d.indices->layout;Check(index.stride==2&&index.attributes.size()==1&&index.attributes[0].scalarType==NativeScalarType::UInt16,"Original Grass original default unsigned-short index storage differs");
	Check(d.count&&d.count%3==0&&d.indexOffset<=index.elements&&d.count<=index.elements-d.indexOffset&&d.baseVertex==0,"Original Grass indexed triangle interval differs");
	Check(d.instanceCount&&d.baseInstance<=instances.elements&&d.instanceCount<=instances.elements-d.baseInstance,"Original Grass contiguous cull run exceeds its instance buffer");
	for(size_t i=d.indexOffset;i<size_t(d.indexOffset)+d.count;++i){uint16_t n;std::memcpy(&n,d.indices->bytes.data()+2*i,2);Check(n<v.elements,"Original Grass index exceeds its actual deduplicated blade mesh");}
	Check(d.original!=nullptr,"Original Grass lost its actual engine UBO publication");ValidateNativeCUSUniformPublication(*d.original);
	Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame,"Original Grass captured a future engine UBO");
	for(float x:d.defines)Check(std::isfinite(x),"Original Grass compile definition is nonfinite");
	for(float x:d.uniforms.wind)Check(std::isfinite(x),"Original Grass wind/alpha uniform is nonfinite");for(float x:d.uniforms.night)Check(std::isfinite(x),"Original Grass night uniform is nonfinite");
	Check(std::isfinite(d.uniforms.distanceMult)&&d.uniforms.unitBendCount>=0&&d.uniforms.unitBendCount<=128,"Original Grass actual fade/bend uniforms differ");
	for(unsigned slot:{0u,1u,2u,4u,5u})Check(d.images[slot].knowledge==NativeCUSBindingKnowledge::BoundObject&&d.images[slot].object&&d.images[slot].object->target==NativeCUSTextureTarget::Image2D,"Original Grass active sampled image has no retained owner");
	if(d.defines[9]==1)Check(d.images[3].knowledge==NativeCUSBindingKnowledge::BoundObject&&d.images[3].object&&d.images[3].object->sampling.compareMode==0x884e,"Original Grass sampler2DShadow has no actual comparison depth binding");
	if(d.defines[13]==1){
		Buffer(d.bend.snapshot,NativeBufferKind::Storage);const auto& b=d.bend;
		// Keep the ORIGINAL Define(size=4) allocation. Its uploaded float stream
		// and the shader's vec4[] read are contiguous; do not repack its records.
		Check(b.binding==6&&b.offset%16==0&&b.offset<=b.snapshot->bytes.size()&&b.size<=b.snapshot->bytes.size()-b.offset&&b.size>=size_t(d.uniforms.unitBendCount)*16,"Original Grass actual SSBO6 range differs");
	}else Check(!d.bend.snapshot,"Original Grass disabled bending cannot invent an SSBO producer");
}
}
