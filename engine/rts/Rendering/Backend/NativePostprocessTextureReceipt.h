/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>

namespace rendering {
enum class NativePostprocessTextureAspect : std::uint8_t { Color, Depth };

// Identity for one immutable postprocess image. Only root, single-sample 2D
// images at slice and mip zero have a receipt on this path.
struct NativePostprocessTextureReceipt {
	const void* version = nullptr;
	const void* storage = nullptr;
	std::uint64_t object = 0;
	std::uint64_t generation = 0;
	std::uint64_t producer = 0;
	std::uint64_t event = 0;
	NativePostprocessTextureAspect aspect = NativePostprocessTextureAspect::Color;
	unsigned slice = 0;
	unsigned level = 0;

	bool Valid() const noexcept
	{
		return version && storage && object && generation && producer && event &&
			slice == 0 && level == 0;
	}

	bool operator==(const NativePostprocessTextureReceipt&) const = default;
};
}
