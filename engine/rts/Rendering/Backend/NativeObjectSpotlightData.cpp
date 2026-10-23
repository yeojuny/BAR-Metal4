/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeObjectSpotlightData.h"
#include "NativeCommanderWreckPrograms.h"
#include <atomic>
#include <cmath>
#include <cstring>
namespace rendering {
namespace {
std::shared_ptr<const void> consumer;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void Snapshot(const NativeBufferSnapshotRef& b){
	Check(b&&b->object&&b->revision&&b->bytes.size()==CheckedNativeBufferByteSize(b->layout),"Object Spotlight actual immutable buffer missing");
}
void Attribute(const NativeBufferLayout& l,unsigned id,std::string_view name,unsigned count,size_t offset,NativeScalarType type){
	const auto it=std::find_if(l.attributes.begin(),l.attributes.end(),[=](const auto& a){return a.id==id;});
	Check(it!=l.attributes.end()&&it->name==name&&it->declaredCount==count&&it->offset==offset&&it->byteSize==count*4&&it->scalarType==type&&it->valueShape==NativeValueShape::Scalar&&!it->normalized,"Object Spotlight original attribute layout differs");
}
float Scalar(const uint8_t* b){float out;std::memcpy(&out,b,4);Check(std::isfinite(out),"Object Spotlight source scalar is nonfinite");return out;}
}
uint64_t NativeObjectSpotlightSourceHash(std::string_view s){uint64_t h=14695981039346656037ULL;for(unsigned char c:s)h=(h^c)*1099511628211ULL;return h;}
void SetNativeObjectSpotlightConsumer(std::shared_ptr<const void> value){std::atomic_store(&consumer,std::move(value));}
std::shared_ptr<const void> NativeObjectSpotlightConsumer(){return std::atomic_load(&consumer);}
bool IsNativeObjectSpotlightConsumerAvailable(){return bool(std::atomic_load(&consumer));}
void ValidateNativeObjectSpotlightInput(const NativeObjectSpotlightInput& d){
	Check(d.compiledFamily&&d.vertexHash==(d.commanderWreck?commanderwreck::VertexHash:objectspotlight::VertexHash)&&d.fragmentHash==(d.commanderWreck?commanderwreck::FragmentHash:objectspotlight::FragmentHash),"Object Spotlight lost its actual paired program identity/lifetime");
	Snapshot(d.vertices);Snapshot(d.instances);const auto& v=d.vertices->layout;const auto& i=d.instances->layout;
 if(d.commanderWreck){
  Check(v.kind==NativeBufferKind::Vertex&&v.stride==12&&v.elements==34&&v.attributes.size()==1,"Commander Wreck original16-section cylinder differs");Attribute(v,0,"position",3,0,NativeScalarType::Float32);
  Check(i.kind==NativeBufferKind::Vertex&&i.stride==36&&i.attributes.size()==4,"Commander Wreck original36-byte instance differs");
  Attribute(i,1,"position",3,0,NativeScalarType::Float32);Attribute(i,2,"radius",1,12,NativeScalarType::Float32);Attribute(i,3,"height",1,16,NativeScalarType::Float32);Attribute(i,4,"color",4,20,NativeScalarType::Float32);
  Check(d.first==0&&d.count==34&&d.firstInstance==0&&d.instanceCount&&d.instanceCount<=i.elements&&!d.models,"Commander Wreck original draw/CPU position ownership differs");
 }else{
	Check(v.kind==NativeBufferKind::Vertex&&v.stride==12&&v.elements==66&&v.attributes.size()==1,"Object Spotlight original32-section cylinder layout differs");
	Attribute(v,0,"localPos",3,0,NativeScalarType::Float32);
	Check(i.kind==NativeBufferKind::Vertex&&i.stride==60&&i.attributes.size()==7,"Object Spotlight original instance stride differs");
	Attribute(i,1,"radius",1,0,NativeScalarType::Float32);Attribute(i,2,"height",1,4,NativeScalarType::Float32);
	Attribute(i,3,"color",4,8,NativeScalarType::Float32);Attribute(i,4,"startTime",1,24,NativeScalarType::Float32);Attribute(i,5,"expireTime",1,28,NativeScalarType::Float32);
	Attribute(i,6,"instData",4,32,NativeScalarType::UInt32);Attribute(i,7,"worldPosOverride",3,48,NativeScalarType::Float32);
	Check(d.first==0&&d.count==66&&d.firstInstance==0&&d.instanceCount&&d.instanceCount<=i.elements,"Object Spotlight actual original draw interval differs");
 }
	Check(d.original!=nullptr,"Object Spotlight original engine uniforms missing");ValidateNativeCUSUniformPublication(*d.original);
	Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame,"Object Spotlight original UBO is from a future frame");
	for(size_t offset=0;offset<d.vertices->bytes.size();offset+=4)Scalar(d.vertices->bytes.data()+offset);
	for(uint32_t n=0;n<d.instanceCount;++n){const auto* b=d.instances->bytes.data()+size_t(d.firstInstance+n)*(d.commanderWreck?36:60);
  if(d.commanderWreck){for(unsigned offset=0;offset<36;offset+=4)Scalar(b+offset);continue;}
		for(unsigned offset=0;offset<32;offset+=4)Scalar(b+offset);for(unsigned offset=48;offset<60;offset+=4)Scalar(b+offset);
	}
	for(size_t offset:{objectspotlight::ViewProjectionByte,objectspotlight::ViewInverseByte})for(unsigned n=0;n<16;++n)Scalar(d.original->matrices->bytes.data()+offset+n*4);
	Scalar(d.original->params->bytes.data()+objectspotlight::DrawSecondsByte);
}
std::vector<std::array<float,4>> ReadNativeObjectSpotlightModelPositions(const NativeObjectSpotlightInput& d){
	ValidateNativeObjectSpotlightInput(d);Check(!d.commanderWreck,"Commander Wreck must not acquire model position backing");std::vector<std::array<float,4>> out(d.instanceCount);
	std::array<uint8_t,128> row;
	for(uint32_t n=0;n<d.instanceCount;++n){const auto* b=d.instances->bytes.data()+size_t(d.firstInstance+n)*60;auto& p=out[n];
		const float x=Scalar(b+48),y=Scalar(b+52),z=Scalar(b+56);
		// This predicate validates source-read requirements only. Never copy an
		// override into model data or choose the final shader world position.
		const bool modelRequired=x==0||y==0||z==0;
		uint32_t index;std::memcpy(&index,b+36,4);
		const bool observed=d.models&&d.models->kind==NativeCUSPublishedKind::ModelUniforms&&d.models->stride==128&&d.models->publishedDrawFrame<=d.sourceDrawFrame&&NativeCUSPublishedElementObserved(*d.models,index);
		if(!observed){Check(!modelRequired,"Object Spotlight requires an actual observed model drawPos for the original VS branch");continue;}
		ReadNativeCUSPublishedElement(*d.models,index,row);std::memcpy(p.data(),row.data()+32,16);
		if(modelRequired)for(unsigned axis=0;axis<3;++axis)Check(std::isfinite(p[axis]),"Object Spotlight observed object drawPos is nonfinite");
	}
	return out;
}
}
