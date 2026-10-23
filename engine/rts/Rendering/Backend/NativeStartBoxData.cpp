/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeStartBoxData.h"
#include "NativeStartBoxPrograms.h"

#include <bitset>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
void Require(bool value, const char* message)
{
	if (!value) throw std::invalid_argument(message);
}

enum class UniformType { Integer, Float, Vector4, Sampler };
struct UniformSpec { std::string_view name; UniformType type; unsigned slot; };
constexpr UniformSpec polygonUniforms[] = {
	{"isMiniMap", UniformType::Integer, 0}, {"myAllyTeamID", UniformType::Integer, 1},
	{"rotationMiniMap", UniformType::Integer, 2}, {"pipVisibleArea", UniformType::Vector4, 0},
	{"mapDepths", UniformType::Sampler, 0}, {"mapNormals", UniformType::Sampler, 1},
	{"scavTexture", UniformType::Sampler, 3}, {"raptorTexture", UniformType::Sampler, 4},
	{"startPolygonSDF", UniformType::Sampler, 5}, {"waterSurfaceMode", UniformType::Integer, 3},
	{"waterLevel", UniformType::Float, 2},
};
constexpr UniformSpec coneUniforms[] = {
	{"isMinimap", UniformType::Float, 0}, {"rotationMiniMap", UniformType::Integer, 2},
	{"startPosScale", UniformType::Float, 1}, {"pipVisibleArea", UniformType::Vector4, 0},
};

constexpr UniformSpec bakeUniforms[] = {{"myAllyTeamID", UniformType::Integer, 1}};

std::span<const UniformSpec> Uniforms(NativeStartBoxProgram program)
{
	if (program == NativeStartBoxProgram::Polygon) return polygonUniforms;
	if (program == NativeStartBoxProgram::SDFBake) return bakeUniforms;
	return coneUniforms;
}

const UniformSpec* Uniform(const NativeStartBoxProgramSnapshot& state, int location)
{
	const auto uniforms = Uniforms(state.program);
	// In31657 the final polygon reads allyteam membership from the bake.
	if (state.program == NativeStartBoxProgram::Polygon && location == 1) return nullptr;
	if (location < 0 || std::size_t(location) >= uniforms.size()) return nullptr;
	if (state.program == NativeStartBoxProgram::Polygon &&
		((location == 6 && !state.defines->scavAllyTeam) || (location == 7 && !state.defines->raptorAllyTeam))) return nullptr;
	return &uniforms[location];
}

std::int32_t Integer(float value)
{
	Require(std::isfinite(value) && std::trunc(value) == value &&
		double(value) >= double(std::numeric_limits<std::int32_t>::min()) &&
		double(value) <= double(std::numeric_limits<std::int32_t>::max()), "Original StartBox integer define is invalid");
	return static_cast<std::int32_t>(value);
}

void Snapshot(const NativeBufferSnapshotRef& value)
{
	Require(value && value->object && value->revision && value->bytes.size() == CheckedNativeBufferByteSize(value->layout),
		"Original StartBox buffer generation or capacity is missing");
}

void Attribute(const NativeBufferAttribute& value, unsigned id, unsigned offset, NativeValueShape shape)
{
	const unsigned bytes = shape == NativeValueShape::Vector4 ? 64 : 16;
	Require(value.id == id && value.offset == offset && value.scalarType == NativeScalarType::Float32 &&
		value.valueShape == shape && value.declaredCount == 4 && value.byteSize == bytes && !value.normalized,
		"Original StartBox buffer attribute differs");
}
}

std::uint64_t NativeStartBoxSourceHash(std::string_view source) noexcept
{
	std::uint64_t value = 14695981039346656037ULL;
	for (unsigned char byte: source) value = (value ^ byte) * 1099511628211ULL;
	return value;
}

void ValidateNativeStartBoxEngineDefinitions(std::string_view source)
{
	Require(NativeStartBoxSourceHash(source) == startBoxProgram::EngineDefinitionsHash,
		"Original31657 StartBox engine definitions differ");
}

NativeStartBoxProgram IdentifyNativeStartBoxProgram(std::string_view vertex, std::string_view fragment)
{
	const auto v = NativeStartBoxSourceHash(vertex), f = NativeStartBoxSourceHash(fragment);
	if (v == startBoxProgram::PolygonVertexHash && f == startBoxProgram::PolygonFragmentHash) return NativeStartBoxProgram::Polygon;
	if (v == startBoxProgram::ConeVertexHash && f == startBoxProgram::ConeFragmentHash) return NativeStartBoxProgram::Cone;
	if (v == startBoxProgram::SDFBakeVertexHash && f == startBoxProgram::SDFBakeFragmentHash) return NativeStartBoxProgram::SDFBake;
	throw std::invalid_argument("StartBox shader differs from the exact31657 translated program");
}

