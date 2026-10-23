/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePiPData.h"
#include "NativePiPPrograms.h"
#include "NativeMiniMapData.h"
#include "NativeScreenLine.h"
#include "NativeWorldDrawGeometry.h"
#include "NativeWorldDrawRecorder.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include <algorithm>
#include <stdexcept>

namespace rendering {
namespace {

void Require(bool condition, const char* message)
{
	if (!condition) throw std::invalid_argument(message);
}

std::uint64_t SourceHash(std::string_view source)
{
	std::uint64_t result = 14695981039346656037ULL;
	for (unsigned char byte: source) result = (result ^ byte) * 1099511628211ULL;
	return result;
}

const NativePiPUniformSpec* Uniform(const NativePiPProgramSnapshot& state, int location)
{
	const auto& spec = GetNativePiPProgramSpec(state.program);
	if (location < 0 || std::size_t(location) >= spec.uniforms.size() || !spec.uniforms[location].active) return nullptr;
	return &spec.uniforms[location];
}

void Snapshot(const NativeBufferSnapshotRef& buffer)
{
	Require(buffer && buffer->object && buffer->revision, "Original PiP needs a live immutable VBO generation");
	Require(buffer->layout.kind == NativeBufferKind::Vertex && buffer->bytes.size() == CheckedNativeBufferByteSize(buffer->layout), "Original PiP VBO storage/stride differs");
}

void Attribute(const NativeBufferAttribute& attribute, unsigned id, unsigned components, std::size_t offset)
{
	Require(attribute.id == id && attribute.offset == offset && attribute.scalarType == NativeScalarType::Float32 &&
		attribute.valueShape == NativeValueShape::Scalar && attribute.declaredCount == components &&
		attribute.byteSize == components * sizeof(float) && !attribute.normalized, "Original PiP VBO attribute differs");
}

} // namespace

const NativePiPProgramSpec& GetNativePiPProgramSpec(NativePiPProgram program)
{
	const auto index = static_cast<std::size_t>(program);
	Require(index < std::size(NativePiPPrograms), "Unknown original PiP program");
	return NativePiPPrograms[index];
}

NativePiPProgram IdentifyNativePiPProgram(std::string_view vertex, std::string_view fragment, std::string_view geometry)
{
	Require(geometry.empty(), "PiP direct instancing requires the exact original NoGS source pair");
	const auto vertexHash = SourceHash(vertex), fragmentHash = SourceHash(fragment);
	for (std::size_t i = 0; i < std::size(NativePiPPrograms); ++i) {
		const auto& spec = NativePiPPrograms[i];
		if (spec.vertexHash == vertexHash && spec.fragmentHash == fragmentHash) return static_cast<NativePiPProgram>(i);
	}
	throw std::invalid_argument("PiP shader differs from exact current31657 translation");
}

NativePiPProgramState::NativePiPProgramState(NativePiPProgram program): state{program, {}, {}}
{
	GetNativePiPProgramSpec(program);
	for (unsigned i = 0; i < 4; ++i) state.uniforms.modelViewProjection[i * 5] = 1;
}

NativePiPProgramState::NativePiPProgramState(std::string_view vertex, std::string_view fragment, std::string_view geometry)
	: NativePiPProgramState(IdentifyNativePiPProgram(vertex, fragment, geometry))
{}

int NativePiPProgramState::GetUniformLocation(std::string_view name) const
{
	const auto& uniforms = GetNativePiPProgramSpec(state.program).uniforms;
	const auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](const auto& uniform) { return uniform.name == name && uniform.active; });
	return found == uniforms.end() ? -1 : int(found - uniforms.begin());
}

bool NativePiPProgramState::SetFloats(int location, std::span<const float> values)
{
	if (location == -1) return true;
	const auto* uniform = Uniform(state, location);
	if (!uniform || uniform->sampler || values.size() != uniform->components) return false;
	std::copy(values.begin(), values.end(), state.uniforms.values[uniform->slot].begin());
	return true;
}

bool NativePiPProgramState::SetIntegers(int location, std::span<const int> values)
{
	if (location == -1) return true;
	const auto* uniform = Uniform(state, location);
	if (!uniform || !uniform->sampler || values.size() != 1) return false;
	state.samplerUnits[uniform->slot] = values.front();
	return true;
}

NativePiPProgramSnapshot NativePiPProgramState::Capture() const { return state; }

