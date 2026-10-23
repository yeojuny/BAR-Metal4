/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeFlamethrowerData.h"
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
	Check(source && source->object && source->revision,"Original Flamethrower immutable buffer is missing");
	Check(source->bytes.size()==CheckedNativeBufferByteSize(source->layout),"Original Flamethrower buffer extent differs");
}
void Attribute(const NativeBufferAttribute& a,unsigned id,unsigned offset)
{
	Check(a.id==id && a.offset==offset && a.byteSize==16 && a.declaredCount==4 &&
		a.scalarType==NativeScalarType::Float32 && a.valueShape==NativeValueShape::Scalar && !a.normalized,
		"Original Flamethrower float4 attribute differs");
}
float Float(const std::vector<std::uint8_t>& bytes,std::size_t offset)
{
	float value;std::memcpy(&value,bytes.data()+offset,sizeof(value));return value;
}
}
NativeFlamethrowerUniformValues ReadNativeFlamethrowerUniforms(const NativeFlamethrowerUniforms& source)
{
	Check(bool(source.original),"Original Flamethrower UBO publication is missing");
	ValidateNativeCUSUniformPublication(*source.original);
	const auto& matrices=*source.original->matrices;
	const auto& params=*source.original->params;
	Check(matrices.sourceDrawFrame<=source.sourceDrawFrame && params.sourceDrawFrame<=source.sourceDrawFrame,
		"Original Flamethrower UBO publication is from a future draw");
	NativeFlamethrowerUniformValues value;
	std::memcpy(value.cameraViewProjection.data(),matrices.bytes.data()+5*64,64);
	std::memcpy(value.cameraViewInverse.data(),matrices.bytes.data()+7*64,64);
	std::memcpy(value.timeInfo.data(),params.bytes.data()+16,16);
	std::uint32_t caps;std::memcpy(&caps,params.bytes.data()+12,4);
	value.clipDepth=(caps&1u)?ClipDepthConvention::ZeroToOne:ClipDepthConvention::MinusOneToOne;
	for (const auto* matrix:{&value.cameraViewProjection,&value.cameraViewInverse})
		for (float f:*matrix) Check(std::isfinite(f),"Original Flamethrower matrix is nonfinite");
	for (float f:value.timeInfo) Check(std::isfinite(f),"Original Flamethrower time is nonfinite");
	return value;
}
void ValidateNativeFlamethrowerDraw(const NativeFlamethrowerDraw& draw)
{
	Check(draw.stage==NativeWorldDrawStage::World,"Original Flamethrower draw is outside its original DrawWorld");
	Snapshot(draw.vertices);Snapshot(draw.indices);Snapshot(draw.instances);
	const auto& vertex=draw.vertices->layout;
	const auto& instance=draw.instances->layout;
	Check(vertex.kind==NativeBufferKind::Vertex && vertex.elements==6 && vertex.stride==16 && vertex.attributes.size()==1,
		"Original Flamethrower rectangle layout differs");
	Attribute(vertex.attributes.front(),0,0);
	constexpr std::array<float,24> rectangle{-1,-1,0,0,-1,1,0,1,1,1,1,1,1,1,1,1,1,-1,1,0,-1,-1,0,0};
	for (std::size_t n=0;n<rectangle.size();++n)
		Check(Float(draw.vertices->bytes,n*4)==rectangle[n],"Original Flamethrower rectangle vertex/UV order differs");
	Check(draw.firstIndex==0 && draw.indexCount==6 && draw.firstInstance==0,
		"Original Flamethrower instanced DrawElements invocation differs");
	const auto& index=draw.indices->layout;
	// makeRectIndexVBO calls Define(6) without a type: the real LuaVBOImpl
	// default is GL_UNSIGNED_SHORT. Snapshot() already validates scalar layout.
	Check(index.kind==NativeBufferKind::Index && index.elements==6 && index.stride==2 &&
		index.attributes.front().id==0 && index.attributes.front().scalarType==NativeScalarType::UInt16,
		"Original Flamethrower unsigned-short rectangle index layout differs");
	for (std::uint16_t n=0;n<6;++n) {
		std::uint16_t actual;std::memcpy(&actual,draw.indices->bytes.data()+std::size_t(n)*2,2);
		Check(actual==n,"Original Flamethrower rectangle index order differs");
	}
	Check(instance.kind==NativeBufferKind::Vertex && instance.stride==64 && instance.attributes.size()==4 &&
		draw.instanceCount<=instance.elements,"Original Flamethrower 64-byte row/range differs");
	for (unsigned n=0;n<4;++n) Attribute(instance.attributes[n],n+1,n*16);
	for (std::size_t n=0;n<std::size_t(draw.instanceCount)*64;n+=4)
		Check(std::isfinite(Float(draw.instances->bytes,n)),"Original Flamethrower instance is nonfinite");
	for (float value:draw.defines) Check(std::isfinite(value),"Original Flamethrower define is nonfinite");
	for (float value:draw.windVelocity) Check(std::isfinite(value),"Original Flamethrower wind is nonfinite");
	constexpr std::array<const char*,2> names{"bitmaps/projectiletextures/BARFlame02.tga","bitmaps/projectiletextures/smoke-beh-anim.tga"};
	for (unsigned n=0;n<2;++n) {
		const auto& image=draw.textures[n];
		Check(image && image->sourceName==names[n] && !image->incomplete && image->format==ScreenTextureFormat::RGBA8Unorm &&
			!image->flipV && !image->renderTarget && !image->postprocessVersion,"Original Flamethrower named atlas differs");
		Check(image->width && image->height && std::size_t(image->width)<=std::numeric_limits<std::size_t>::max()/4/image->height &&
			image->bytesPerRow==std::size_t(image->width)*4 &&
			image->bytes.size()==std::size_t(image->width)*image->height*4 && image->mips.empty() && image->generateMipmaps,
			"Original Flamethrower actual atlas/mip storage differs");
		ValidateScreenSampler(draw.samplers[n]);
	}
}
void NativeFlamethrowerRecorder::Begin(NativeFlamethrowerUniforms uniforms)
{
	if (frame) throw std::logic_error("Original Flamethrower frame is already active");
	(void)ReadNativeFlamethrowerUniforms(uniforms);
	auto next=std::make_shared<NativeFlamethrowerFrame>();next->uniforms=std::move(uniforms);frame=std::move(next);
}
void NativeFlamethrowerRecorder::Record(NativeFlamethrowerDraw draw)
{
	if (!frame) throw std::logic_error("Original Flamethrower draw has no active frame");
	ValidateNativeFlamethrowerDraw(draw);draw.sequence=NextNativeWorldDrawSequence();frame->draws.push_back(std::move(draw));
}
std::shared_ptr<const NativeFlamethrowerFrame> NativeFlamethrowerRecorder::End()
{
	if (!frame) throw std::logic_error("Original Flamethrower frame is inactive");
	return std::exchange(frame,{});
}
void NativeFlamethrowerRecorder::Abort() noexcept { frame.reset(); }
void SetNativeFlamethrowerConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeFlamethrowerConsumerAvailable() noexcept { return available.load(); }
}
