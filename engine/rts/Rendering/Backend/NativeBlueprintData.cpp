/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBlueprintData.h"
#include "NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Range> void Finite(const Range& values, const char* message)
{
	for (auto value: values) Check(std::isfinite(value),message);
}
void Stage(NativeWorldDrawStage value)
{
	Check(value == NativeWorldDrawStage::PreUnit || value == NativeWorldDrawStage::World,"Unknown original Blueprint draw stage");
}
void View(const NativeBlueprintView& value)
{
	Finite(value.viewProjection,"Nonfinite original Blueprint view projection");
	Finite(value.cameraPosition,"Nonfinite original Blueprint camera position");
	Finite(value.timeInfo,"Nonfinite original Blueprint frame timing");
	Check(value.clipDepth == ClipDepthConvention::MinusOneToOne || value.clipDepth == ClipDepthConvention::ZeroToOne,
		"Original Blueprint camera depth convention unavailable");
}
void State(const ScreenDrawState& surface, const NativeWorldDrawState& world)
{
	Check(surface.blend.sourceColor <= ScreenBlendFactor::SourceAlphaSaturate && surface.blend.sourceAlpha <= ScreenBlendFactor::SourceAlphaSaturate &&
		surface.blend.destinationColor <= ScreenBlendFactor::SourceAlphaSaturate && surface.blend.destinationAlpha <= ScreenBlendFactor::SourceAlphaSaturate &&
		surface.blend.colorEquation <= ScreenBlendEquation::Maximum && surface.blend.alphaEquation <= ScreenBlendEquation::Maximum,
		"Unknown original Blueprint blend state");
	Check(surface.alphaTest.compare <= ScreenAlphaCompare::Always && std::isfinite(surface.alphaTest.reference),"Invalid original Blueprint alpha test");
	Check(world.depthCompare <= ScreenAlphaCompare::Always && world.cull <= NativeWorldCull::FrontAndBack &&
		world.frontPolygonMode <= NativePolygonMode::Fill && world.backPolygonMode <= NativePolygonMode::Fill,
		"Invalid original Blueprint depth/cull/polygon state");
	Check(std::isfinite(world.lineWidth) && world.lineWidth > 0 && std::isfinite(world.offsetFactor) && std::isfinite(world.offsetUnits),
		"Invalid original Blueprint raster parameter");
	for (const auto* face: {&world.frontStencil,&world.backStencil}) {
		Check(face->compare <= ScreenAlphaCompare::Always && face->fail <= NativeWorldStencilOp::DecrementWrap &&
			face->depthFail <= NativeWorldStencilOp::DecrementWrap && face->pass <= NativeWorldStencilOp::DecrementWrap,
			"Invalid original Blueprint stencil state");
	}
}
void Layout(const NativeBufferSnapshotRef& buffer, std::size_t stride,
	std::span<const std::uint32_t> ids, std::span<const std::uint32_t> counts, std::span<const NativeScalarType> types)
{
	Check(buffer && buffer->object && buffer->revision,"Original Blueprint requires a real immutable buffer generation");
	const auto& layout = buffer->layout;
	Check(layout.kind == NativeBufferKind::Vertex && layout.stride == stride && layout.attributes.size() == ids.size(),
		"Original Blueprint buffer layout differs");
	Check(buffer->bytes.size() == CheckedNativeBufferByteSize(layout),"Original Blueprint snapshot storage differs");
	std::size_t offset = 0;
	for (std::size_t i = 0; i < ids.size(); ++i) {
		const auto& attribute = layout.attributes[i];
		Check(attribute.id == ids[i] && attribute.declaredCount == counts[i] && attribute.scalarType == types[i] &&
			attribute.valueShape == NativeValueShape::Scalar && !attribute.normalized && attribute.offset == offset && attribute.byteSize == counts[i]*4,
			"Original Blueprint attribute type/location/address differs");
		offset += counts[i]*4;
	}
}
template<class T> T Scalar(const NativeBufferSnapshot& buffer, std::size_t row, std::size_t byteOffset)
{
	T result{};
	std::memcpy(&result,buffer.bytes.data()+row*buffer.layout.stride+byteOffset,sizeof(result));
	return result;
}
void Texture(const std::shared_ptr<const WorldTexture>& texture)
{
	Check(texture && !texture->sourcePath.empty() && !texture->mips.empty(),"Original Blueprint model texture binding is missing");
	Check(texture->format <= WorldTextureFormat::BGRA8Unorm,"Unknown original Blueprint model texture format");
	Check(texture->mips.front().width && texture->mips.front().height,"Original Blueprint model texture has no extent");
	for (const auto& mip: texture->mips) Check(mip.width && mip.height && mip.bytesPerRow && !mip.bytes.empty(),"Original Blueprint model texture level is empty");
}
float Mix(float a, float b, float amount) { return a*(1.f-amount)+b*amount; }
}

