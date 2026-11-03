/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeMetalInfoData.h"
#include <atomic>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>

namespace rendering {

class NativeInfoSourceRevision {
public:
	explicit NativeInfoSourceRevision(std::uint64_t initial=1) noexcept:value(initial){}
	std::uint64_t Get()const noexcept{return value.load(std::memory_order_relaxed);}
	void Invalidate() noexcept {
		auto old=value.load(std::memory_order_relaxed);
		while(old && !value.compare_exchange_weak(old,
			old==std::numeric_limits<std::uint64_t>::max()?0:old+1,
			std::memory_order_relaxed)){}
	}
private:
	// Zero is permanently unknown after exhaustion: compare bytes, never wrap.
	std::atomic<std::uint64_t> value;
};

class NativeMetalDensitySnapshotCache {
public:
	std::shared_ptr<const NativeMetalDensity> Capture(const void* owner,
		std::span<const std::uint8_t> values,std::uint32_t width,std::uint32_t height,
		std::uint64_t revision)
	{
		if(!owner || !width || !height || values.size()!=std::uint64_t(width)*height)
			throw std::invalid_argument("Original metal density source/extent differs");
		const bool same=cached && sourceOwner==owner && sourceValues==values.data() &&
			cached->width==width && cached->height==height;
		if(same && revision && revision==sourceRevision) return cached;
		if(!same || std::memcmp(cached->values.data(),values.data(),values.size_bytes())){
			auto next=std::make_shared<NativeMetalDensity>();
			next->width=width;next->height=height;
			next->values.assign(values.begin(),values.end());cached=std::move(next);
		}
		sourceOwner=owner;sourceValues=values.data();sourceRevision=revision;
		return cached;
	}
private:
	std::shared_ptr<const NativeMetalDensity> cached;
	const void* sourceOwner=nullptr;
	const std::uint8_t* sourceValues=nullptr;
	std::uint64_t sourceRevision=0;
};
}
