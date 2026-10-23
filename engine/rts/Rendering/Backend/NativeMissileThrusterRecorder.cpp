/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMissileThrusterRecorder.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool valid, const char* message) { if (!valid) throw std::invalid_argument(message); }
float Float(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
	float value; std::memcpy(&value,bytes.data()+offset,sizeof(value)); return value;
}
void Snapshot(const NativeBufferSnapshotRef& ref)
{
	Check(ref && ref->object && ref->revision,"Original MissileThruster buffer identity is missing");
	Check(ref->bytes.size()==CheckedNativeBufferByteSize(ref->layout),"Original MissileThruster buffer storage extent differs");
}
}
void ValidateNativeMissileThrusterDraw(const NativeMissileThrusterDraw& draw)
{
	Snapshot(draw.vertices); Snapshot(draw.indices); Snapshot(draw.instances);
	Check(unsigned(draw.program)<7,"Original MissileThruster program kind differs");
	const bool game31657=unsigned(draw.program)>=unsigned(NativeMissileThrusterProgram::Flame31657);
	const unsigned vertexCount=draw.program==NativeMissileThrusterProgram::Flame31657?12u:6u;
	Check(draw.vertexCount==vertexCount&&(!game31657||!draw.indexed),"Original MissileThruster draw count/method differs");
	const auto& vertex=draw.vertices->layout; const auto& index=draw.indices->layout; const auto& instance=draw.instances->layout;
	Check(vertex.kind==NativeBufferKind::Vertex && vertex.elements==(game31657?12u:6u) && vertex.stride==16 && vertex.attributes.size()==1,"Original MissileThruster rectangle layout differs");
	const auto& a=vertex.attributes.front();
	Check(a.id==0 && a.offset==0 && a.byteSize==16 && a.declaredCount==4 && a.scalarType==NativeScalarType::Float32 && !a.normalized,"Original MissileThruster rectangle attribute differs");
	if(game31657){
		constexpr std::array<float,12> corners{-1,-1,-1,1,1,1,1,1,1,-1,-1,-1};
		for(unsigned n=0;n<12;++n){const std::array<float,4> expected{corners[(n%6)*2],corners[(n%6)*2+1],n>=6?1.f:0.f,0.f};for(unsigned c=0;c<4;++c)Check(Float(draw.vertices->bytes,(n*4+c)*4)==expected[c],"Pinned31657 MissileThruster crossed-plane template differs");}
	}else{
		constexpr std::array<float,24> rectangle{-1,-1,0,0,-1,1,0,1,1,1,1,1,1,1,1,1,1,-1,1,0,-1,-1,0,0};
		for(std::size_t n=0;n<rectangle.size();++n)Check(Float(draw.vertices->bytes,n*4)==rectangle[n],"Original MissileThruster rectangle vertex/UV order differs");
	}
	Check(index.kind==NativeBufferKind::Index&&index.elements==(game31657?1u:6u)&&(index.stride==2||index.stride==4),"Original MissileThruster index layout differs");
	for(unsigned n=0;n<index.elements;++n){std::uint32_t value=0;std::memcpy(&value,draw.indices->bytes.data()+n*index.stride,index.stride);Check(value==(game31657?0u:n),"Original MissileThruster index order differs");}
	const unsigned attributeCount=game31657?6:5,stride=attributeCount*16;
	Check(instance.kind==NativeBufferKind::Vertex&&instance.stride==stride&&instance.attributes.size()==attributeCount&&draw.instanceCount<=instance.elements,"Original MissileThruster instance layout/range differs");
	for(unsigned n=0;n<attributeCount;++n){const auto& f=instance.attributes[n];Check(f.id==n+1&&f.offset==n*16&&f.byteSize==16&&f.declaredCount==4&&f.scalarType==NativeScalarType::Float32&&!f.normalized,"Original MissileThruster instance attribute differs");}
	for(std::size_t n=0;n<std::size_t(draw.instanceCount)*stride;n+=4)Check(std::isfinite(Float(draw.instances->bytes,n)),"Original MissileThruster instance is nonfinite");
	for (float value:draw.defines) Check(std::isfinite(value),"Original MissileThruster shader define is nonfinite");

 const bool muzzle=draw.program==NativeMissileThrusterProgram::Flame || draw.program==NativeMissileThrusterProgram::CrossFlame || draw.program==NativeMissileThrusterProgram::Flame31657;
 const auto& image=draw.texture;Check(image && image->sourceName==(muzzle?"bitmaps/projectiletextures/muzzleside.tga":"bitmaps/projectiletextures/glow2.tga") && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm && !image->flipV && !image->renderTarget && !image->postprocessVersion,"Original MissileThruster atlas identity/format differs");
 Check(image->width && image->height && std::size_t(image->width)<=std::numeric_limits<std::size_t>::max()/image->height/4 && image->bytesPerRow==std::size_t(image->width)*4 && image->bytes.size()==std::size_t(image->width)*image->height*4 && image->mips.empty() && image->generateMipmaps,"Original MissileThruster real atlas storage/mip policy differs");
}
void NativeMissileThrusterRecorder::Begin()
{
	if (frame) throw std::logic_error("Original MissileThruster world scope is already active");
	frame=std::make_shared<NativeMissileThrusterFrame>();
}
void NativeMissileThrusterRecorder::Record(NativeMissileThrusterDraw draw)
{
	if (!frame) throw std::logic_error("Original MissileThruster draw is outside its world scope");
	ValidateNativeMissileThrusterDraw(draw); draw.sequence=NextNativeWorldDrawSequence(); frame->draws.push_back(std::move(draw));
}
void NativeMissileThrusterRecorder::SetFrameResources(const NativeMissileThrusterFrame& resources)
{
	if (!frame) throw std::logic_error("Original MissileThruster frame resources have no world scope");
	for (const auto* matrix:{&resources.cameraViewProjection,&resources.cameraViewInverse}) for (float value:*matrix) Check(std::isfinite(value),"Original MissileThruster camera is nonfinite");
	for (float value:resources.timeInfo) Check(std::isfinite(value),"Original MissileThruster time is nonfinite");
	frame->cameraViewProjection=resources.cameraViewProjection; frame->cameraViewInverse=resources.cameraViewInverse; frame->timeInfo=resources.timeInfo;
}
std::shared_ptr<const NativeMissileThrusterFrame> NativeMissileThrusterRecorder::End()
{
	if (!frame) throw std::logic_error("Original MissileThruster world scope is inactive");
	return std::exchange(frame,{});
}
void NativeMissileThrusterRecorder::Abort() noexcept { frame.reset(); }
void SetNativeMissileThrusterConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeMissileThrusterConsumerAvailable() noexcept { return available.load(); }
}