void ValidateNativeBlueprintOutline(const NativeBlueprintOutlineDraw& draw)
{
	Stage(draw.stage); View(draw.view); State(draw.surface,draw.world);
	static constexpr std::array<std::uint32_t,1> vertexIds{0}, vertexCounts{2};
	static constexpr std::array<NativeScalarType,1> vertexTypes{NativeScalarType::Float32};
	static constexpr std::array<std::uint32_t,3> instanceIds{1,2,3}, instanceCounts{3,2,4};
	static constexpr std::array<NativeScalarType,3> instanceTypes{NativeScalarType::Float32,NativeScalarType::Float32,NativeScalarType::Float32};
	Layout(draw.vertices,8,vertexIds,vertexCounts,vertexTypes);
	Layout(draw.instances,36,instanceIds,instanceCounts,instanceTypes);
	Check(draw.firstVertex == 0 && draw.vertexCount == 5 && draw.vertices->layout.elements == 5,"Original Blueprint five-point line strip differs");
	static constexpr std::array<float,10> original{-0.5f,-0.5f,-0.5f,0.5f,0.5f,0.5f,0.5f,-0.5f,-0.5f,-0.5f};
	for (unsigned i = 0; i < original.size(); ++i) Check(Scalar<float>(*draw.vertices,i/2,(i%2)*4) == original[i],"Original Blueprint outline topology/winding differs");
	Check(draw.firstInstance <= draw.instances->layout.elements && draw.instanceCount <= draw.instances->layout.elements-draw.firstInstance,
		"Original Blueprint instance range exceeds capacity");
	for (std::size_t row = draw.firstInstance; row < std::size_t(draw.firstInstance)+draw.instanceCount; ++row)
		for (unsigned field = 0; field < 9; ++field) Check(std::isfinite(Scalar<float>(*draw.instances,row,field*4)),"Nonfinite selected original Blueprint instance");
	Check(draw.heights && draw.heights->width >= 3 && draw.heights->height >= 3 &&
		draw.heights->corners.size() == std::size_t(draw.heights->width)*draw.heights->height,"Original Blueprint height texture storage differs");
	Finite(draw.mapWorldSize,"Nonfinite original Blueprint map extent");
	Check(draw.mapWorldSize[0] == float(draw.heights->width-1)*8 && draw.mapWorldSize[1] == float(draw.heights->height-1)*8,
		"Original Blueprint heightmap dimensions do not match mapSize");
	Check(draw.world.depthTest && draw.world.depthCompare == ScreenAlphaCompare::Always && !draw.world.depthWrite && draw.world.lineWidth == 2,
		"Original Blueprint outline depth/width contract differs");
}

