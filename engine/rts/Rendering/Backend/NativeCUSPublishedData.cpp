/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSPublishedData.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

uint32_t Stride(NativeCUSPublishedKind kind)
{
	switch (kind) {
		case NativeCUSPublishedKind::Transforms: return 32;
		case NativeCUSPublishedKind::ModelUniforms: return 128;
	}
	throw std::runtime_error("Unknown original CUS published buffer kind");
}

bool Observed(const NativeCUSPublishedPage& page, size_t element)
{
	return (page.observed[element / 64] & (uint64_t(1) << (element % 64))) != 0;
}
}

NativeCUSPublishedStorage::NativeCUSPublishedStorage(NativeCUSPublishedKind value): kind(value)
{
	(void)Stride(kind);
}

void NativeCUSPublishedStorage::NewContext()
{
	Check(context != std::numeric_limits<uint64_t>::max(), "CUS published context overflow");
	++context;
	allocation = publication = 0;
	current.reset();
}

std::shared_ptr<const NativeCUSPublishedBuffer> NativeCUSPublishedStorage::Publish(
	uint64_t drawFrame, size_t capacity, bool newAllocation,
	std::span<const NativeCUSPublishedWrite> writes)
{
	const uint32_t stride = Stride(kind);
	Check(context != 0, "CUS publication has no observed original context");
	Check(capacity != 0 && capacity <= std::numeric_limits<size_t>::max() / stride,
		"CUS publication allocation extent overflows");
	Check(publication != std::numeric_limits<uint64_t>::max(), "CUS publication sequence overflow");
	Check(!current || newAllocation || current->capacityElements == capacity,
		"CUS allocation changed without the original replacement event");
	newAllocation = newAllocation || !current;
	Check(!newAllocation || allocation != std::numeric_limits<uint64_t>::max(),
		"CUS publication allocation sequence overflow");
	const size_t elementsPerPage = NativeCUSPublishedPage::Bytes / stride;
	const size_t pageCount = capacity / elementsPerPage + (capacity % elementsPerPage != 0);
	for (const auto& write: writes) {
		Check(write.bytes.size() % stride == 0 && write.firstElement <= capacity &&
			write.bytes.size() / stride <= capacity - write.firstElement,
			"CUS uploaded source range is incomplete or outside its actual allocation");
	}

	auto next = std::make_shared<NativeCUSPublishedBuffer>();
	next->kind = kind;
	next->stride = stride;
	next->contextGeneration = context;
	next->allocationSerial = allocation + uint64_t(newAllocation);
	next->publicationSerial = publication + 1;
	next->publishedDrawFrame = drawFrame;
	next->capacityElements = capacity;
	if (!newAllocation) next->pages = current->pages;
	else next->pages.resize(pageCount);

	std::map<size_t, std::shared_ptr<NativeCUSPublishedPage>> changed;
	for (const auto& write: writes) {
		for (size_t element = 0; element < write.bytes.size() / stride; ++element) {
			const size_t index = write.firstElement + element;
			const size_t pageIndex = index / elementsPerPage;
			const size_t local = index % elementsPerPage;
			const uint8_t* data = write.bytes.data() + element * stride;
			auto existing = changed.find(pageIndex);
			const auto& prior = existing == changed.end() ? next->pages[pageIndex] : existing->second;
			if (prior && Observed(*prior, local) &&
				std::memcmp(prior->bytes.data() + local * stride, data, stride) == 0) continue;
			if (existing == changed.end()) {
				auto page = prior ? std::make_shared<NativeCUSPublishedPage>(*prior) : std::make_shared<NativeCUSPublishedPage>();
				existing = changed.emplace(pageIndex, std::move(page)).first;
			}
			auto& page = *existing->second;
			std::memcpy(page.bytes.data() + local * stride, data, stride);
			page.observed[local / 64] |= uint64_t(1) << (local % 64);
		}
	}
	for (auto& [index, page]: changed) next->pages[index] = std::move(page);
	// Publish only after all range checks and allocations have succeeded.
	allocation = next->allocationSerial;
	publication = next->publicationSerial;
	current = next;
	return current;
}

bool NativeCUSPublishedElementObserved(const NativeCUSPublishedBuffer& buffer, size_t index)
{
	Check(buffer.stride == Stride(buffer.kind), "CUS published original element stride differs");
	if (index >= buffer.capacityElements) return false;
	const size_t elementsPerPage = NativeCUSPublishedPage::Bytes / buffer.stride;
	const size_t page = index / elementsPerPage;
	return page < buffer.pages.size() && buffer.pages[page] &&
		Observed(*buffer.pages[page], index % elementsPerPage);
}

void ReadNativeCUSPublishedElement(const NativeCUSPublishedBuffer& buffer, size_t index,
	std::span<uint8_t> destination)
{
	Check(destination.size() == buffer.stride, "CUS original element read has a different ABI");
	Check(NativeCUSPublishedElementObserved(buffer, index),
		"CUS original GPU-visible element was not published; no zero/identity replacement is allowed");
	const size_t elementsPerPage = NativeCUSPublishedPage::Bytes / buffer.stride;
	const auto& page = *buffer.pages[index / elementsPerPage];
	std::memcpy(destination.data(), page.bytes.data() + (index % elementsPerPage) * buffer.stride, buffer.stride);
}

} // namespace rendering