NativeStartBoxDefines ParseNativeStartBoxDefines(NativeStartBoxProgram program, std::string_view text)
{
	Require(program == NativeStartBoxProgram::Polygon || program == NativeStartBoxProgram::Cone || program == NativeStartBoxProgram::SDFBake, "Unknown original StartBox program");
	NativeStartBoxDefines out; out.originalText = text;
	constexpr std::array<std::string_view,9> names{"ALPHA", "NUM_BOXES", "NUM_POLYGONS", "NUM_POINTS", "MINY", "MAXY", "MAX_STEEPNESS", "SCAV_ALLYTEAM_ID", "RAPTOR_ALLYTEAM_ID"};
	std::bitset<9> seen;
	while (!text.empty()) {
		const auto end = text.find('\n');
		Require(end != std::string_view::npos, "Original StartBox define lost its line boundary");
		auto line = text.substr(0,end); text.remove_prefix(end + 1);
		Require(line.starts_with("#define "), "Original StartBox define prefix differs");
		line.remove_prefix(8);
		const auto split = line.find(' ');
		Require(split != std::string_view::npos, "Original StartBox define has no value");
		const auto found = std::find(names.begin(), names.end(), line.substr(0,split));
		Require(found != names.end(), "Unknown original StartBox define");
		const auto index = std::size_t(found - names.begin());
		Require(!seen[index] && (program == NativeStartBoxProgram::Polygon ||
			(program == NativeStartBoxProgram::Cone && index == 0) ||
			(program == NativeStartBoxProgram::SDFBake && (index == 2 || index == 3 || index == 7 || index == 8))), "Duplicate or wrong-program StartBox define");
		line.remove_prefix(split + 1);
		float value = 0;
		const auto parsed = std::from_chars(line.data(),line.data()+line.size(),value);
		Require(parsed.ec == std::errc{} && parsed.ptr == line.data()+line.size() && std::isfinite(value), "Original StartBox numeric define is invalid");
		seen[index] = true;
		switch (index) {
			case 0: out.alpha = value; break;
			case 1: case 2: case 3: {
				const auto count = Integer(value); Require(count > 0, "Original StartBox polygon count is empty");
				if (index == 1) out.boxes = count;
				if (index == 2) out.polygons = count;
				if (index == 3) out.points = count;
				break;
			}
			case 4: out.minimumY = value; break;
			case 5: out.maximumY = value; break;
			case 6: out.maximumSteepness = value; break;
			case 7: out.scavAllyTeam = Integer(value); break;
			case 8: out.raptorAllyTeam = Integer(value); break;
		}
	}
	if (program != NativeStartBoxProgram::SDFBake)
		Require(seen[0] && out.alpha == 0.5f, "Original StartBox alpha definition differs");
	if (program == NativeStartBoxProgram::SDFBake)
		Require(seen[2] && seen[3] && out.polygons >= 2 && out.points % 4 == 0 && out.points >= out.polygons,
			"Original StartBox bake polygon allocation differs");
	if (program == NativeStartBoxProgram::Polygon) {
		for (unsigned i = 1; i < 7; ++i) Require(seen[i], "Original StartBox required define is missing");
		Require(out.boxes == out.polygons && out.polygons >= 2 && out.points % 4 == 0 && out.points >= out.polygons &&
			out.minimumY < out.maximumY && out.maximumSteepness >= -1 && out.maximumSteepness <= 1,
			"Original StartBox polygon allocation or ground range differs");
	}
	return out;
}

NativeStartBoxProgramState::NativeStartBoxProgramState(std::string_view vertex, std::string_view fragment, std::string_view defines)
{
	state.program = IdentifyNativeStartBoxProgram(vertex,fragment);
	state.defines = std::make_shared<const NativeStartBoxDefines>(ParseNativeStartBoxDefines(state.program,defines));
}

int NativeStartBoxProgramState::GetUniformLocation(std::string_view name) const
{
	const auto uniforms = Uniforms(state.program);
	for (unsigned i = 0; i < uniforms.size(); ++i) if (uniforms[i].name == name && Uniform(state,i)) return i;
	return -1;
}

bool NativeStartBoxProgramState::SetFloats(int location, std::span<const float> values)
{
	if (location == -1) return true;
	const auto* spec = Uniform(state,location);
	if (!spec) return false;
	if (spec->type == UniformType::Vector4 && values.size() == 4) {
		std::copy(values.begin(),values.end(),state.pipVisibleArea.begin()); return true;
	}
	if (spec->type != UniformType::Float || values.size() != 1) return false;
	if (spec->slot == 0) state.coneIsMiniMap = values.front();
	if (spec->slot == 1) state.startPosScale = values.front();
	if (spec->slot == 2) state.waterLevel = values.front();
	return true;
}

