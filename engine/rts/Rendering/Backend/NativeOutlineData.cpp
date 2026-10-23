/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeOutlineData.h"
#include "NativeOutlinePrograms.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
void Snapshot(const NativeBufferSnapshotRef& value)
{
	Check(value && value->object && value->revision && value->layout.kind == NativeBufferKind::Vertex,
		"Outline draw lacks an actual immutable VBO generation");
	Check(value->bytes.size() == CheckedNativeBufferByteSize(value->layout),
		"Outline VBO backing differs from its original definition");
}
uint32_t UInt(const NativeBufferSnapshot& buffer, size_t record, size_t offset)
{
	uint32_t value;
	std::memcpy(&value, buffer.bytes.data() + record * buffer.layout.stride + offset, sizeof(value));
	return value;
}
std::string Assemble(std::string source, const NativeOutlineProgramSource& input)
{
	for (const auto& [marker, replacement]: std::array<std::pair<std::string_view, std::string_view>, 3>{{
		{"//__ENGINEUNIFORMBUFFERDEFS__", input.engineDefinitions},
		{"//__DEFINES__", input.definitions}, {"//__QUATERNIONDEFS__", input.quaternionDefinitions}}}) {
		const auto offset = source.find(marker);
		if (offset != std::string::npos) source.replace(offset, marker.size(), replacement);
	}
	return source;
}
}
float NativeOutlineDefine(const NativeOutlineProgramSource& source, const char* name)
{
	const auto& value = source.defines.at(name);
	return std::holds_alternative<float>(value) ? std::get<float>(value) : std::stof(std::get<std::string>(value));
}
void ValidateNativeOutlineProgramSource(const NativeOutlineProgramSource& source)
{
	Check(source.vertex == outlineProgram::VertexSource && source.fragment == outlineProgram::FragmentSource,
		"Original Outline shader literal identity differs");
	Check(source.engineDefinitions == outlineProgram::EngineDefinitions && source.quaternionDefinitions == outlineProgram::QuaternionDefinitions,
		"Original Outline LuaShader helper identity differs");
	Check(source.assembledVertex == Assemble(source.vertex, source) && source.assembledFragment == Assemble(source.fragment, source),
		"Original Outline compiled stages differ from captured helper assembly");
	const std::set<std::string, std::less<>> names{"PER_UNIT", "USEQUATERNIONS", "WIDTH_MIN", "WIDTH_MAX", "OUTLINE_REACH"};
	Check(source.defines.size() == names.size(), "Original Outline compile selector count differs");
	for (const auto& [name, value]: source.defines) {
		Check(names.contains(name), "Unknown original Outline compile selector");
		if (const auto number = std::get_if<float>(&value)) Check(std::isfinite(*number), "Nonfinite Outline compile selector");
		else Check((name == "PER_UNIT" || name == "USEQUATERNIONS") &&
			(std::get<std::string>(value) == "0" || std::get<std::string>(value) == "1"), "Original Outline textual selector differs");
	}
	std::istringstream definitions(source.definitions);
	std::string directive, name, value;
	std::set<std::string> observed;
	while (definitions >> directive >> name >> value) {
		Check(directive == "#define" && names.contains(name) && observed.insert(name).second,
			"Original Outline define assembly contains another directive");
		size_t end = 0;
		const auto scalar = std::stof(value, &end);
		Check(end == value.size() && scalar == NativeOutlineDefine(source, name.c_str()), "Original Outline define value differs from captured selector");
	}
	Check(observed.size() == names.size(), "Original Outline assembled selector was omitted");
	const float perUnit = NativeOutlineDefine(source, "PER_UNIT"), quaternions = NativeOutlineDefine(source, "USEQUATERNIONS");
	Check((perUnit == 0 || perUnit == 1) && (quaternions == 0 || quaternions == 1) &&
		NativeOutlineDefine(source, "WIDTH_MIN") == .1f && NativeOutlineDefine(source, "WIDTH_MAX") == 12.f &&
		NativeOutlineDefine(source, "OUTLINE_REACH") == 1.4241f, "Original Outline selected family constants differ");
	Check(source.name == (perUnit ? "Outline Shader GL4 units" : "Outline Shader GL4 all units"),
		"Original Outline program name does not match its PER_UNIT variant");
}
void ValidateNativeOutlineDraw(const NativeOutlineDraw& draw)
{
	Check(draw.sequence && draw.program && draw.program->pipelineLifetime,
		"Outline draw has no compiled original source owner or ordinal");
	Check(draw.mode == 0x0005 && draw.count == 4 && draw.firstVertex == 0 && draw.instanceCount,
		"Original Outline four-corner instanced triangle strip differs");
	Snapshot(draw.vertices); Snapshot(draw.instances);
	const auto& corners = draw.vertices->layout;
	Check(corners.elements == 4 && corners.stride == 8 && corners.attributes.size() == 1,
		"Original Outline corner buffer layout differs");
	const auto& corner = corners.attributes.front();
	Check(corner.id == 0 && corner.offset == 0 && corner.byteSize == 8 && corner.declaredCount == 2 &&
		corner.scalarType == NativeScalarType::Float32 && !corner.normalized, "Original Outline corner attribute differs");
	const auto& layout = draw.instances->layout;
	const unsigned fields = draw.program->perUnit ? 5 : 2;
	Check(layout.stride == fields * 16 && layout.attributes.size() == fields,
		"Original Outline instance is not its actual bounds/style/SSBO-ID record");
	for (unsigned i = 0; i < fields; ++i) {
		const auto field = std::find_if(layout.attributes.begin(), layout.attributes.end(), [i](const auto& a) { return a.id == i + 1; });
		Check(field != layout.attributes.end() && field->offset == i * 16 && field->byteSize == 16 && field->declaredCount == 4 &&
			!field->normalized && field->scalarType == (i + 1 == fields ? NativeScalarType::UInt32 : NativeScalarType::Float32),
			"Original Outline instance attribute type or offset differs");
	}
	Check(draw.firstInstance <= layout.elements && draw.instanceCount <= layout.elements - draw.firstInstance,
		"Outline instance range exceeds its captured original backing");
	Check(bool(draw.engineUniforms), "Outline draw lacks original engine UBO publications");
	ValidateNativeCUSUniformPublication(*draw.engineUniforms);
	Check(draw.transforms && draw.modelUniforms && draw.modelUniforms->stride == 128 &&
		draw.transforms->stride == (draw.program->useQuaternions ? 32 : 64),
		"Outline draw lacks the actual selected engine model buffers");
	for (size_t i = draw.firstInstance; i < size_t(draw.firstInstance) + draw.instanceCount; ++i) {
		const size_t offset = (fields - 1) * 16;
		const uint32_t transform = UInt(*draw.instances, i, offset), uniform = UInt(*draw.instances, i, offset + 4);
		Check(NativeCUSPublishedElementObserved(*draw.modelUniforms, uniform) && NativeCUSPublishedElementObserved(*draw.transforms, transform),
			"Outline references an unobserved original engine element");
		if (draw.program->useQuaternions)
			Check(NativeCUSPublishedElementObserved(*draw.transforms, uint64_t(transform) + 1),
				"Outline interpolation lacks its second original world transform");
	}
}
void ValidateNativeOutlineClear(const NativeOutlineClear& clear)
{
	Check(clear.sequence && clear.stage == NativeWorldDrawStage::World && clear.mask == ScreenAttachmentStencil,
		"Original Outline clear must retain its ordered world stencil-only event");
}
} // namespace rendering
