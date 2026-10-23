/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeEnergyExplosionRecorder.h"
#include "NativeEnergyExplosionProgram.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool value,const char* text){if(!value)throw std::invalid_argument(text);}
float Float(const std::vector<uint8_t>& bytes,size_t offset){float v;std::memcpy(&v,bytes.data()+offset,4);return v;}
void Snapshot(const NativeBufferSnapshotRef& v){Check(v&&v->object&&v->revision&&v->bytes.size()==CheckedNativeBufferByteSize(v->layout),"Energy burst buffer identity/extent differs");}
}
void SetNativeEnergyExplosionConsumerAvailable(bool v) noexcept{available.store(v);}
bool IsNativeEnergyExplosionConsumerAvailable() noexcept{return available.load();}
void ValidateNativeEnergyExplosionDraw(const NativeEnergyExplosionDraw& d){
	Snapshot(d.vertexSlots);Snapshot(d.indices);Snapshot(d.instances);
	const auto& v=d.vertexSlots->layout;const auto& i=d.indices->layout;const auto& n=d.instances->layout;
	Check(v.kind==NativeBufferKind::Vertex&&v.elements==28&&v.stride==4&&v.attributes.size()==1,"Energy burst needs original28 template slots");
	const auto& a=v.attributes.front();Check(a.id==0&&a.offset==0&&a.byteSize==4&&a.declaredCount==1&&a.scalarType==NativeScalarType::Float32&&a.valueShape==NativeValueShape::Scalar&&!a.normalized,"Energy burst template attribute differs");
	for(unsigned k=0;k<28;++k)Check(Float(d.vertexSlots->bytes,k*4)==float(k),"Energy burst template order differs");
	Check(d.uniforms.size()==NativeEnergyUniforms.size(),"Energy burst missing/excess original uniforms");
	for(const auto& spec:NativeEnergyUniforms){const auto found=d.uniforms.find(std::string(spec.name));Check(found!=d.uniforms.end()&&found->second.size()==spec.count,"Energy burst uniform cardinality differs");for(float value:found->second)Check(std::isfinite(value),"Energy burst nonfinite uniform");}
	const auto shape=d.uniforms.at("u_shape")[0];Check(shape==0||shape==1,"Unsupported original energy burst shape");
	const unsigned count=shape==0?42:30;Check(i.kind==NativeBufferKind::Index&&i.elements==count&&i.attributes.size()==1&&((i.stride==2&&i.attributes.front().scalarType==NativeScalarType::UInt16)||(i.stride==4&&i.attributes.front().scalarType==NativeScalarType::UInt32)),"Energy burst original indexed topology schema differs");
	const auto& indexAttribute=i.attributes.front();
	Check(indexAttribute.id==0&&indexAttribute.offset==0&&indexAttribute.byteSize==i.stride&&indexAttribute.declaredCount==1&&indexAttribute.valueShape==NativeValueShape::Scalar&&!indexAttribute.normalized,"Energy burst original scalar index attribute differs");
	std::vector<unsigned> expected;
	if(shape==0)for(unsigned q=0;q<6;++q)for(unsigned c:{0u,1u,2u,2u,1u,3u})expected.push_back(q*4+c);
	else for(unsigned k=0;k<24;++k)expected.push_back(k);
	for(unsigned c:{0u,1u,2u,2u,1u,3u})expected.push_back(24+c);
	for(unsigned k=0;k<count;++k){uint32_t value=0;std::memcpy(&value,d.indices->bytes.data()+k*i.stride,i.stride);Check(value==expected[k],"Energy burst original triangle strip expansion differs");}
	Check(n.kind==NativeBufferKind::Vertex&&n.stride==64&&n.attributes.size()==4&&d.firstInstance<=n.elements&&d.instanceCount<=n.elements-d.firstInstance,"Energy burst instance range/stride differs");
	for(unsigned k=0;k<4;++k){const auto& f=n.attributes[k];Check(f.id==k+1&&f.offset==k*16&&f.byteSize==16&&f.declaredCount==4&&f.scalarType==NativeScalarType::Float32&&f.valueShape==NativeValueShape::Scalar&&!f.normalized,"Energy burst original four-vec4 layout differs");}
	for(size_t k=size_t(d.firstInstance)*64;k<(size_t(d.firstInstance)+d.instanceCount)*64;k+=4)Check(std::isfinite(Float(d.instances->bytes,k)),"Energy burst selected instance record is nonfinite");
}
std::vector<std::uint16_t> NativeEnergyExplosionGSIndices(const NativeEnergyExplosionDraw& d) {
	ValidateNativeEnergyExplosionDraw(d);
	std::vector<std::uint16_t> out;
	out.reserve(d.indices->layout.elements);
	if (d.uniforms.at("u_shape")[0] == 0) {
		for (unsigned face=0; face<6; ++face)
			for (unsigned corner: {0u,1u,2u,2u,1u,3u}) out.push_back(face*4+corner);
	} else {
		for (unsigned corner=0; corner<24; ++corner) out.push_back(corner);
	}
	for (unsigned corner: {0u,1u,2u,2u,1u,3u}) out.push_back(24+corner);
	return out;
}
void NativeEnergyExplosionRecorder::Begin(){if(frame)throw std::logic_error("Energy burst frame is already active");frame=std::make_shared<NativeEnergyExplosionFrame>();}
void NativeEnergyExplosionRecorder::Record(NativeEnergyExplosionDraw d){if(!frame)throw std::logic_error("Energy burst draw outside actual world scope");ValidateNativeEnergyExplosionDraw(d);d.sequence=NextNativeWorldDrawSequence();frame->draws.emplace_back(std::move(d));}
void NativeEnergyExplosionRecorder::SetFrameResources(const NativeEnergyExplosionFrame& r){if(!frame)throw std::logic_error("Energy burst resources outside world scope");for(const auto* m:{&r.cameraViewProjection,&r.cameraViewInverse})for(float v:*m)Check(std::isfinite(v),"Energy burst camera is nonfinite");for(float v:r.timeInfo)Check(std::isfinite(v),"Energy burst timeInfo is nonfinite");frame->cameraViewProjection=r.cameraViewProjection;frame->cameraViewInverse=r.cameraViewInverse;frame->timeInfo=r.timeInfo;}
std::shared_ptr<const NativeEnergyExplosionFrame> NativeEnergyExplosionRecorder::End(){if(!frame)throw std::logic_error("Energy burst frame inactive");return std::exchange(frame,{});}
void NativeEnergyExplosionRecorder::Abort() noexcept{frame.reset();}
}
