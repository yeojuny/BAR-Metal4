/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "Rendering/Backend/WorldFrameData.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>

class CReadMap;

namespace rendering {

// Context, allocation name, completed CPU height-writer revision, explicit
// CPU-only copy revision. No simulation/draw frame is used as a content key.
using NativeTerrainHeightVersion = std::array<uint64_t,4>;

class NativeTerrainHeightSnapshotCache {
public:
	std::shared_ptr<const WorldTerrainHeights> Capture(const void* owner,
		const float* values,uint32_t width,uint32_t height,
		std::optional<NativeTerrainHeightVersion> version)
	{
		const uint64_t count=uint64_t(width)*height;
		if (!owner || !values || !width || !height ||
			count>std::numeric_limits<size_t>::max()/sizeof(float))
			throw std::invalid_argument("Original terrain height snapshot source/extent is invalid");
		const bool sameSource=cached && sourceOwner==owner && sourceValues==values &&
			cached->width==width && cached->height==height;
		if (sameSource && version && sourceVersion==version)
			return cached;
		// A writer can publish a dirty region whose values did not change. Keep
		// the immutable owner then, so consumers do not rebuild GPU resources.
		// Without an authoritative writer version, retain the full comparison.
		if (!sameSource || std::memcmp(values,cached->corners.data(),size_t(count)*sizeof(float))) {
			auto next=std::make_shared<WorldTerrainHeights>();
			next->width=width;next->height=height;
			next->corners.assign(values,values+size_t(count));
			for (float value:next->corners)
				if (!std::isfinite(value))
					throw std::invalid_argument("Non-finite original terrain height");
			cached=std::move(next);
		}
		sourceOwner=owner;sourceValues=values;sourceVersion=version;
		return cached;
	}
	void Reset() noexcept {cached.reset();sourceOwner=nullptr;sourceValues=nullptr;sourceVersion.reset();}

private:
	std::shared_ptr<const WorldTerrainHeights> cached;
	const void* sourceOwner=nullptr;
	const float* sourceValues=nullptr;
	std::optional<NativeTerrainHeightVersion> sourceVersion;
};

// One unsynced draw-thread cache, shared by world, blueprint, placement and
// minimap consumers. Snapshot ownership remains immutable through GPU use.
std::shared_ptr<const WorldTerrainHeights> CaptureNativeTerrainHeightSnapshot(
	const CReadMap*,const float*,uint32_t width,uint32_t height);

} // namespace rendering