void ValidateNativeBlueprintModel(const NativeBlueprintModelDraw& draw)
{
	Stage(draw.stage); View(draw.view); State(draw.surface,draw.world);
	const bool feature=draw.program==NativeBlueprintProgram::FeatureShape;
	Check(draw.program == NativeBlueprintProgram::Unit || draw.program == NativeBlueprintProgram::UnitShape || feature,"Unknown original Blueprint model program");
	Check((draw.program == NativeBlueprintProgram::UnitShape && draw.stage == NativeWorldDrawStage::PreUnit) ||
		((draw.program == NativeBlueprintProgram::Unit || feature) && draw.stage == NativeWorldDrawStage::World),"Original Blueprint model call-in stage differs");
	static constexpr std::array<std::uint32_t,4> ids{6,7,8,9}, counts{4,4,2,4};
	static constexpr std::array<NativeScalarType,4> types{NativeScalarType::Float32,NativeScalarType::Float32,NativeScalarType::UInt32,NativeScalarType::UInt32};
	if(feature){
  static constexpr std::array<uint32_t,5> featureIds{6,7,8,9,10},featureCounts{4,4,4,4,4};
  static constexpr std::array<NativeScalarType,5> featureTypes{NativeScalarType::Float32,NativeScalarType::Float32,NativeScalarType::Float32,NativeScalarType::Float32,NativeScalarType::UInt32};
  Layout(draw.instances,80,featureIds,featureCounts,featureTypes);
 }else Layout(draw.instances,56,ids,counts,types);
	Check(std::isfinite(draw.iconDistance),"Nonfinite original Blueprint icon distance");
	if (draw.modelTextures) {
		Check(!draw.textures[0]&&!draw.textures[1],"Original Blueprint has competing model texture owners");
		for (unsigned slot=0;slot<2;++slot) Texture(NativeS3OTexturePixels(*draw.modelTextures,slot));
	} else {
		for (const auto& image: draw.textures) Texture(image);
	}
	Check(draw.world.depthTest && draw.world.depthCompare == ScreenAlphaCompare::LessEqual && draw.world.depthWrite == !feature &&
		draw.world.cullEnabled && draw.world.cull == NativeWorldCull::Back && draw.world.polygonOffset && (feature || draw.world.stencilTest),
		"Original Blueprint model self-occlusion/cull/stencil state differs");
	Check(draw.world.frontPolygonMode == NativePolygonMode::Fill && draw.world.backPolygonMode == NativePolygonMode::Fill,
		"Original Blueprint ghost requires the original filled-model polygon state");
	if(!feature)for (const auto* face: {&draw.world.frontStencil,&draw.world.backStencil}) Check(face->compare == ScreenAlphaCompare::Always &&
		face->reference == 0x40 && face->readMask == 0x40 && face->writeMask == 0x40 && face->fail == NativeWorldStencilOp::Keep &&
		face->depthFail == NativeWorldStencilOp::Keep && face->pass == NativeWorldStencilOp::Replace,"Original Blueprint ghost stencil ownership differs");
	const float offset = draw.program == NativeBlueprintProgram::UnitShape ? 1.f : -2.f;
	Check(draw.world.offsetFactor == offset && draw.world.offsetUnits == offset,"Original Blueprint ghost depth offset differs");
	Check(draw.submissions.size() <= draw.instances->layout.elements,"Original Blueprint model submission exceeds instance capacity");
	for (std::size_t n = 0; n < draw.submissions.size(); ++n) {
		const auto& submission = draw.submissions[n];
		Check(submission.kind == NativeBlueprintObjectKind::Unit || submission.kind == NativeBlueprintObjectKind::UnitDefinition || submission.kind == NativeBlueprintObjectKind::FeatureDefinition,"Unknown original Blueprint model object kind");
		Check(submission.objectId >= 0 && submission.instanceIndex == n && submission.model,"Original Blueprint model submission ordering differs");
		Check(submission.kind == (feature?NativeBlueprintObjectKind::FeatureDefinition:(draw.program == NativeBlueprintProgram::UnitShape ? NativeBlueprintObjectKind::UnitDefinition : NativeBlueprintObjectKind::Unit)),
			"Original Blueprint model submission/program mismatch");
		for (unsigned scalar = 0; scalar < (feature?16u:8u); ++scalar) Check(std::isfinite(Scalar<float>(*draw.instances,n,scalar*4)),"Nonfinite selected original Blueprint model parameter");
		Check(Scalar<std::uint32_t>(*draw.instances,n,feature?64:40) == submission.sourceTransformBase,"Original Blueprint transform capture uses the wrong instance base");
		Check(submission.transforms.size() > 0 && submission.transforms.size() <= 256,"Original Blueprint low-eight-bit transform range differs");
		Finite(submission.selectedTeamColor,"Nonfinite original Blueprint team color");
		for (const auto& transform: submission.transforms) {
			Finite(transform.quaternion,"Nonfinite original Blueprint quaternion"); Finite(transform.translationScale,"Nonfinite original Blueprint translation/scale");
		}
		for (const auto& piece: submission.model->pieces) {
			Check(piece.indices.size()%3 == 0,"Original Blueprint model is not triangulated");
			for (auto index: piece.indices) Check(index < piece.vertices.size(),"Original Blueprint model index exceeds original geometry");
			for (const auto& vertex: piece.vertices) {
				Check((vertex.boneIds[0]&0xffu) < submission.transforms.size(),"Original Blueprint vertex addresses an uncaptured source transform");
				Finite(vertex.position,"Nonfinite original Blueprint model vertex"); Finite(vertex.uv[0],"Nonfinite original Blueprint model UV");
			}
		}
	}
}

