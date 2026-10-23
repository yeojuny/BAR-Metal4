/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Raw CPU S3O data shared by the engine adapter and renderer diagnostics.
// No engine globals, graphics API types, normal preparation or texture loading.
namespace s3o {

enum PrimitiveType : uint32_t {
	PRIMTYPE_TRIANGLES = 0,
	PRIMTYPE_TRIANGLE_STRIP = 1,
	PRIMTYPE_QUADS = 2,
};

inline constexpr uint32_t NO_PARENT = UINT32_MAX;

struct DecodedVertex {
	std::array<float, 3> position{};
	std::array<float, 3> normal{}; // Raw bits, including invalid normals handled by the engine adapter.
	std::array<float, 2> uv{};
};

struct DecodedPiece {
	std::string name;
	uint32_t parent = NO_PARENT;
	std::vector<uint32_t> children;
	std::array<float, 3> offset{};
	uint32_t primitiveType = PRIMTYPE_TRIANGLES;
	std::vector<DecodedVertex> vertices;
	std::vector<uint32_t> indices; // Raw piece-local indices; UINT32_MAX is a strip restart.
};

struct DecodedModel {
	float radius = 0;
	float height = 0;
	std::array<float, 3> midpoint{};
	std::array<std::string, 2> textures;
	std::vector<DecodedPiece> pieces; // Preorder, root at zero; parents precede their children.
};

// Throws std::runtime_error for invalid structure or non-finite geometry fields.
// Empty tables ignore their unused file offsets, matching the pinned loader.
// Safety bounds: 65,534 pieces, root depth <= 256, and cumulative raw geometry,
// copied strings and reserved child indices <= 4x input bytes.
DecodedModel Decode(std::span<const uint8_t> bytes);

// Exact pinned primitive conversion, including the strip zero-prefix defect.
// Short strips and malformed quad lists are cleared. Triangle lists are untouched.
void Triangulate(uint32_t& primitiveType, std::vector<uint32_t>& indices);

} // namespace s3o
