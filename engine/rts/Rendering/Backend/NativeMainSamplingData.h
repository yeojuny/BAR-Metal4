/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace rendering {

// Startup-only selection. A capability candidate is not an allocated framebuffer.
// The renderer publishes this state only after color/depth/stencil allocation.
struct NativeMainSampling {

	std::uint32_t requested = 0;
	std::uint32_t effective = 0; // Original msaaLevel convention: zero means OFF.
	std::uint32_t samples = 1; // Actual texture/PSO count, always at least one.
	std::uint64_t allocationGeneration = 0;
};

inline std::array<std::uint32_t, 7> NativeMainSampleCandidates(int requested)
{
	// Match CreateWindowAndContext's even request followed by CreateSDLWindow's
	// successive halves. Do not turn an unsupported request into a larger one.
	if (requested < 0 || requested > 32)
		throw std::invalid_argument("Original main MSAA request is outside MSAALevel bounds");
	const auto normalized = std::uint32_t(requested + (requested & 1));
	return {normalized, normalized / 2, normalized / 4, normalized / 8,
		normalized / 16, normalized / 32, 0};
}

inline bool NativeSampleCountSupported(std::uint64_t supportedMask, std::uint32_t samples)
{
	return samples != 0 && samples < 64 && (supportedMask & (std::uint64_t{1} << samples)) != 0;
}

} // namespace rendering
