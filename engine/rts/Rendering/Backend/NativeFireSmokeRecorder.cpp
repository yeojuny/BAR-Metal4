/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeFireSmokeRecorder.h"
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
	Check(ref && ref->object && ref->revision,"Original FireSmoke buffer identity is missing");
	Check(ref->bytes.size()==CheckedNativeBufferByteSize(ref->layout),"Original FireSmoke buffer storage extent differs");
}
}
void ValidateNativeFireSmokeDraw(const NativeFireSmokeDraw& draw)
{
	Snapshot(draw.vertices); Snapshot(draw.indices); Snapshot(draw.instances);
	const auto& vertex=draw.vertices->layout; const auto& index=draw.indices->layout; const auto& instance=draw.instances->layout;
	Check(vertex.kind==NativeBufferKind::Vertex && vertex.elements==6 && vertex.stride==16 && vertex.attributes.size()==1,"Original FireSmoke rectangle layout differs");
	const auto& a=vertex.attributes.front();
	Check(a.id==0 && a.offset==0 && a.byteSize==16 && a.declaredCount==4 && a.scalarType==NativeScalarType::Float32 && !a.normalized,"Original FireSmoke rectangle attribute differs");
	constexpr std::array<float,24> rectangle{-1,-1,0,0,-1,1,0,1,1,1,1,1,1,1,1,1,1,-1,1,0,-1,-1,0,0};
	for (std::size_t n=0;n<rectangle.size();++n) Check(Float(draw.vertices->bytes,n*4)==rectangle[n],"Original FireSmoke rectangle vertex/UV order differs");
	Check(index.kind==NativeBufferKind::Index && index.elements==6 && (index.stride==2 || index.stride==4),"Original FireSmoke index layout differs");
	for (unsigned n=0;n<6;++n) { std::uint32_t value=0; std::memcpy(&value,draw.indices->bytes.data()+n*index.stride,index.stride); Check(value==n,"Original FireSmoke index order differs"); }
	Check(instance.kind==NativeBufferKind::Vertex && instance.stride==64 && instance.attributes.size()==4 && draw.instanceCount<=instance.elements,"Original FireSmoke instance layout/range differs");
	for (unsigned n=0;n<4;++n) { const auto& f=instance.attributes[n]; Check(f.id==n+1 && f.offset==n*16 && f.byteSize==16 && f.declaredCount==4 && f.scalarType==NativeScalarType::Float32 && !f.normalized,"Original FireSmoke instance attribute differs"); }
	for (std::size_t n=0;n<std::size_t(draw.instanceCount)*64;n+=4) Check(std::isfinite(Float(draw.instances->bytes,n)),"Original FireSmoke instance is nonfinite");
	for (float value:draw.defines) Check(std::isfinite(value),"Original FireSmoke shader define is nonfinite");
	for (float value:draw.windVelocity) Check(std::isfinite(value),"Original FireSmoke wind is nonfinite");
	for (float value:draw.highlightPass) Check(std::isfinite(value),"Original FireSmoke highlight is nonfinite");
	constexpr std::array<const char*,2> names{"bitmaps/projectiletextures/BARFlame02.tga","bitmaps/projectiletextures/smoke-beh-anim.tga"};
	for (unsigned n=0;n<2;++n) { const auto& image=draw.textures[n]; Check(image && image->sourceName==names[n] && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm && !image->flipV && !image->renderTarget && !image->postprocessVersion,"Original FireSmoke atlas identity/format differs"); Check(image->width && image->height && std::size_t(image->width)<=std::numeric_limits<std::size_t>::max()/image->height/4 && image->bytesPerRow==std::size_t(image->width)*4 && image->bytes.size()==std::size_t(image->width)*image->height*4 && image->mips.empty() && image->generateMipmaps,"Original FireSmoke real atlas storage/mip policy differs"); }
}
void NativeFireSmokeRecorder::Begin()
{
	if (frame) throw std::logic_error("Original FireSmoke world scope is already active");
	frame=std::make_shared<NativeFireSmokeFrame>();
}
void NativeFireSmokeRecorder::Record(NativeFireSmokeDraw draw)
{
	if (!frame) throw std::logic_error("Original FireSmoke draw is outside its world scope");
	ValidateNativeFireSmokeDraw(draw); draw.sequence=NextNativeWorldDrawSequence(); frame->draws.push_back(std::move(draw));
}
void NativeFireSmokeRecorder::SetFrameResources(const NativeFireSmokeFrame& resources)
{
	if (!frame) throw std::logic_error("Original FireSmoke frame resources have no world scope");
	for (const auto* matrix:{&resources.cameraViewProjection,&resources.cameraViewInverse}) for (float value:*matrix) Check(std::isfinite(value),"Original FireSmoke camera is nonfinite");
	for (float value:resources.timeInfo) Check(std::isfinite(value),"Original FireSmoke time is nonfinite");
	frame->cameraViewProjection=resources.cameraViewProjection; frame->cameraViewInverse=resources.cameraViewInverse; frame->timeInfo=resources.timeInfo;
}
std::shared_ptr<const NativeFireSmokeFrame> NativeFireSmokeRecorder::End()
{
	if (!frame) throw std::logic_error("Original FireSmoke world scope is inactive");
	return std::exchange(frame,{});
}
void NativeFireSmokeRecorder::Abort() noexcept { frame.reset(); }
void SetNativeFireSmokeConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeFireSmokeConsumerAvailable() noexcept { return available.load(); }
}