void ValidateNativeWorldStencilClear(const NativeWorldStencilClear& clear) { Stage(clear.stage); }

std::array<float,2> NativeBlueprintHeightUV(std::array<float,2> position, std::array<float,2> mapWorldSize)
{
	Finite(position,"Nonfinite Blueprint height lookup"); Finite(mapWorldSize,"Nonfinite Blueprint height map extent");
	for (unsigned i = 0; i < 2; ++i) {
		Check(mapWorldSize[i] >= 16,"Blueprint height map extent is too small");
		const float inverse = 1.f/mapWorldSize[i];
		position[i] += -8.f*(position[i]*inverse)+4.f;
		position[i] = std::clamp(position[i],8.f,mapWorldSize[i]-8.f)*inverse;
	}
	return position;
}

float NativeBlueprintHeightLOD0(const WorldTerrainHeights& heights, std::array<float,2> uv)
{
	Check(heights.width && heights.height && heights.corners.size() == std::size_t(heights.width)*heights.height,"Missing actual Blueprint height storage");
	Finite(uv,"Nonfinite Blueprint height coordinate");
	// The original texture has MAG=LINEAR, MIN=NEAREST and an explicit LOD 0.
	// Keep this sampler behavior visible for the paired GPU oracle gate.
	const float x = std::clamp(uv[0],0.f,1.f)*heights.width-0.5f, z = std::clamp(uv[1],0.f,1.f)*heights.height-0.5f;
	const int x0 = int(std::floor(x)), z0 = int(std::floor(z));
	const auto sample = [&](int sx, int sz) {
		return heights.corners[std::size_t(std::clamp(sz,0,int(heights.height)-1))*heights.width+std::clamp(sx,0,int(heights.width)-1)];
	};
	return Mix(Mix(sample(x0,z0),sample(x0+1,z0),x-float(x0)),Mix(sample(x0,z0+1),sample(x0+1,z0+1),x-float(x0)),z-float(z0));
}

