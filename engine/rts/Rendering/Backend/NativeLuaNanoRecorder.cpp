/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeLuaNanoRecorder.h"
#include "NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace rendering {
namespace {
std::atomic<bool> consumerAvailable{false};
std::atomic<bool> engineConsumerAvailable{false};
std::atomic<uint64_t> nextSerial{1};
void Check(bool valid,const char* message) {if(!valid)throw std::invalid_argument(message);}
void Snapshot(const NativeBufferSnapshotRef& source)
{
	Check(bool(source),"Original Lua nano draw has a missing native buffer");
	Check(source->object&&source->revision&&source->bytes.size()==CheckedNativeBufferByteSize(source->layout),"Original Lua nano buffer identity/extent mismatch");
}
float Float(const std::vector<uint8_t>& bytes,size_t offset) {float value;std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;}
}
void ValidateNativeLuaNanoDraw(const NativeLuaNanoDraw& draw)
{
	Snapshot(draw.vertexSlots);Snapshot(draw.indices);Snapshot(draw.instances);
	const auto& v=draw.vertexSlots->layout;const auto& i=draw.indices->layout;const auto& instance=draw.instances->layout;
	Check(v.kind==NativeBufferKind::Vertex&&v.elements==28&&v.stride==4&&v.attributes.size()==1,"Original Lua nano template must contain 28 float slots");
	const auto& slot=v.attributes.front();Check(slot.id==0&&slot.offset==0&&slot.byteSize==4&&slot.scalarType==NativeScalarType::Float32&&slot.declaredCount==1&&!slot.normalized,"Original Lua nano template attribute differs");
	for(unsigned n=0;n<28;++n)Check(Float(draw.vertexSlots->bytes,n*4)==float(n),"Original Lua nano template slot order differs");
	Check(draw.shape==0||draw.shape==1,"Unsupported original Lua nano shape family");
	const unsigned count=draw.shape==0?42:30;
	Check(i.kind==NativeBufferKind::Index&&i.elements==count&&((i.stride==2&&i.attributes.front().scalarType==NativeScalarType::UInt16)||(i.stride==4&&i.attributes.front().scalarType==NativeScalarType::UInt32)),"Original Lua nano triangle index schema differs");
	std::vector<uint32_t> expected;expected.reserve(count);
	if(draw.shape==0)for(unsigned q=0;q<6;++q)for(unsigned corner:{0u,1u,2u,0u,2u,3u})expected.push_back(q*4+corner);
	else for(unsigned n=0;n<24;++n)expected.push_back(n);
	for(unsigned corner:{0u,1u,2u,0u,2u,3u})expected.push_back(24+corner);
	for(unsigned n=0;n<count;++n){uint32_t value=0;std::memcpy(&value,draw.indices->bytes.data()+n*i.stride,i.stride);Check(value==expected[n],"Original Lua nano indexed topology/order differs");}
	Check(instance.kind==NativeBufferKind::Vertex&&instance.stride==64&&instance.attributes.size()==4&&draw.instanceCount<=instance.elements,"Original Lua nano instance capacity/schema differs");
	for(unsigned n=0;n<4;++n){const auto& a=instance.attributes[n];Check(a.id==n+1&&a.offset==n*16&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Original Lua nano four-vec4 attribute layout differs");}
	for(size_t offset=0;offset<size_t(draw.instanceCount)*64;offset+=4)Check(std::isfinite(Float(draw.instances->bytes,offset)),"Original Lua nano instance contains nonfinite geometry/state");
	for(const auto& [name,value]:draw.uniforms){Check(!name.empty()&&std::isfinite(value),"Original Lua nano has invalid uniform state");}
}
void NativeLuaNanoRecorder::Begin()
{
	if(frame)throw std::logic_error("Native Lua nano world scope already active");
	frame=std::make_shared<NativeLuaNanoFrame>();frame->serial=nextSerial.fetch_add(1);
}
void NativeLuaNanoRecorder::Record(NativeLuaNanoDraw draw)
{
	if(!frame)throw std::logic_error("Original Lua nano submission outside world scope");
	// Original RunCallIn may catch one gadget's Lua error and continue the
	// world callback. Reject this draw atomically while retaining valid prior
	// commands and an End-able frame; Game owns whole-frame Abort on exception.
	ValidateNativeLuaNanoDraw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.emplace_back(std::move(draw));
}
void NativeLuaNanoRecorder::SetFrameResources(const NativeLuaNanoFrame& resources)
{
	if(!frame)throw std::logic_error("Native Lua nano frame resources outside world scope");
	for(float value:resources.cameraViewInverse)Check(std::isfinite(value),"Native Lua nano camera source is nonfinite");
	for(float value:resources.cameraViewProjection)Check(std::isfinite(value),"Native Lua nano camera product source is nonfinite");
	Check(resources.mapSquares[0]&&resources.mapSquares[1]&&resources.mapSquares[2]>=resources.mapSquares[0]&&resources.mapSquares[3]>=resources.mapSquares[1],"Native Lua nano original map extent is invalid");
	frame->info=resources.info;frame->infoBackground=resources.infoBackground;frame->infoIsNormal=resources.infoIsNormal;
	frame->cameraViewInverse=resources.cameraViewInverse;frame->cameraViewProjection=resources.cameraViewProjection;frame->mapSquares=resources.mapSquares;
}
std::shared_ptr<const NativeLuaNanoFrame> NativeLuaNanoRecorder::End()
{
	if(!frame)throw std::logic_error("Native Lua nano world scope is inactive or aborted");
	return std::exchange(frame,{});
}
void NativeLuaNanoRecorder::Abort() noexcept {frame.reset();}
void SetNativeLuaNanoConsumerAvailable(bool value) noexcept {consumerAvailable.store(value);}
bool IsNativeLuaNanoConsumerAvailable() noexcept {return consumerAvailable.load();}
void SetNativeEngineNanoConsumerAvailable(bool value) noexcept {engineConsumerAvailable.store(value);}
bool IsNativeEngineNanoConsumerAvailable() noexcept {return engineConsumerAvailable.load();}
}
