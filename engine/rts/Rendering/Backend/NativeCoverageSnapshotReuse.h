/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeInfoSnapshotReuse.h"
#include "Rendering/Backend/NativeMiniMapData.h"
#include <array>

namespace rendering {
// Register/reset runs on the existing engine capture thread after LOS workers
// join. Writers may run concurrently for separate LOS types. No simulation
// object fields, locks, allocations or per-cell notifications are added.
class NativeCoverageMutationTracker {
public:
	std::uint64_t Watch(unsigned index, const void* owner) noexcept {
		if (index >= slots.size() || !owner) return 0;
		auto& slot = slots[index];
		if (slot.owner.load(std::memory_order_acquire) != owner) {
			slot.revision.Invalidate();
			slot.owner.store(owner, std::memory_order_release);
		}
		return slot.revision.Get();
	}
	void Invalidate(const void* owner) noexcept {
		if (!owner) return;
		for (auto& slot: slots)
			if (slot.owner.load(std::memory_order_acquire) == owner)
				slot.revision.Invalidate();
	}
	void Reset() noexcept {
		for (auto& slot: slots) {
			slot.owner.store(nullptr, std::memory_order_release);
			slot.revision.Invalidate();
		}
	}
private:
	struct Slot { std::atomic<const void*> owner{nullptr}; NativeInfoSourceRevision revision; };
	std::array<Slot, 4> slots;
};

class NativeCoverageSnapshotCache {
public:
	std::shared_ptr<const NativeMiniMapCoverage> Capture(const void* owner,
		std::span<const std::uint16_t> values, std::uint32_t width,
		std::uint32_t height, std::uint64_t revision)
	{
		if (!owner || !width || !height || width > 16384 || height > 16384 ||
			values.size() != std::uint64_t(width) * height)
			throw std::invalid_argument("Original info coverage source/extent differs");
		if (cached && revision && revision == sourceRevision && sourceOwner == owner &&
			sourceValues == values.data() && cached->width == width && cached->height == height)
			return cached;
		// Dirty/unknown sources use the original copy. Do not add a full memcmp:
		// candidate65 measured that comparison as slower than the original copy.
		auto next = std::make_shared<NativeMiniMapCoverage>();
		next->width = width; next->height = height;
		next->counts.assign(values.begin(), values.end());
		cached = std::move(next); sourceOwner = owner;
		sourceValues = values.data(); sourceRevision = revision;
		return cached;
	}
private:
	std::shared_ptr<const NativeMiniMapCoverage> cached;
	const void* sourceOwner = nullptr;
	const std::uint16_t* sourceValues = nullptr;
	std::uint64_t sourceRevision = 0;
};
}
