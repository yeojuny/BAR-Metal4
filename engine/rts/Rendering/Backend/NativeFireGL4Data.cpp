/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeFireGL4Data.h"
#include "NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool valid,const char* message) { if (!valid) throw std::invalid_argument(message); }
void Snapshot(const NativeBufferSnapshotRef& source)
{
	Check(source && source->object && source->revision,"Original FireGL4 immutable buffer is missing");
	Check(source->bytes.size()==CheckedNativeBufferByteSize(source->layout),"Original FireGL4 buffer extent differs");
}
void Attribute(const NativeBufferAttribute& a,unsigned id,unsigned offset)
{
	Check(a.id==id && a.offset==offset && a.byteSize==16 && a.declaredCount==4 &&
		a.scalarType==NativeScalarType::Float32 && a.valueShape==NativeValueShape::Scalar && !a.normalized,
		"Original FireGL4 float4 attribute differs");
}
float Float(const std::vector<std::uint8_t>& bytes,std::size_t offset)
{
	float value;std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
}
}
NativeFireGL4UniformValues ReadNativeFireGL4Uniforms(const NativeFireGL4Uniforms& source)
{
 // Same original published cameraViewProj/cameraViewInv/timeInfo/renderCaps.
 return ReadNativeFlamethrowerUniforms(source);
}
void ValidateNativeFireGL4Draw(const NativeFireGL4Draw& draw)
{
	Check(draw.stage==NativeWorldDrawStage::World,"Original FireGL4 draw is outside its original DrawWorld");
	Snapshot(draw.vertices);Snapshot(draw.instances);
	const auto& vertex=draw.vertices->layout;
	const auto& instance=draw.instances->layout;
	Check(vertex.kind==NativeBufferKind::Vertex && vertex.elements==6 && vertex.stride==16 && vertex.attributes.size()==1,
		"Original FireGL4 rectangle layout differs");
	Attribute(vertex.attributes.front(),0,0);
	constexpr std::array<float,24> rectangle{-1,-1,0,0,-1,1,0,1,1,1,1,1,1,1,1,1,1,-1,1,0,-1,-1,0,0};
	for (std::size_t n=0;n<rectangle.size();++n)
		Check(Float(draw.vertices->bytes,n*4)==rectangle[n],"Original FireGL4 rectangle vertex/UV order differs");
	Check(draw.firstVertex==0 && draw.vertexCount==6 && draw.firstInstance==0,
		"Original FireGL4 instanced DrawArrays invocation differs");
	Check(instance.kind==NativeBufferKind::Vertex && instance.stride==64 && instance.attributes.size()==4 &&
		draw.instanceCount<=instance.elements,"Original FireGL4 64-byte row/range differs");
	for (unsigned n=0;n<4;++n) Attribute(instance.attributes[n],n+1,n*16);
	for (std::size_t n=0;n<std::size_t(draw.instanceCount)*64;n+=4)
		Check(std::isfinite(Float(draw.instances->bytes,n)),"Original FireGL4 instance is nonfinite");
	for (float value:draw.defines) Check(std::isfinite(value),"Original FireGL4 define is nonfinite");
	for (float value:draw.windVelocity) Check(std::isfinite(value),"Original FireGL4 wind is nonfinite");
	constexpr std::array<const char*,2> names{"bitmaps/projectiletextures/BARFlame02.tga","bitmaps/projectiletextures/smoke-beh-anim.tga"};
	for (unsigned n=0;n<2;++n) {
		const auto& image=draw.textures[n];
		Check(image && image->sourceName==names[n] && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm &&
			!image->flipV && !image->renderTarget && !image->postprocessVersion,"Original FireGL4 named atlas differs");
		Check(image->width && image->height && std::size_t(image->width)<=std::numeric_limits<std::size_t>::max()/4/image->height &&
			image->bytesPerRow==std::size_t(image->width)*4 &&
			image->bytes.size()==std::size_t(image->width)*image->height*4 && image->mips.empty() && image->generateMipmaps,
			"Original FireGL4 actual atlas/mip storage differs");
		ValidateScreenSampler(draw.samplers[n]);
	}
}
void NativeFireGL4Recorder::Begin(NativeFireGL4Uniforms uniforms)
{
	if (frame) throw std::logic_error("Original FireGL4 frame is already active");
	(void)ReadNativeFireGL4Uniforms(uniforms);
	auto next=std::make_shared<NativeFireGL4Frame>();next->uniforms=std::move(uniforms);frame=std::move(next);
}
void NativeFireGL4Recorder::Record(NativeFireGL4Draw draw)
{
	if (!frame) throw std::logic_error("Original FireGL4 draw has no active frame");
	ValidateNativeFireGL4Draw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeFireGL4Frame> NativeFireGL4Recorder::End()
{
	if (!frame) throw std::logic_error("Original FireGL4 frame is inactive");
	return std::exchange(frame,{});
}
void NativeFireGL4Recorder::Abort() noexcept { frame.reset(); }
void SetNativeFireGL4ConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeFireGL4ConsumerAvailable() noexcept { return available.load(); }
}
