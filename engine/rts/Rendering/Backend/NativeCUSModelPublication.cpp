/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSModelPublication.h"

#include "Rendering/Common/UpdateList.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Models/ModelsMemStorage.h"

#include <limits>
#include <stdexcept>

namespace rendering {

std::shared_ptr<const NativeCUSPublishedBuffer> PublishNativeCUSModelRanges(
	NativeCUSPublishedStorage& publisher, NativeCUSPublishedKind kind, UpdateList& updates,
	std::span<const uint8_t> storage, uint64_t drawFrame, size_t& capacity)
{
	const size_t stride = kind == NativeCUSPublishedKind::Transforms ? 32 : 128;
	const size_t increment = kind == NativeCUSPublishedKind::Transforms ? 1u << 14 : 1u << 11;
	if (kind != NativeCUSPublishedKind::Transforms && kind != NativeCUSPublishedKind::ModelUniforms)
		throw std::runtime_error("Unknown CUS original model uploader kind");
	if (capacity == 0 || storage.size() % stride != 0 || updates.Size() != storage.size() / stride)
		throw std::runtime_error("CUS original model update storage/capacity differs");
	const size_t elements = storage.size() / stride;
	size_t nextCapacity = capacity;
	if (elements > capacity) {
		if (elements > std::numeric_limits<size_t>::max() - increment + 1)
			throw std::runtime_error("CUS original model SSBO growth overflows");
		nextCapacity = ((elements + increment - 1) / increment) * increment;
		// Exact original Resize contract: its replacement does not copy bytes.
		updates.SetNeedUpdateAll();
	}
	if (!updates.NeedUpdate() && publisher.Current()) return publisher.Current();
	std::vector<NativeCUSPublishedWrite> writes;
	std::vector<std::pair<size_t, size_t>> ranges;
	const UpdateList& readUpdates = updates;
	for (auto range = readUpdates.GetNext(); range; range = readUpdates.GetNext(range)) {
		const auto [first, count] = readUpdates.GetOffsetAndSize(*range);
		if (first > elements || count > elements - first)
			throw std::runtime_error("CUS original dirty SSBO range is outside CPU storage");
		writes.push_back({first, storage.subspan(first * stride, count * stride)});
		ranges.emplace_back(first, count);
	}
	auto published = publisher.Publish(drawFrame, nextCapacity, nextCapacity != capacity, writes);
	for (const auto& [first, count]: ranges) updates.DecrementUpdate(first, count);
	updates.CalcNeedUpdateAll();
	capacity = nextCapacity;
	return published;
}

void NativeCUSModelPublication::Initialize()
{
	if (initialized) throw std::runtime_error("CUS model publication initialized twice");
	if (!globalRendering || globalRendering->haveGL4)
		throw std::runtime_error("CUS native model publication cannot consume an active original GL upload list");
	transforms.NewContext();
	uniforms.NewContext();
	transformCapacity = 1u << 16;
	uniformCapacity = 1u << 12;
	// One immutable publication per update; retained snapshots own prior pages.
	// GL's rotating upload-buffer count is not a data dependency for Metal.
	transformsMemStorage.GetUpdateList().SetTrueValue(1);
	modelUniformsStorage.GetUpdateList().SetTrueValue(1);
	initialized = true;
}

void NativeCUSModelPublication::Reset()
{
	initialized = false;
	transformCapacity = uniformCapacity = 0;
	transforms.NewContext();
	uniforms.NewContext();
}

void NativeCUSModelPublication::UpdateTransforms(uint64_t frame)
{
	if (!initialized) throw std::runtime_error("CUS transform publication is not initialized");
	auto lock = CModelsLock::GetScopedLock();
	static_assert(sizeof(Transform) == 32);
	const auto& data = transformsMemStorage.GetData();
	const auto bytes = std::span(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(Transform));
	PublishNativeCUSModelRanges(transforms, NativeCUSPublishedKind::Transforms,
		transformsMemStorage.GetUpdateList(), bytes, frame, transformCapacity);
}

void NativeCUSModelPublication::UpdateModelUniforms(uint64_t frame)
{
	if (!initialized) throw std::runtime_error("CUS model-uniform publication is not initialized");
	auto lock = CModelsLock::GetScopedLock();
	static_assert(sizeof(ModelUniformData) == 128);
	const auto& data = modelUniformsStorage.GetData();
	const auto bytes = std::span(reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(ModelUniformData));
	PublishNativeCUSModelRanges(uniforms, NativeCUSPublishedKind::ModelUniforms,
		modelUniformsStorage.GetUpdateList(), bytes, frame, uniformCapacity);
}

NativeCUSModelPublication& OriginalCUSModelPublication()
{
	static NativeCUSModelPublication publication;
	return publication;
}

} // namespace rendering
