/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "ScreenDrawList.h"
#include <memory>
#include <utility>

namespace rendering {

// One entry per texture unit, local to one PrepareList/device/lease. This is
// resource preparation reuse, not encoder state reuse. The first resolver
// call must validate and retain the GPU object through submission completion.
// No map, allocation, cross-list cache, or ownership based on a bare address.
template<class Value>
class NativeScreenImagePreparation152 {
public:
	static bool Eligible(const std::shared_ptr<const ScreenTexture>& source) noexcept {
		return !source || (!source->incomplete && !source->renderTarget &&
			!source->postprocessVersion && !source->sourceTexture &&
			!source->sourceDependent && !source->generateMipmaps);
	}

	template<class Resolve>
	Value Prepare(const std::shared_ptr<const ScreenTexture>& source, Resolve&& resolve) {
		// Every ordered/generated/incomplete image keeps its original resolver,
		// including its per-consumer validation and encode-time read receipt.
		if (!Eligible(source)) return std::forward<Resolve>(resolve)();
		if (ready && owner.get() == source.get() &&
			!owner.owner_before(source) && !source.owner_before(owner)) return value;
		auto resolved = std::forward<Resolve>(resolve)();
		if (!resolved) return resolved;
		// A failed resolver never publishes a new key or a partial result.
		owner = source;
		value = std::move(resolved);
		ready = true;
		return value;
	}

private:
	std::shared_ptr<const ScreenTexture> owner;
	Value value{};
	bool ready = false;
};

template<class Value>
class NativeScreenSamplerPreparation152 {
public:
	template<class Resolve>
	Value Prepare(const ScreenSampler& source, Resolve&& resolve) {
		// Compare every original field, before the original resolver validates
		// and packs its key. Invalid values cannot alias a previously valid key.
		if (ready && sampler == source) return value;
		auto resolved = std::forward<Resolve>(resolve)();
		if (!resolved) return resolved;
		sampler = source;
		value = std::move(resolved);
		ready = true;
		return value;
	}

private:
	ScreenSampler sampler;
	Value value{};
	bool ready = false;
};

} // namespace rendering