void ValidateNativePiPDraw(const NativePiPDraw& draw)
{
	const auto& spec = GetNativePiPProgramSpec(draw.program.program);
	Require(draw.viewportWidth && draw.viewportHeight, "PiP viewport extent is empty");
	for (const auto& uniform: spec.uniforms) {
		if (!uniform.sampler || !uniform.active) continue;
		Require(draw.program.samplerUnits[uniform.slot] >= 0 && draw.program.samplerUnits[uniform.slot] < 16, "Original PiP sampler selects an invalid texture unit");
		const auto& source = draw.sampledImages[uniform.slot];
		const bool incomplete = draw.program.uniforms.flipV[3] & (1u << uniform.slot);
		Require(source.incomplete == incomplete, "PiP shader incomplete-image flag differs from its original binding");
		if (source.incomplete) {
			Require(!source.image && !source.original.object && !source.engineSource && source.engineImage == NativePiPEngineImage::None && draw.program.uniforms.flipV[uniform.slot] == 0, "PiP incomplete shader binding has competing image storage/origin");
			continue;
		}
		if (source.original.object) {
			Require(!source.image && !source.engineSource && source.engineImage == NativePiPEngineImage::None && !source.incomplete, "PiP $info has competing storage");
			const auto& object = *source.original.object;
			const auto info = std::dynamic_pointer_cast<const NativeTerrainInfoBacking>(object.backing);
			Require(source.original.knowledge == NativeCUSBindingKnowledge::BoundObject && object.target == NativeCUSTextureTarget::Image2D &&
				object.storage == NativeTextureStorageKnowledge::Published && info && info->source && object.identity == info->storageGeneration &&
				info->contentVersion <= object.version && draw.program.uniforms.flipV[uniform.slot] == 0, "PiP $info lost its original immutable producer generation");
			ValidateScreenSampler(source.sampler);
			continue;
		}
		if (source.engineImage != NativePiPEngineImage::None) {
			Require(!source.image && source.engineSource && source.engineSource->base && source.engineSource->heights,
				"PiP engine image lacks its actual terrain source or has competing CPU storage");
			Require(draw.program.uniforms.flipV[uniform.slot] == 0, "Original PiP engine map images use increasing map-Z rows");
			if (source.engineImage == NativePiPEngineImage::GroundLOS || source.engineImage == NativePiPEngineImage::Radar) {
				const auto& info = source.engineSource->info;
				Require(info && info->mode == NativeMiniMapInfoMode::Los && info->groundLos && info->radar && info->jammer,
					"PiP LOS/radar image lacks the original viewer coverage");
			}
			ValidateScreenSampler(source.sampler);
			continue;
		}
		Require(!source.engineSource, "PiP ordinary image has a competing engine producer");
		Require(source.image && source.image->width && source.image->height && !source.image->incomplete, "Original PiP shader requires its real sampled image");
		ValidateScreenSampler(source.sampler);
		Require(draw.program.uniforms.flipV[uniform.slot] == unsigned(source.image->flipV), "PiP sampled image origin does not match its captured uniform");
	}
	if (spec.geometry == NativePiPGeometry::Immediate) {
		Require(!draw.vertices && !draw.instances && draw.firstVertex == 0 && draw.firstInstance == 0 && draw.instanceCount == 0, "PiP immediate draw has a competing VBO source");
		Require(draw.vertexCount == draw.immediate.size() && !draw.immediate.empty() && !draw.indices.empty() && draw.indices.size() % 3 == 0, "PiP immediate triangle extent differs");
		for (auto index: draw.indices) Require(index < draw.immediate.size(), "PiP immediate index exceeds vertices");
		return;
	}
	Require(draw.immediate.empty() && draw.indices.empty(), "PiP VBO draw has competing immediate geometry");
	Snapshot(draw.vertices);
	const auto& layout = draw.vertices->layout;
	Require(draw.firstVertex <= layout.elements && draw.vertexCount <= layout.elements - draw.firstVertex, "PiP selected vertex range exceeds its generation");
	if (spec.geometry == NativePiPGeometry::Line) {
		Require(!draw.instances && draw.instanceCount == 0 && draw.firstInstance == 0 && layout.stride == 24 && layout.attributes.size() == 2 && draw.vertexCount % 2 == 0, "PiP original line layout/range differs");
		Attribute(layout.attributes[0], 0, 2, 0);
		Attribute(layout.attributes[1], 1, 4, 8);
		return;
	}
	Require(layout.stride == 8 && layout.attributes.size() == 1 && draw.vertexCount == 4 && draw.firstVertex == 0, "PiP instancing needs the original four-corner strip");
	Attribute(layout.attributes[0], 0, 2, 0);
	constexpr std::array<float,8> originalQuad{-1,-1, 1,-1, -1,1, 1,1};
	Require(layout.elements == 4 && std::memcmp(draw.vertices->bytes.data(), originalQuad.data(), sizeof(originalQuad)) == 0, "PiP original corner order differs");
	Snapshot(draw.instances);
	const auto& instances = draw.instances->layout;
	Require(instances.stride == unsigned(spec.instanceFields) * 16 && instances.attributes.size() == spec.instanceFields && draw.firstInstance <= instances.elements && draw.instanceCount <= instances.elements - draw.firstInstance, "PiP instance schema/range differs");
	for (unsigned i = 0; i < spec.instanceFields; ++i) Attribute(instances.attributes[i], i + 1, 4, i * 16);
}