bool NativeStartBoxProgramState::SetIntegers(int location, std::span<const std::int32_t> values)
{
	if (location == -1) return true;
	const auto* spec = Uniform(state,location);
	if (!spec || values.size() != 1) return false;
	if (spec->type == UniformType::Sampler) { state.samplerUnits[spec->slot] = values.front(); return true; }
	if (spec->type != UniformType::Integer) return false;
	if (spec->slot == 0) state.isMiniMap = values.front();
	if (spec->slot == 1) state.myAllyTeam = values.front();
	if (spec->slot == 2) state.rotationMiniMap = values.front();
	if (spec->slot == 3) state.waterSurfaceMode = values.front();
	return true;
}

std::array<bool,6> NativeStartBoxActiveSamplers(const NativeStartBoxProgramSnapshot& program)
{
	Require(bool(program.defines), "Original StartBox shader defines are missing");
	if (program.program == NativeStartBoxProgram::Cone || program.program == NativeStartBoxProgram::SDFBake) return {};
	Require(program.program == NativeStartBoxProgram::Polygon, "Unknown original StartBox program");
	return {true,true,false,program.defines->scavAllyTeam.has_value(),program.defines->raptorAllyTeam.has_value(),true};
}

NativeStartBoxUniformBlock ReadNativeStartBoxUniforms(const NativeStartBoxDraw& draw)
{
	Require(bool(draw.uniforms), "Original StartBox UBO publication is missing");
	ValidateNativeCUSUniformPublication(*draw.uniforms);
	const auto& matrices = *draw.uniforms->matrices;
	const auto& params = *draw.uniforms->params;
	Require(matrices.sourceDrawFrame <= draw.sourceDrawFrame && params.sourceDrawFrame <= draw.sourceDrawFrame,
		"Original StartBox UBO publication is from a future draw");
	NativeStartBoxUniformBlock out;
	// Exact UniformConstants / selected LuaShader ABI. Retain the publication,
	// not a later live camera or a new per-family UpdateMatrices invocation.
	std::memcpy(out.cameraViewProjection.data(), matrices.bytes.data() + 5*64, 64);
	std::memcpy(out.cameraViewProjectionInverse.data(), matrices.bytes.data() + 9*64, 64);
	std::memcpy(out.cameraViewInverse.data(), matrices.bytes.data() + 7*64, 64);
	std::memcpy(out.timeInfo.data(), params.bytes.data() + 16, 16);
	std::memcpy(out.mapSize.data(), params.bytes.data() + 48, 16);
	std::uint32_t capabilities;
	std::memcpy(&capabilities, params.bytes.data() + 12, 4);
	out.options[1] = (capabilities & 1u) ? 0 : 1;
	out.pipVisibleArea = draw.program.pipVisibleArea;
	out.parameters = {draw.program.isMiniMap, draw.program.myAllyTeam, draw.program.rotationMiniMap, draw.program.waterSurfaceMode};
	out.values[3] = draw.program.waterLevel;
	out.values[0] = draw.program.startPosScale;
	out.values[1] = draw.program.coneIsMiniMap;
	for (const auto* matrix: {&out.cameraViewProjection, &out.cameraViewProjectionInverse, &out.cameraViewInverse})
		for (float value: *matrix) Require(std::isfinite(value), "Original StartBox matrix is nonfinite");
	for (const auto* vector: {&out.timeInfo, &out.mapSize, &out.pipVisibleArea})
		for (float value: *vector) Require(std::isfinite(value), "Original StartBox frame uniform is nonfinite");
	return out;
}

