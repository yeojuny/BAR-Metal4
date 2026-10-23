/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "../../rts/Rendering/Backend/AssetDiagnosticTypes.h"

// CPU-only diagnostic boundary. No GL, Metal, filesystem singleton or GPU handle.
// Raw S3O decoding and triangulation are shared with the engine through S3OData.
// Normal preparation, texture loading and upload remain diagnostic adapters.
namespace metal4probe {

using Vertex = rendering::AssetDiagnosticVertex;

struct Piece {
	std::string name;
	int32_t parent = -1;
	std::array<float, 3> offset{};
	std::array<float, 3> globalOffset{};
	uint32_t firstVertex = 0;
	uint32_t vertexCount = 0;
	uint32_t firstIndex = 0;
	uint32_t indexCount = 0;
	uint32_t sourcePrimitive = 0;
};

struct ModelData {
	std::string texture1;
	std::string texture2;
	float sourceRadius = 0;
	float sourceHeight = 0;
	std::array<float, 3> sourceMidpoint{};
	std::vector<Vertex> vertices; // Local positions; hierarchy translations remain explicit.
	std::vector<uint32_t> indices; // Global vertex indices, pinned parser triangulation.
	std::vector<Piece> pieces; // Parent precedes children.
	std::array<float, 3> minimum{};
	std::array<float, 3> maximum{};
	uint32_t stripZeroPrefixTriangles = 0;
	uint32_t degenerateTriangles = 0;
	uint32_t zeroNormals = 0;
	std::vector<std::string> warnings;
};

struct TextureMip {
	uint32_t width = 0;
	uint32_t height = 0;
	size_t offset = 0;
	size_t byteCount = 0;
};

struct TextureData {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> ddsBytes;
	std::vector<TextureMip> mips;
	// Original DDS row order; S3O unit textures sample (u, 1-v), matching Bitmap's flipDDS.
	std::string format = "DXT5 / BC3 RGBA";
};

ModelData LoadS3O(const std::filesystem::path& path);
TextureData LoadDXT5DDS(const std::filesystem::path& path);
std::vector<uint8_t> DecodeDXT5Mip(const TextureData& texture, size_t mipIndex);
std::vector<Vertex> BuildRestPoseVertices(const ModelData& model);

} // namespace metal4probe
