/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeLightningEffectRecorder.h"
#include "NativeLightningEffectPrograms.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <atomic>
#include <cmath>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool value, const char* message) { if (!value) throw std::invalid_argument(message); }
void Snapshot(const NativeBufferSnapshotRef& buffer)
{
	Check(buffer && buffer->object && buffer->revision && buffer->bytes.size() == CheckedNativeBufferByteSize(buffer->layout), "Lightning buffer identity or extent differs");
}
float Float(const NativeBufferSnapshot& buffer, std::size_t offset)
{
	float value; std::memcpy(&value, buffer.bytes.data() + offset, sizeof(value)); return value;
}
void Attribute(const NativeBufferAttribute& attribute, unsigned id, std::size_t offset)
{
	Check(attribute.id == id && attribute.offset == offset && attribute.scalarType == NativeScalarType::Float32 && attribute.valueShape == NativeValueShape::Scalar && attribute.declaredCount == 4 && attribute.byteSize == 16 && !attribute.normalized, "Lightning original vec4 attribute differs");
}
}
void SetNativeLightningConsumerAvailable(bool value) noexcept { available.store(value); }
bool IsNativeLightningConsumerAvailable() noexcept { return available.load(); }
void ValidateNativeLightningEffectDraw(const NativeLightningEffectDraw& draw)
{
	const auto kind = static_cast<std::size_t>(draw.program);
	Check(kind < NativeLightningPrograms.size(), "Unknown original lightning program");

	const auto& blend = draw.surface.blend;
	Check(blend.enabled && blend.sourceColor == ScreenBlendFactor::One && blend.destinationColor == ScreenBlendFactor::One && blend.sourceAlpha == ScreenBlendFactor::One && blend.destinationAlpha == ScreenBlendFactor::One, "Lightning original additive blend differs");
	Check(draw.world.depthTest && !draw.world.depthWrite && !draw.world.cullEnabled, "Lightning original enabled-depth/no-write/no-cull pass differs");
	Check(draw.world.frontPolygonMode == NativePolygonMode::Fill && draw.world.backPolygonMode == NativePolygonMode::Fill, "Lightning inherited non-fill polygon mode is not implemented");
	Check(draw.world.depthCompare <= ScreenAlphaCompare::Always && draw.surface.alphaTest.compare <= ScreenAlphaCompare::Always && std::isfinite(draw.surface.alphaTest.reference), "Lightning inherited comparison or alpha reference differs");
	Check(std::isfinite(draw.world.offsetFactor) && std::isfinite(draw.world.offsetUnits), "Lightning inherited polygon offset is nonfinite");
	for (const auto* face: {&draw.world.frontStencil, &draw.world.backStencil}) Check(face->compare <= ScreenAlphaCompare::Always && face->fail <= NativeWorldStencilOp::DecrementWrap && face->depthFail <= NativeWorldStencilOp::DecrementWrap && face->pass <= NativeWorldStencilOp::DecrementWrap, "Lightning inherited stencil state differs");
	Snapshot(draw.vertices); Snapshot(draw.instances);
	const auto& mesh = draw.vertices->layout; const auto& instances = draw.instances->layout;
	Check(draw.indexed && mesh.kind==NativeBufferKind::Vertex && mesh.elements>=4 && mesh.elements%4==0 && mesh.stride==16 && mesh.attributes.size()==1,"Original31657 Lightning indexed strip schema differs");
	Attribute(mesh.attributes.front(),0,0);
	const unsigned quads=mesh.elements/4;
	constexpr std::array<float,8> corners{-1,-1,-1,1,1,1,1,-1};
	for(unsigned n=0;n<mesh.elements;++n){
		const std::array<float,4> expected{corners[(n%4)*2],corners[(n%4)*2+1],float(n/4),0};
		for(unsigned c=0;c<4;++c)Check(Float(*draw.vertices,size_t(n)*16+c*4)==expected[c],"Original31657 Lightning corner/segment stream differs");
	}
	Snapshot(draw.indices);const auto& layout=draw.indices->layout;
	Check(layout.kind==NativeBufferKind::Index && std::uint64_t(layout.elements)==std::uint64_t(quads)*6 && layout.attributes.size()==1 && ((layout.stride==2&&layout.attributes.front().scalarType==NativeScalarType::UInt16)||(layout.stride==4&&layout.attributes.front().scalarType==NativeScalarType::UInt32)),"Original31657 Lightning strip index schema differs");
	const auto& a=layout.attributes.front();Check(a.id==0&&a.offset==0&&a.byteSize==layout.stride&&a.declaredCount==1&&a.valueShape==NativeValueShape::Scalar&&!a.normalized,"Original31657 Lightning index attribute differs");
	constexpr std::array<unsigned,6> order{0,1,2,2,3,0};
	for(unsigned n=0;n<layout.elements;++n){std::uint32_t value=0;std::memcpy(&value,draw.indices->bytes.data()+size_t(n)*layout.stride,layout.stride);Check(value==(n/6)*4+order[n%6],"Original31657 Lightning strip index order differs");}
	const bool body=draw.program==NativeLightningProgram::CannonBolt||draw.program==NativeLightningProgram::EnvironmentBolt;
	Check(draw.indexCount==(body?layout.elements:6u),"Original31657 Lightning body/first-quad draw range differs");
	Check(instances.kind == NativeBufferKind::Vertex && instances.stride == 96 && instances.attributes.size() == 6 && draw.firstInstance <= instances.elements && draw.instanceCount <= instances.elements - draw.firstInstance, "Lightning original instance range or stride differs");
	for (unsigned i = 0; i < 6; ++i) Attribute(instances.attributes[i], i + 1, i * 16);
	for (std::size_t i = std::size_t(draw.firstInstance) * 96; i < (std::size_t(draw.firstInstance) + draw.instanceCount) * 96; i += 4) Check(std::isfinite(Float(*draw.instances, i)), "Lightning selected instance is nonfinite");
	std::size_t expected = 0;
	for (std::size_t i = 0; i < NativeLightningParameterNames.size(); ++i) {
		if (!NativeLightningRequiredParameters[kind][i]) continue;
		++expected; const auto found = draw.parameters.find(std::string(NativeLightningParameterNames[i]));
		Check(found != draw.parameters.end() && std::isfinite(found->second), "Lightning shader configuration is missing or nonfinite");
	}
	Check(draw.parameters.size() == expected, "Lightning shader configuration contains unknown parameters");
	if (!NativeLightningPrograms[kind].texture.empty()) {
		Check(draw.texture && draw.texture->sourceName == NativeLightningPrograms[kind].texture && draw.texture->width == 512 && draw.texture->height == 512 && draw.texture->rgba.size() == 512 * 512 * 4, "Lightning impact requires the real original flare2 image");
	} else Check(!draw.texture, "Untextured original lightning program has an unexpected image");
}
void NativeLightningEffectRecorder::Begin()
{
	if (frame) throw std::logic_error("Lightning world frame is already active");
	frame = std::make_shared<NativeLightningEffectFrame>();
}
void NativeLightningEffectRecorder::Record(NativeLightningEffectDraw draw)
{
	if (!frame) throw std::logic_error("Lightning draw outside actual world scope");
	ValidateNativeLightningEffectDraw(draw);
	draw.sequence = NextNativeWorldDrawSequence();
	frame->draws.emplace_back(std::move(draw));
}
void NativeLightningEffectRecorder::SetFrameResources(const NativeLightningEffectFrame& resources)
{
	if (!frame) throw std::logic_error("Lightning resources outside actual world scope");
	for (const auto* matrix: {&resources.cameraViewProjection, &resources.cameraViewInverse}) for (float value: *matrix) Check(std::isfinite(value), "Lightning camera is nonfinite");
	for (float value: resources.timeInfo) Check(std::isfinite(value), "Lightning time is nonfinite");
	frame->cameraViewProjection = resources.cameraViewProjection; frame->cameraViewInverse = resources.cameraViewInverse; frame->timeInfo = resources.timeInfo;
}
std::shared_ptr<const NativeLightningEffectFrame> NativeLightningEffectRecorder::End()
{
	if (!frame) throw std::logic_error("Lightning world frame is inactive");
	return std::exchange(frame, {});
}
void NativeLightningEffectRecorder::Abort() noexcept { frame.reset(); }
}
