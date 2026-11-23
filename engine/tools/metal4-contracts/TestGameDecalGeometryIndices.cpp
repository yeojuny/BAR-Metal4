/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Metal4/Metal4GameDecalGeometryIndices.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

namespace {
namespace topology = rendering::metal4::gameDecalGeometry;
using Values = std::array<float, 4>;
using Stream = std::array<Values, 96>;
constexpr Values Clipped{2.f, 2.f, 2.f, 1.f};
unsigned checks = 0;

void Check(bool condition, const char* message)
{
	++checks;
	if (!condition) {
		std::fprintf(stderr, "FAIL %s (check %u)\n", message, checks);
		std::exit(1);
	}
}

bool Identical(const Values& a, const Values& b)
{
	using Bits = std::array<std::uint32_t, 4>;
	return std::bit_cast<Bits>(a) == std::bit_cast<Bits>(b);
}

// Independent oracle: construct four 10-vertex strips and turn them into
// ordered triangles; does not call VertexKey or use its integer coordinate key.
Stream GridOracle()
{
	Stream stream{};
	unsigned out = 0;
	for (unsigned strip = 0; strip < 4; ++strip) {
		std::array<Values, 10> points{};
		for (unsigned column = 0; column < 5; ++column) {
			const float u = 1.f - .25f * float(column);
			const float vl = .25f * float(strip);
			const float vh = .25f * float(strip + 1);
			points[2 * column] = {2.f * u - 1.f, 2.f * vh - 1.f, u, vh};
			points[2 * column + 1] = {2.f * u - 1.f, 2.f * vl - 1.f, u, vl};
		}
		for (unsigned tri = 0; tri < 8; ++tri) {
			stream[out++] = points[tri + (tri & 1)];
			stream[out++] = points[tri + ((tri & 1) ? 0 : 1)];
			stream[out++] = points[tri + 2];
		}
	}
	return stream;
}

Stream TinyOracle()
{
	Stream stream;
	stream.fill(Clipped);
	const Values upperLeft{1.f, 1.f, 1.f, 1.f}, lowerLeft{1.f, -1.f, 1.f, 0.f};
	const Values upperRight{-1.f, 1.f, 0.f, 1.f}, lowerRight{-1.f, -1.f, 0.f, 0.f};
	stream[0] = upperLeft;
	stream[1] = lowerLeft;
	stream[2] = upperRight;
	stream[3] = upperRight;
	stream[4] = lowerLeft;
	stream[5] = lowerRight;
	return stream;
}

// CPU transcription of the existing shader's vertex-ID logic, separate
// from the independently generated strips above. Alpha and sphere inputs
// here are the shader's post-calculation values, not a GPU math emulation.
Values OldShader(float width, float length, bool outside, float alpha, unsigned slot)
{
	unsigned skip = 0;
	if (width < 24.f && length < 24.f) skip = 2;
	if (outside) skip = 1;
	if (alpha < .01f) skip = 1;
	if (skip == 1 || (skip == 2 && slot >= 6)) return Clipped;
	const unsigned strip = slot / 24, triangle = (slot % 24) / 3, corner = slot % 3;
	const unsigned emitted = triangle + (corner == 2 ? 2 : ((triangle & 1) ? 1 - corner : corner));
	const unsigned column = emitted / 2;
	const bool bottom = emitted & 1;
	if (skip == 2) return {1.f - 2.f * float(column), bottom ? -1.f : 1.f, 1.f - float(column), bottom ? 0.f : 1.f};
	const float v = float(strip) * .25f;
	return {1.f - .5f * float(column), 2.f * v - (bottom ? 1.f : .5f),
		1.f - .25f * float(column), v + (bottom ? 0.f : .25f)};
}
}

int main()
{
	const auto grid = GridOracle();
	const auto tiny = TinyOracle();
	std::array<bool, 96> used{};
	Check(topology::Indices.size() == 96, "exactly 32 triangles");
	for (unsigned slot = 0; slot < 96; ++slot) {
		const unsigned index = topology::Indices[slot];
		Check(index <= slot && index < 96, "original vertex ID in range");
		Check(topology::Indices[index] == index, "representative is a fixed point");
		Check(Identical(grid[slot], grid[index]), "regular primitive slot (sequence, winding, local, UV)");
		Check(Identical(tiny[slot], tiny[index]), "tiny primitive slot (sequence, winding, local, UV, clip-out)");
		used[index] = true;
		for (unsigned other = 0; other < 96; ++other) {
			const bool equivalent = Identical(grid[slot], grid[other]) && Identical(tiny[slot], tiny[other]);
			Check((index == topology::Indices[other]) == equivalent, "partition is exact and fully deduplicated");
		}
	}
	unsigned unique = 0;
	for (const bool selected : used) unique += selected;
	Check(unique == 28, "28 exact normal/tiny equivalence classes");

	unsigned wrongTinySlots = 0;
	for (unsigned slot = 0; slot < 96; ++slot) {
		unsigned gridOnly = 0;
		while (!Identical(grid[gridOnly], grid[slot])) ++gridOnly;
		wrongTinySlots += !Identical(tiny[gridOnly], tiny[slot]);
	}
	Check(wrongTinySlots == 8, "negative control: merging by grid alone revives eight clipped tiny slots");

	const auto dimensions = std::array{
		0.f, 1.f, std::nextafter(24.f, 0.f), 24.f, std::nextafter(24.f, 100.f), 128.f,
		std::numeric_limits<float>::quiet_NaN()
	};
	const auto alphas = std::array{0.f, std::nextafter(.01f, 0.f), .01f, 1.f, std::numeric_limits<float>::quiet_NaN()};
	for (const auto width : dimensions)
		for (const auto length : dimensions)
			for (const bool outside : {false, true})
				for (const auto alpha : alphas)
					for (unsigned slot = 0; slot < 96; ++slot)
						Check(Identical(OldShader(width, length, outside, alpha, slot),
							OldShader(width, length, outside, alpha, topology::Indices[slot])), "all shader branch outcomes and threshold boundaries");
	std::printf("PASS GameDecalGeometryIndices checks=%u index_count=96 triangles=32 representatives=%u\n", checks, unique);
}