std::array<float,3> NativeBlueprintApplyTransform(const NativeBlueprintTransform& transform, std::array<float,3> position)
{
	const auto& q = transform.quaternion; const auto& ts = transform.translationScale;
	for (auto& value: position) value *= ts[3];
	const float dotQV = q[0]*position[0]+q[1]*position[1]+q[2]*position[2], dotQQ = q[0]*q[0]+q[1]*q[1]+q[2]*q[2];
	const std::array<float,3> cross{q[1]*position[2]-q[2]*position[1],q[2]*position[0]-q[0]*position[2],q[0]*position[1]-q[1]*position[0]};
	for (unsigned i = 0; i < 3; ++i) position[i] = 2.f*dotQV*q[i]+(q[3]*q[3]-dotQQ)*position[i]+2.f*q[3]*cross[i]+ts[i];
	return position;
}

std::array<float,3> NativeBlueprintRotateAndPlace(std::array<float,3> local, std::array<float,4> worldPositionRotation)
{
	const float sine = std::sin(worldPositionRotation[3]), cosine = std::cos(worldPositionRotation[3]);
	return {cosine*local[0]+sine*local[2]+worldPositionRotation[0],local[1]+worldPositionRotation[1],-sine*local[0]+cosine*local[2]+worldPositionRotation[2]};
}

std::array<float,4> NativeBlueprintModelColor(std::array<float,4> base, std::array<float,4> extra,
	std::array<float,4> team, std::array<float,4> parameters, float worldY, const std::array<float,4>& timeInfo)
{
	for (auto& value: base) value += value*extra[0];
	base[3] *= extra[3];
	for (unsigned i = 0; i < 3; ++i) base[i] = Mix(base[i],team[i],base[3]);
	base[3] *= team[3];
	for (unsigned i = 0; i < 3; ++i) base[i] = Mix(base[i],team[i],parameters[2]);
	if (parameters[3] > 0) {
		const float phase = worldY*0.03f+(timeInfo[0]+timeInfo[3])*0.05f;
		const float amount = parameters[3]*(phase-std::floor(phase));
		for (unsigned i = 0; i < 3; ++i) base[i] = Mix(base[i],1.f,amount);
	}
	// The original intentionally publishes instance alpha, not texture alpha.
	base[3] = team[3];
	return base;
}

std::uint8_t NativeBlueprintClearStencil(std::uint8_t previous, const NativeWorldStencilClear& clear) noexcept
{
	return std::uint8_t((previous&~clear.writeMask)|(clear.value&clear.writeMask));
}

void NativeBlueprintRecorder::Begin()
{
	Check(!frame,"Original Blueprint frame is already active"); frame = std::make_shared<NativeBlueprintFrame>();
}
void NativeBlueprintRecorder::Record(NativeBlueprintOutlineDraw draw)
{
	Check(bool(frame),"Original Blueprint outline outside frame"); ValidateNativeBlueprintOutline(draw);
	if (!draw.instanceCount) return;
	draw.sequence = NextNativeWorldDrawSequence(); frame->outlines.push_back(std::move(draw));
}
void NativeBlueprintRecorder::Record(NativeBlueprintModelDraw draw)
{
	Check(bool(frame),"Original Blueprint model outside frame"); ValidateNativeBlueprintModel(draw);
	if (draw.submissions.empty()) return;
	draw.sequence = NextNativeWorldDrawSequence(); frame->models.push_back(std::move(draw));
}
void NativeBlueprintRecorder::Record(NativeWorldStencilClear clear)
{
	Check(bool(frame),"Original Blueprint stencil clear outside frame"); ValidateNativeWorldStencilClear(clear);
	clear.sequence = NextNativeWorldDrawSequence(); frame->clears.push_back(std::move(clear));
}
std::shared_ptr<const NativeBlueprintFrame> NativeBlueprintRecorder::End()
{
	Check(bool(frame),"Original Blueprint End without frame"); return std::exchange(frame,{});
}
void NativeBlueprintRecorder::Abort() noexcept { frame.reset(); }
void SetNativeBlueprintConsumerAvailable(bool value) noexcept { available.store(value,std::memory_order_release); }
bool IsNativeBlueprintConsumerAvailable() noexcept { return available.load(std::memory_order_acquire); }

} // namespace rendering
