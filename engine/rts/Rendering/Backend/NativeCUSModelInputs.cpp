/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSModelInputs.h"

#include "WorldFrameData.h"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}
}

std::shared_ptr<const NativeCUSModelInputs> MakeNativeCUSModelInputs(
	uint64_t frame, uint64_t sequence,
	std::span<const std::shared_ptr<const NativeCUSModelSubmission>> submissions,
	NativeBufferSnapshotRef instances,
	std::shared_ptr<const NativeCUSPublishedBuffer> transforms,
	std::shared_ptr<const NativeCUSPublishedBuffer> uniforms)
{
	Check(sequence != 0 && !submissions.empty() && instances && transforms && uniforms,
		"CUS original model submission lacks immutable source owners");
	Check(transforms->kind == NativeCUSPublishedKind::Transforms && transforms->stride == 32 &&
		uniforms->kind == NativeCUSPublishedKind::ModelUniforms && uniforms->stride == 128,
		"CUS original model SSBO ABI differs");
	Check(transforms->contextGeneration != 0 && uniforms->contextGeneration != 0 &&
		transforms->publicationSerial != 0 && uniforms->publicationSerial != 0,
		"CUS original model buffers have no actual publication");
	const auto& layout = instances->layout;
	Check(layout.kind == NativeBufferKind::Vertex && layout.stride == 16 &&
		layout.elements >= submissions.size() && layout.attributes.size() == 1,
		"CUS original instance buffer layout/capacity differs");
	const auto& attribute = layout.attributes[0];
	Check(attribute.id == 6 && attribute.scalarType == NativeScalarType::UInt32 &&
		attribute.offset == 0 && attribute.byteSize == 16 && attribute.declaredCount == 4 &&
		!attribute.normalized && attribute.valueShape == NativeValueShape::Scalar &&
		instances->bytes.size() == CheckedNativeBufferByteSize(layout),
		"CUS original uvec4 instance payload differs");
	for (const auto& submission: submissions) {
		Check(submission && submission->geometry, "CUS submitted original model owner is unavailable");
		size_t indices = 0;
		for (const auto& piece: submission->geometry->pieces) {
			Check(piece.indices.size() <= std::numeric_limits<size_t>::max() - indices,
				"CUS original model index count overflows");
			indices += piece.indices.size();
		}
		Check(indices == submission->originalIndexCount,
			"CUS retained original model differs from the AddToSubmission index range");
	}
	auto result = std::make_shared<NativeCUSModelInputs>();
	result->sourceDrawFrame = frame;
	result->sourceSequence = sequence;
	result->submissions.assign(submissions.begin(), submissions.end());
	result->originalInstances = std::move(instances);
	result->transforms = std::move(transforms);
	result->modelUniforms = std::move(uniforms);
	return result;
}

std::array<uint32_t, 4> ReadNativeCUSInstance(const NativeCUSModelInputs& inputs, size_t submission)
{
	Check(submission < inputs.submissions.size() && inputs.originalInstances,
		"CUS original instance submission is outside its captured draw");
	Check(inputs.originalInstances->bytes.size() / 16 > submission,
		"CUS original instance bytes were not retained");
	std::array<uint32_t, 4> values;
	std::memcpy(values.data(), inputs.originalInstances->bytes.data() + submission * 16, 16);
	return values;
}

std::array<uint8_t, 32> ReadNativeCUSTransform(const NativeCUSModelInputs& inputs, uint32_t index)
{
	Check(bool(inputs.transforms), "CUS source transform publication is missing");
	std::array<uint8_t, 32> value;
	ReadNativeCUSPublishedElement(*inputs.transforms, index, value);
	return value;
}

std::array<uint8_t, 128> ReadNativeCUSModelUniform(const NativeCUSModelInputs& inputs, size_t submission)
{
	Check(bool(inputs.modelUniforms), "CUS source model-uniform publication is missing");
	const auto instance = ReadNativeCUSInstance(inputs, submission);
	std::array<uint8_t, 128> value;
	ReadNativeCUSPublishedElement(*inputs.modelUniforms, instance[1], value);
	return value;
}

} // namespace rendering
