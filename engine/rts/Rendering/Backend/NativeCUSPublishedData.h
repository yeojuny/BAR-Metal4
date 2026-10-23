/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace rendering {

enum class NativeCUSPublishedKind { Transforms, ModelUniforms };

struct NativeCUSPublishedPage {
	static constexpr size_t Bytes = 4096;
	std::array<uint8_t, Bytes> bytes{};
	// One bit per original element. Zero-initialized CPU backing is never
	// evidence that an unobserved original GPU element contained zero.
	std::array<uint64_t, 2> observed{};
};

struct NativeCUSPublishedBuffer {
	NativeCUSPublishedKind kind;
	uint32_t stride = 0;
	uint64_t contextGeneration = 0, allocationSerial = 0, publicationSerial = 0;
	uint64_t publishedDrawFrame = 0;
	size_t capacityElements = 0;
	std::vector<std::shared_ptr<const NativeCUSPublishedPage>> pages;
};

struct NativeCUSPublishedWrite {
	size_t firstElement = 0;
	std::span<const uint8_t> bytes;
};

// Publishes the bytes visible at the original uploader stage, not live CPU
// storage read later during Draw. Retained draws share unchanged pages.
class NativeCUSPublishedStorage {
public:
	explicit NativeCUSPublishedStorage(NativeCUSPublishedKind);
	void NewContext();
	std::shared_ptr<const NativeCUSPublishedBuffer> Publish(uint64_t drawFrame,
		size_t capacityElements, bool newAllocation, std::span<const NativeCUSPublishedWrite>);
	std::shared_ptr<const NativeCUSPublishedBuffer> Current() const { return current; }
private:
	NativeCUSPublishedKind kind;
	uint64_t context = 0, allocation = 0, publication = 0;
	std::shared_ptr<const NativeCUSPublishedBuffer> current;
};

void ReadNativeCUSPublishedElement(const NativeCUSPublishedBuffer&, size_t index,
	std::span<uint8_t> destination);
bool NativeCUSPublishedElementObserved(const NativeCUSPublishedBuffer&, size_t index);

} // namespace rendering
