/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace rendering {

// The caller serializes Acquire/BeginBatch/Clear. Every prepared consumer and
// every submitted completion graph must own the returned shared_ptr. A slot
// can be changed only when the pool is its sole owner; elapsed time, a CPU
// worker return, and one of several completion signals are not retirement.
template<class Key, class Value, std::size_t Capacity>
class NativeRetiredResourcePool {
public:
	void BeginBatch() noexcept {
		for (auto& [key, bucket]: buckets) bucket.next = 0;
	}
	void Clear() noexcept { buckets.clear(); cached = 0; }
	std::size_t CachedCount() const noexcept { return cached; }

	template<class Factory, class Reset>
	std::shared_ptr<Value> Acquire(const Key& key, Factory&& factory, Reset&& reset) {
		auto& bucket = buckets[key];
		// Visit each old slot at most once per batch. A busy slot stays busy for
		// this scan even if another owner releases it later; a fresh allocation
		// is always safe. Newly allocated slots already have a consumer owner.
		while (bucket.next < bucket.slots.size()) {
			auto& slot = bucket.slots[bucket.next++];
			if (slot.use_count() != 1) continue;
			reset(*slot);
			return slot;
		}
		auto slot = std::make_shared<Value>(factory());
		if (cached < Capacity) {
			bucket.slots.push_back(slot);
			bucket.next = bucket.slots.size();
			++cached;
		}
		return slot;
	}

private:
	struct Bucket {
		std::vector<std::shared_ptr<Value>> slots;
		std::size_t next = 0;
	};
	std::map<Key, Bucket> buckets;
	std::size_t cached = 0;
};

} // namespace rendering
