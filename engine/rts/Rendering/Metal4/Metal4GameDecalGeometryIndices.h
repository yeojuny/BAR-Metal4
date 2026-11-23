/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rendering::metal4::gameDecalGeometry {
inline constexpr std::size_t VertexSlots = 96;

// Keep this in sync with the vertex-ID decoding in NativeGameDecalShaders.h.
// Index values are original GameDecalGeometryVertex vertex IDs, not dense IDs.
// A key describes both outputs of the old shader: regular grid location and
// tiny-quad corner (or the shared clip-out sentinel for all slots >= 6).
// Grid UVs and local coordinates are exact dyadics determined by this location.
constexpr std::array<unsigned, 3> VertexKey(unsigned slot) noexcept
{
	const unsigned strip = slot / 24;
	const unsigned triangle = (slot % 24) / 3;
	const unsigned corner = slot % 3;
	const unsigned emitted = triangle + (corner == 2 ? 2 : ((triangle & 1) ? 1 - corner : corner));
	return {emitted / 2, strip + (1 - (emitted & 1)), slot < 6 ? emitted : 4};
}

constexpr auto MakeIndices() noexcept
{
	std::array<std::uint16_t, VertexSlots> indices{};
	for (std::uint16_t slot = 0; slot < VertexSlots; ++slot) {
		const auto key = VertexKey(slot);
		std::uint16_t representative = 0;
		while (representative < slot && VertexKey(representative) != key)
			++representative;
		indices[slot] = representative;
	}
	return indices;
}

inline constexpr auto Indices = MakeIndices();
static_assert(Indices.size() == VertexSlots);
}
