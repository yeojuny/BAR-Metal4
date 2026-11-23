/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace rendering::metal4::triangleReuse {

// Keep each shader's original procedural vertex ID space. An index substitutes
// only an ordinal with the same shader inputs and outputs in that triangle slot.
template <std::size_t Segments>
constexpr auto RepeatSegment(std::array<std::uint16_t, 6> ordinal)
{
	static_assert(Segments > 0 && Segments * 6 <= 65536);
	std::array<std::uint16_t, Segments * 6> indices{};
	for (std::size_t segment = 0; segment < Segments; ++segment) {
		for (std::size_t i = 0; i < ordinal.size(); ++i)
			indices[segment * 6 + i] = static_cast<std::uint16_t>(segment * 6 + ordinal[i]);
	}
	return indices;
}

inline constexpr std::uint32_t maxSensorSegments = 1024;
inline constexpr auto sensorCircle = RepeatSegment<maxSensorSegments>({0, 1, 2, 0, 2, 5});

// R03 LOS chooses different winding maps for X-major and Y-major strips.
// Only slots 0 and 3 produce the same corner in all of its geometry branches.
inline constexpr auto sensorLOS = RepeatSegment<maxSensorSegments>({0, 1, 2, 0, 4, 5});

inline constexpr auto sensorLOSFan = [] {
	std::array<std::uint16_t, maxSensorSegments * 3> indices{};
	// Use the original center and the previous triangle's matching rim ID.
	for (std::size_t triangle = 0; triangle < maxSensorSegments; ++triangle) {
		const std::size_t first = triangle * 3;
		indices[first] = 0;
		indices[first + 1] = static_cast<std::uint16_t>(triangle == 0 ? 1 : first - 1);
		indices[first + 2] = static_cast<std::uint16_t>(first + 2);
	}
	return indices;
}();

// Each original face is a separate four-corner strip. Keep face IDs separate.
inline constexpr auto unitStencil = RepeatSegment<3>({0, 1, 2, 2, 1, 5});

constexpr std::size_t HealthStripCorner(std::size_t ordinal)
{
	const auto triangle = ordinal / 3;
	const auto corner = ordinal % 3;
	return corner == 2 ? triangle + 2 : triangle + (corner ^ (triangle & 1));
}

inline constexpr auto healthBars = [] {
	std::array<std::uint16_t, 78> indices{};
	// The first three strips have eight corners each, followed by four quads.
	for (std::size_t first = 0; first < indices.size();) {
		const std::size_t count = first < 54 ? 18 : 6;
		std::array<std::uint16_t, 8> firstOrdinal{};
		firstOrdinal.fill(UINT16_MAX);
		for (std::size_t i = 0; i < count; ++i) {
			auto& representative = firstOrdinal[HealthStripCorner(i)];
			if (representative == UINT16_MAX)
				representative = static_cast<std::uint16_t>(first + i);
			indices[first + i] = representative;
		}
		first += count;
	}
	return indices;
}();

} // namespace rendering::metal4::triangleReuse
