/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDeferredLightData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
void Snapshot(const NativeBufferSnapshotRef& value, NativeBufferKind kind)
{
	Check(value && value->object && value->revision && value->layout.kind == kind,
		"Deferred light draw lacks an actual immutable VBO generation");
	Check(value->bytes.size() == CheckedNativeBufferByteSize(value->layout),
		"Deferred light VBO backing does not match its original definition");
}
uint32_t UInt(const NativeBufferSnapshot& buffer, size_t record, size_t offset)
{
	uint32_t value;
	std::memcpy(&value, buffer.bytes.data() + record * buffer.layout.stride + offset, sizeof(value));
	return value;
}
float Float(const NativeBufferSnapshot& buffer, size_t record, size_t offset)
{
	float value;
	std::memcpy(&value, buffer.bytes.data() + record * buffer.layout.stride + offset, sizeof(value));
	return value;
}
float Uniform(const NativeDeferredLightDraw& draw, const char* name)
{
	const auto item = draw.floatUniforms.find(name);
	Check(item != draw.floatUniforms.end() && item->second.size() == 1,
		"Deferred light draw lacks an actual scalar uniform value");
	return item->second[0];
}
}
void ValidateNativeDeferredLightProgramSource(const NativeDeferredLightProgramSource& source)
{
	Check(source.name == "Deferred Lights GL4" && !source.vertex.empty() && !source.fragment.empty(),
		"Original deferred light program name or source is missing");
	Check(!source.assembledVertex.empty() && !source.assembledFragment.empty() &&
		!source.engineDefinitions.empty() && !source.quaternionDefinitions.empty(),
		"Original deferred light helper assembly was not captured");
	const std::set<std::string, std::less<>> names{
		"MIERAYLEIGHRATIO", "RAYMARCHSTEPS", "USE3DNOISE", "SURFACECOLORMODULATION",
		"BLEEDFACTOR", "VOIDWATER", "SCREENSPACESHADOWS", "DEPTH_CLIP01", "USEQUATERNIONS"
	};
	for (const auto& [name, value]: source.defines) {
		Check(names.contains(name), "Unknown original deferred light compile selector");
		if (const auto number = std::get_if<float>(&value))
			Check(std::isfinite(*number), "Nonfinite deferred light compile selector");
		else {
			const auto& text = std::get<std::string>(value);
			Check((name == "DEPTH_CLIP01" || name == "USEQUATERNIONS") && (text == "0" || text == "1"),
				"Original deferred light textual compile selector differs");
		}
	}
	for (const auto& name: names)
		Check(name == "SCREENSPACESHADOWS" || source.defines.contains(name),
			"Original deferred light compile selector was omitted");
}
void ValidateNativeDeferredLightDraw(const NativeDeferredLightDraw& draw)
{
	Check(draw.sequence && draw.program && draw.program->pipelineLifetime,
		"Deferred light draw has no compiled original source owner or ordinal");
	Check(draw.mode == 0x0004 && draw.count && draw.instanceCount,
		"Original deferred light instanced triangle draw parameters differ");
	Snapshot(draw.vertices, NativeBufferKind::Vertex);
	Snapshot(draw.instances, NativeBufferKind::Vertex);
	const auto& layout = draw.instances->layout;
	Check(layout.stride == 116 && layout.attributes.size() == 8,
		"Original deferred light instance record is not the actual 116-byte layout");
	for (unsigned i = 0; i < 8; ++i) {
		const auto field = std::find_if(layout.attributes.begin(), layout.attributes.end(), [i](const auto& a) { return a.id == i + 3; });
		const size_t offset = i < 6 ? 16 * i : (i == 6 ? 96 : 100);
		const size_t bytes = i == 6 ? 4 : 16;
		Check(field != layout.attributes.end() && field->offset == offset && field->byteSize == bytes &&
			field->declaredCount == (i == 6 ? 1 : 4) && !field->normalized &&
			field->scalarType == (i < 6 ? NativeScalarType::Float32 : NativeScalarType::UInt32),
			"Original deferred light instance attribute type or offset differs");
	}
	Check(draw.firstInstance <= layout.elements && draw.instanceCount <= layout.elements - draw.firstInstance,
		"Deferred light instance range exceeds original captured backing");
	if (draw.indexed) {
		Snapshot(draw.indices, NativeBufferKind::Index);
		const auto scalar = draw.indices->layout.attributes.front().scalarType;
		const auto type = scalar == NativeScalarType::UInt8 ? 0x1401u : scalar == NativeScalarType::UInt16 ? 0x1403u : 0x1405u;
		Check(draw.indexType == type && draw.firstIndex <= draw.indices->layout.elements &&
			draw.count <= draw.indices->layout.elements - draw.firstIndex,
			"Deferred light index type or range differs from original VBO");
		for (size_t i = draw.firstIndex; i < size_t(draw.firstIndex) + draw.count; ++i) {
			uint32_t index = 0;
			std::memcpy(&index, draw.indices->bytes.data() + i * draw.indices->layout.stride, draw.indices->layout.stride);
			const uint32_t restart = type == 0x1401u ? 0xffu : type == 0x1403u ? 0xffffu : 0xffffffffu;
			if (index == restart) continue; // Original DrawElements enables fixed maximum restart index.
			Check(draw.baseVertex >= 0 && uint64_t(index) + uint32_t(draw.baseVertex) < draw.vertices->layout.elements,
				"Deferred light index references outside original captured vertices");
		}
	} else {
		Check(!draw.indices && draw.firstVertex <= draw.vertices->layout.elements &&
			draw.count <= draw.vertices->layout.elements - draw.firstVertex,
			"Deferred light DrawArrays range exceeds original captured vertices");
	}
	Check(bool(draw.engineUniforms), "Deferred light draw lacks original engine UBO publications");
	ValidateNativeCUSUniformPublication(*draw.engineUniforms);
	if (Uniform(draw, "attachedtounitID") > 0) {
		Check(draw.transforms && draw.modelUniforms && draw.transforms->stride && draw.modelUniforms->stride == 128,
			"Unit-attached deferred light lacks original transform or model uniform publications");
		for (size_t i = draw.firstInstance; i < size_t(draw.firstInstance) + draw.instanceCount; ++i) {
			const uint32_t transform = UInt(*draw.instances, i, 100), uniform = UInt(*draw.instances, i, 104);
			const uint32_t piece = UInt(*draw.instances, i, 96);
			Check(NativeCUSPublishedElementObserved(*draw.modelUniforms, uniform) &&
				NativeCUSPublishedElementObserved(*draw.transforms, transform),
				"Unit-attached deferred light references an unobserved original engine element");
			if (draw.transforms->stride == 32) {
				Check(NativeCUSPublishedElementObserved(*draw.transforms, uint64_t(transform) + 1),
					"Unit light interpolation lacks its second original world transform");
				if (piece) {
					const uint64_t index = uint64_t(transform) + 2 * uint64_t(piece);
					Check(NativeCUSPublishedElementObserved(*draw.transforms, index) && NativeCUSPublishedElementObserved(*draw.transforms, index + 1),
						"Unit light interpolation lacks its original piece transform pair");
				}
			} else {
				Check(draw.transforms->stride == 64 && (!piece || NativeCUSPublishedElementObserved(*draw.transforms, uint64_t(transform) + piece)),
					"Unit light matrix path lacks its original piece matrix");
			}
			if (Float(*draw.instances, i, 32) < -.01f || Float(*draw.instances, i, 36) < -.01f || Float(*draw.instances, i, 40) < -.01f ||
				Float(*draw.instances, i, 80) < -99.f || Float(*draw.instances, i, 84) < -99.f || Float(*draw.instances, i, 88) < -99.f)
				(void)ReadNativeCUSTeamColor(*draw.engineUniforms, UInt(*draw.instances, i, 108) & 255u);
		}
	}
}
} // namespace rendering