void ValidateNativeStartBoxDraw(const NativeStartBoxDraw& draw)
{
	Require(bool(draw.program.defines), "Original StartBox program definition is missing");
	Require(draw.viewportWidth && draw.viewportHeight, "Original StartBox target viewport is empty");
	const auto& program = draw.program;
	const auto& defines = *program.defines;
	Snapshot(draw.vertices);
	const auto& vertex = draw.vertices->layout;
	Require(vertex.kind == NativeBufferKind::Vertex && vertex.stride == 16 && vertex.attributes.size() == 1 &&
		draw.firstVertex <= vertex.elements && draw.vertexCount <= vertex.elements - draw.firstVertex && draw.vertexCount % 3 == 0,
		"Original StartBox triangle stream or range differs");
	Attribute(vertex.attributes[0],0,0,NativeValueShape::Scalar);
	if (draw.indices) {
		Snapshot(draw.indices);
		Require(draw.indices->layout.kind == NativeBufferKind::Index, "Original StartBox index attachment differs");
	}
	if (program.program != NativeStartBoxProgram::Cone && draw.instances) {
		Snapshot(draw.instances);
		const auto& placeholder = draw.instances->layout;
		Require(placeholder.kind == NativeBufferKind::Vertex && placeholder.elements == 1 && placeholder.stride == 4 &&
			placeholder.attributes.size() == 1, "Original StartBox unused instance slot differs");
		const auto& attribute = placeholder.attributes[0];
		Require(attribute.id == 15 && attribute.offset == 0 && attribute.declaredCount == 1 && attribute.byteSize == 4 &&
			attribute.scalarType == NativeScalarType::Float32 && attribute.valueShape == NativeValueShape::Scalar && !attribute.normalized,
			"Original StartBox placeholder attribute differs");
	}
	if (program.program == NativeStartBoxProgram::Polygon) {
		Require(draw.stage == NativeStartBoxStage::WorldPreUnit || draw.stage == NativeStartBoxStage::MiniMap,
			"Original StartBox polygon is outside its original callback");
		Require(draw.instanceCount == 1 && draw.firstInstance == 0 && !draw.polygons.snapshot,
			"Original StartBox final rectangle must sample the baked field without a polygon SSBO");
	} else if (program.program == NativeStartBoxProgram::SDFBake) {
		Require(draw.stage == NativeStartBoxStage::TextureBake && draw.instanceCount == 1 && draw.firstInstance == 0 &&
			!draw.surface.blend.enabled && !draw.raster.depthTest && !draw.raster.depthWrite,
			"Original StartBox bake must write unblended data into its world RTT");
		Snapshot(draw.polygons.snapshot);
		const auto& storage = draw.polygons.snapshot->layout;
		Require(storage.kind == NativeBufferKind::Storage && storage.stride == 64 && storage.elements == defines.points &&
			storage.attributes.size() == 1 && draw.polygons.binding == 4 && draw.polygons.offset == 0 &&
			draw.polygons.size == draw.polygons.snapshot->bytes.size(), "Original StartBox SSBO allocation/binding differs");
		Attribute(storage.attributes[0],0,0,NativeValueShape::Vector4);
		// The engine allocates 64 bytes per element, but original Upload writes a
		// contiguous vec4 prefix. GPU must read that original 16-byte stream.
		Require(std::size_t(defines.points) * 16 <= draw.polygons.size, "Original StartBox vec4 stream exceeds its SSBO");
	} else {
		Require(program.program == NativeStartBoxProgram::Cone &&
			(draw.stage == NativeStartBoxStage::World || draw.stage == NativeStartBoxStage::MiniMap),
			"Original StartBox cone program or callback differs");
		Require(!draw.polygons.snapshot, "Original StartBox cone has a competing polygon buffer");
		Snapshot(draw.instances);
		const auto& instances = draw.instances->layout;
		Require(instances.kind == NativeBufferKind::Vertex && instances.stride == 32 && instances.attributes.size() == 2 &&
			draw.firstInstance <= instances.elements && draw.instanceCount <= instances.elements - draw.firstInstance,
			"Original StartBox cone instance layout or range differs");
		Attribute(instances.attributes[0],3,0,NativeValueShape::Scalar);
		Attribute(instances.attributes[1],4,16,NativeValueShape::Scalar);
	}
	const auto active = NativeStartBoxActiveSamplers(program);
	for (unsigned i = 0; i < active.size(); ++i) {
		if (!active[i]) continue;
		Require(program.samplerUnits[i] >= 0 && program.samplerUnits[i] < NativeCUSTextureHistory::Units,
			"Original StartBox sampler selects an invalid texture unit");
		const auto& binding = draw.images[i];
		if (i == 5)
			Require(binding.knowledge == NativeCUSBindingKnowledge::BoundObject && binding.object &&
				binding.object->allocation.originalInternalFormat == 0x881A &&
				binding.object->storage == NativeTextureStorageKnowledge::Published,
				"Original StartBox final field requires the published RGBA16F bake version");
		if (binding.knowledge == NativeCUSBindingKnowledge::DefaultObject) {
			Require(!binding.object && binding.defaultStorageKnownEmpty, "Original StartBox default texture storage is unobserved");
			continue;
		}
		Require(binding.knowledge == NativeCUSBindingKnowledge::BoundObject && binding.object &&
			binding.object->identity && binding.object->contextGeneration && binding.object->version &&
			binding.object->target == NativeCUSTextureTarget::Image2D &&
			binding.object->storage != NativeTextureStorageKnowledge::Unobserved,
			"Original StartBox texture binding or storage was not observed");
		// Allocation-only storage and retained color/depth bindings are resolved
		// by the common actual-image provider. Never invent a black depth image.
	}
	(void)ReadNativeStartBoxUniforms(draw);
}

} // namespace rendering
