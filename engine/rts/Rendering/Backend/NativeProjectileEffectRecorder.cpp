/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeProjectileEffectRecorder.h"
#include "NativeProjectileEffectPrograms.h"
#include "NativeWorldDrawSequence.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool valid,const char* message){if(!valid)throw std::invalid_argument(message);}
float Float(const std::vector<std::uint8_t>& bytes,size_t offset){float value;std::memcpy(&value,bytes.data()+offset,4);return value;}
void Snapshot(const NativeBufferSnapshotRef& ref){Check(bool(ref),"Original projectile effect is missing a buffer");Check(ref->object&&ref->revision&&ref->bytes.size()==CheckedNativeBufferByteSize(ref->layout),"Original projectile effect buffer identity/extent mismatch");}
}
void ValidateNativeProjectileEffectDraw(const NativeProjectileEffectDraw& draw)
{
	Snapshot(draw.vertices);Snapshot(draw.indices);Snapshot(draw.instances);
	const auto& v=draw.vertices->layout;const auto& i=draw.indices->layout;const auto& inst=draw.instances->layout;
	const auto kind=static_cast<unsigned>(draw.program);
	Check(kind<NativeProjectilePrograms.size(),"Unknown original31657 projectile program");
	const bool beam=kind<4;
	Check(v.kind==NativeBufferKind::Vertex&&v.elements==(beam?6u:12u)&&v.stride==16&&v.attributes.size()==1,"Original31657 projectile vertex schema differs");
	const auto& a=v.attributes.front();Check(a.id==0&&a.offset==0&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Original projectile vertex attribute differs");
	constexpr std::array<float,24> quad{-1,-1,0,0,-1,1,0,1,1,1,1,1,1,1,1,1,1,-1,1,0,-1,-1,0,0};
	for(unsigned n=0;n<v.elements;++n){
		const unsigned corner=n%6;
		const std::array<float,4> expected=beam?std::array<float,4>{quad[corner*4],quad[corner*4+1],quad[corner*4+2],quad[corner*4+3]}:std::array<float,4>{quad[corner*4],quad[corner*4+1],float(n/6),0};
		for(unsigned c=0;c<4;++c)Check(Float(draw.vertices->bytes,size_t(n)*16+c*4)==expected[c],"Original31657 crossed-plane/quad vertex order differs");
	}
	Check(draw.vertexCount==(draw.program==NativeProjectileProgram::Plasma?12u:6u)&&(!draw.indexed||beam),"Original31657 projectile draw extent differs");
	Check(i.kind==NativeBufferKind::Index&&i.elements==(beam?6u:1u)&&(i.stride==2||i.stride==4),"Original31657 index or unused placeholder schema differs");
	for(unsigned n=0;n<i.elements;++n){uint32_t value=0;std::memcpy(&value,draw.indices->bytes.data()+n*i.stride,i.stride);Check(value==(beam?n:0u),"Original31657 real index/placeholder contents differ");}
	const unsigned attributes=beam?5:4;
	Check(inst.kind==NativeBufferKind::Vertex&&inst.stride==attributes*16&&inst.attributes.size()==attributes&&draw.instanceCount<=inst.elements,"Original beam/plasma instance capacity/stride differs");
	for(unsigned n=0;n<attributes;++n){const auto& field=inst.attributes[n];Check(field.id==n+1&&field.offset==n*16&&field.byteSize==16&&field.declaredCount==4&&field.scalarType==NativeScalarType::Float32&&!field.normalized,"Original beam/plasma instance attribute differs");}
	for(size_t n=0;n<size_t(draw.instanceCount)*inst.stride;n+=4)Check(std::isfinite(Float(draw.instances->bytes,n)),"Original projectile effect has nonfinite instance data");
	for(const auto& [name,value]:draw.parameters)Check(std::find(NativeProjectileParameterNames.begin(),NativeProjectileParameterNames.end(),name)!=NativeProjectileParameterNames.end()&&std::isfinite(value),"Original projectile effect has invalid parameters");
	for(size_t n=0;n<NativeProjectileParameterNames.size();++n)if(NativeProjectileRequiredParameters[static_cast<unsigned>(draw.program)][n])
		Check(draw.parameters.contains(std::string(NativeProjectileParameterNames[n])),"Original projectile effect has a missing shader define");
	const char* path=nullptr;
	switch(draw.program){case NativeProjectileProgram::Beam:path="bitmaps/projectiletextures/largebeam.tga";break;case NativeProjectileProgram::BeamFlare:path="bitmaps/projectiletextures/flare2.tga";break;case NativeProjectileProgram::PlasmaGlow:path="bitmaps/projectiletextures/glow2.tga";break;default:break;}
	if(path){Check(draw.texture&&draw.texture->sourceName==path,"Original projectile effect named texture differs");const auto& t=*draw.texture;Check(t.width&&t.height&&size_t(t.width)<=std::numeric_limits<size_t>::max()/t.height/4&&t.rgba.size()==size_t(t.width)*t.height*4,"Original projectile effect image extent differs");}
}
void NativeProjectileEffectRecorder::Begin(){if(frame)throw std::logic_error("Native projectile effect world scope already active");frame=std::make_shared<NativeProjectileEffectFrame>();}
void NativeProjectileEffectRecorder::Record(NativeProjectileEffectDraw draw){if(!frame)throw std::logic_error("Original projectile effect submission outside world scope");ValidateNativeProjectileEffectDraw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.emplace_back(std::move(draw));}
void NativeProjectileEffectRecorder::SetFrameResources(const NativeProjectileEffectFrame& resources){if(!frame)throw std::logic_error("Native projectile effect resources outside world scope");for(const auto* matrix:{&resources.cameraViewProjection,&resources.cameraViewInverse})for(float f:*matrix)Check(std::isfinite(f),"Original projectile effect camera is nonfinite");for(float f:resources.timeInfo)Check(std::isfinite(f),"Original projectile effect time is nonfinite");frame->cameraViewProjection=resources.cameraViewProjection;frame->cameraViewInverse=resources.cameraViewInverse;frame->timeInfo=resources.timeInfo;}
std::shared_ptr<const NativeProjectileEffectFrame> NativeProjectileEffectRecorder::End(){if(!frame)throw std::logic_error("Native projectile effect scope is inactive");return std::exchange(frame,{});}
void NativeProjectileEffectRecorder::Abort() noexcept{frame.reset();}
void SetNativeProjectileEffectsConsumerAvailable(bool value) noexcept{available.store(value);}
bool IsNativeProjectileEffectsConsumerAvailable() noexcept{return available.load();}
}