NativePiPLineGeometry BuildNativePiPLineGeometry(const NativePiPDraw& draw,unsigned targetSamples,std::array<double,4> rasterViewport,std::array<uint32_t,2> targetExtent)
{
	ValidateNativePiPDraw(draw);
	Require(draw.program.program == NativePiPProgram::Line, "PiP line rasterization needs its exact line program");
	const auto& u = draw.program.uniforms.values;
	std::vector<ScreenLine> lines;
	lines.reserve(draw.vertexCount / 2);
	const auto clipVertex = [&](std::uint32_t index) {
		std::array<float,6> raw;
		std::memcpy(raw.data(), draw.vertices->bytes.data() + std::size_t(index) * sizeof(raw), sizeof(raw));
		// Original gui_pip.lua lineShaderCode, in original expression order.
		const float x = u[1][0] + raw[0] * u[0][0], y = u[1][1] + raw[1] * u[0][1];
		const float dx = x - u[4][0], dy = y - u[4][1];
		const float px = u[4][0] + (dx * u[3][1] - dy * u[3][0]);
		const float py = u[4][1] + (dx * u[3][0] + dy * u[3][1]);
		const float clipX = px * u[2][0] - 1.0f, clipY = py * u[2][1] - 1.0f;
		float alpha = raw[5];
		if (u[6][0] > 0.5f) {
			const float fadeDuration = std::min(1.0f, u[6][2]);
			const float remaining = u[6][2] - (u[6][1] - raw[5]);
			alpha = 0.85f * std::clamp(remaining / fadeDuration, 0.0f, 1.0f);
		}
		return NativeWorldDrawVertex{{clipX,clipY,0,1},{raw[2] * u[5][0], raw[3] * u[5][1], raw[4] * u[5][2], alpha * u[5][3]}, {}};
	};
	if(targetSamples>1){
		Require(targetExtent[0]&&targetExtent[1]&&rasterViewport[2]>0&&rasterViewport[3]>0,"PiP multisample line requires actual attachment/viewport extent");
		std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(draw.vertexCount);for(uint32_t n=0;n<draw.vertexCount;++n)vertices.push_back(clipVertex(draw.firstVertex+n));
		auto mesh=BuildNativeWorldDrawGeometry(MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Lines,vertices,ScreenDrawState{},draw.raster),rasterViewport,targetExtent[0],targetExtent[1],targetSamples);
		NativePiPLineGeometry result;result.indices=std::move(mesh.indices);result.fullViewport=mesh.fullViewport;
		for(auto& v:mesh.vertices){v.position[2]=(v.position[2]+v.position[3])*.5f;result.vertices.push_back({v.position,v.color,{0,0,0,0}});}return result;
	}
	const auto vertex=[&](uint32_t index){const auto v=clipVertex(index);return ScreenVertex{(v.position[0]+1.0f)*.5f*draw.viewportWidth,(v.position[1]+1.0f)*.5f*draw.viewportHeight,0,v.color};};
	for (std::uint32_t i = 0; i < draw.vertexCount; i += 2)
		lines.push_back({vertex(draw.firstVertex + i), vertex(draw.firstVertex + i + 1), draw.raster.lineWidth,
			draw.raster.lineStipple, draw.raster.stipplePattern, draw.raster.stippleFactor, true});
	ScreenDrawList triangles;
	// The original line program consumes vertex color only. Bound fixed-function
	// textures cannot make this shader line textured or alter its coverage.
	AppendNativeScreenLines(triangles, lines, ScreenDrawState{}, draw.viewportWidth, draw.viewportHeight,targetSamples);
	NativePiPLineGeometry result;
	result.indices.assign(triangles.Indices().begin(), triangles.Indices().end());
	for (const auto& v: triangles.Vertices()) result.vertices.push_back({
		{2.0f * v.x / draw.viewportWidth - 1.0f, 2.0f * v.y / draw.viewportHeight - 1.0f, 0.5f, 1}, v.color, {0,0,0,0}});
	return result;
}

} // namespace rendering
