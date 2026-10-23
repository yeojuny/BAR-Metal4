/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "AssetData.h"
#include "../../rts/Rendering/Models/S3OData.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace metal4probe {
namespace {

constexpr size_t MAX_FILE_BYTES = 128 * 1024 * 1024;
constexpr uint32_t MAX_VERTICES = 2'000'000;
constexpr uint32_t MAX_INDICES = 12'000'000;
constexpr uint32_t MAX_PIECES = 4096;
constexpr uint32_t MAX_TEXTURE_DIMENSION = 8192;

std::vector<uint8_t> ReadBounded(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Cannot open asset: " + path.string());
	const auto size = file.tellg();
	if (size <= 0 || uint64_t(size) > MAX_FILE_BYTES)
		throw std::runtime_error("Asset size outside diagnostic bounds: " + path.string());
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
		throw std::runtime_error("Cannot read complete asset: " + path.string());
	return bytes;
}

class Reader {
public:
	explicit Reader(const std::vector<uint8_t>& bytes): bytes(bytes) {}

	void Range(size_t offset, size_t count) const
	{
		if (offset > bytes.size() || count > bytes.size() - offset)
			throw std::runtime_error("Asset range points outside file");
	}

	uint32_t U32(size_t offset) const
	{
		Range(offset, 4);
		return uint32_t(bytes[offset]) | uint32_t(bytes[offset + 1]) << 8 |
			uint32_t(bytes[offset + 2]) << 16 | uint32_t(bytes[offset + 3]) << 24;
	}

	uint32_t Count(size_t offset, uint32_t maximum) const
	{
		const auto count = U32(offset);
		if (count > maximum)
			throw std::runtime_error("Asset count outside diagnostic bounds");
		return count;
	}

private:
	const std::vector<uint8_t>& bytes;
};

std::array<uint8_t, 3> Decode565(uint16_t color)
{
	return {uint8_t(((color >> 11) & 31) * 255 / 31), uint8_t(((color >> 5) & 63) * 255 / 63), uint8_t((color & 31) * 255 / 31)};
}

} // namespace

ModelData LoadS3O(const std::filesystem::path& path)
{
	const auto bytes = ReadBounded(path);
	const auto decoded = s3o::Decode(bytes);
	ModelData model;
	model.texture1 = decoded.textures[0];
	model.texture2 = decoded.textures[1];
	model.sourceRadius = decoded.radius;
	model.sourceHeight = decoded.height;
	model.sourceMidpoint = decoded.midpoint;
	model.minimum.fill(std::numeric_limits<float>::infinity());
	model.maximum.fill(-std::numeric_limits<float>::infinity());
	if (decoded.pieces.size() > MAX_PIECES)
		throw std::runtime_error("S3O exceeds diagnostic piece limit");
	model.pieces.reserve(decoded.pieces.size());
	for (const auto& source: decoded.pieces) {
		Piece piece;
		piece.name = source.name;
		piece.parent = source.parent == s3o::NO_PARENT ? -1 : int32_t(source.parent);
		piece.offset = source.offset;
		piece.sourcePrimitive = source.primitiveType;
		if (source.parent != s3o::NO_PARENT && source.parent >= model.pieces.size())
			throw std::runtime_error("Shared S3O decoder returned a non-preorder hierarchy");
		for (size_t axis = 0; axis < 3; ++axis) {
			piece.globalOffset[axis] = piece.offset[axis] + (piece.parent >= 0 ? model.pieces[piece.parent].globalOffset[axis] : 0);
			if (!std::isfinite(piece.globalOffset[axis]))
				throw std::runtime_error("S3O global transform overflow");
		}
		if (source.vertices.size() > MAX_VERTICES - model.vertices.size())
			throw std::runtime_error("S3O total diagnostic vertex limit exceeded");
		piece.firstVertex = uint32_t(model.vertices.size());
		piece.vertexCount = uint32_t(source.vertices.size());
		for (const auto& input: source.vertices) {
			Vertex vertex{input.position, input.normal, input.uv};
			float normalLengthSquared = 0;
			for (size_t axis = 0; axis < 3; ++axis) {
				if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
					throw std::runtime_error("S3O diagnostic rejects non-finite vertex data");
				normalLengthSquared += vertex.normal[axis] * vertex.normal[axis];
				const float world = vertex.position[axis] + piece.globalOffset[axis];
				if (!std::isfinite(world))
					throw std::runtime_error("S3O world bounds overflow");
				model.minimum[axis] = std::min(model.minimum[axis], world);
				model.maximum[axis] = std::max(model.maximum[axis], world);
			}
			if (!std::isfinite(normalLengthSquared))
				throw std::runtime_error("S3O normal magnitude overflow");
			if (normalLengthSquared > 0) {
				const float inverseLength = 1.0f / std::sqrt(normalLengthSquared);
				for (auto& component: vertex.normal)
					component *= inverseLength;
			} else {
				++model.zeroNormals;
			}
			if (!std::isfinite(vertex.uv[0]) || !std::isfinite(vertex.uv[1]))
				throw std::runtime_error("S3O diagnostic rejects non-finite UV data");
			model.vertices.push_back(vertex);
		}
		// Keep the probe's allocation policy separate from the shared engine decoder.
		// The conservative strip bound covers the pinned prefix plus every possible window.
		size_t indexUpperBound = source.indices.size();
		if (source.primitiveType == s3o::PRIMTYPE_TRIANGLE_STRIP)
			indexUpperBound = source.indices.size() < 3 ? 0 : source.indices.size() * 6;
		if (source.primitiveType == s3o::PRIMTYPE_QUADS) {
			indexUpperBound = source.indices.size() % 4 ? 0 : source.indices.size() / 4 * 6;
			if (source.indices.size() % 4)
				model.warnings.push_back("Discarded malformed quad index list in piece " + piece.name);
		}
		if (indexUpperBound > MAX_INDICES - model.indices.size())
			throw std::runtime_error("S3O triangulated diagnostic index limit exceeded");
		auto localIndices = source.indices;
		auto primitive = source.primitiveType;
		s3o::Triangulate(primitive, localIndices);
		if (primitive != s3o::PRIMTYPE_TRIANGLES || localIndices.size() % 3)
			throw std::runtime_error("S3O diagnostic requires complete triangulated primitives");
		if (source.primitiveType == s3o::PRIMTYPE_TRIANGLE_STRIP && source.indices.size() >= 3)
			model.stripZeroPrefixTriangles += uint32_t(source.indices.size());
		piece.firstIndex = uint32_t(model.indices.size());
		for (size_t i = 0; i < localIndices.size(); i += 3) {
			const auto a = localIndices[i];
			const auto b = localIndices[i + 1];
			const auto c = localIndices[i + 2];
			if (a >= piece.vertexCount || b >= piece.vertexCount || c >= piece.vertexCount)
				throw std::runtime_error("S3O triangulated index references missing local vertex");
			model.indices.insert(model.indices.end(), {a + piece.firstVertex, b + piece.firstVertex, c + piece.firstVertex});
			model.degenerateTriangles += (a == b || a == c || b == c);
		}
		piece.indexCount = uint32_t(model.indices.size()) - piece.firstIndex;
		model.pieces.push_back(std::move(piece));
	}
	if (model.vertices.empty() || model.indices.empty())
		throw std::runtime_error("S3O has no diagnostic drawable geometry");
	return model;
}

TextureData LoadDXT5DDS(const std::filesystem::path& path)
{
	TextureData result;
	result.ddsBytes = ReadBounded(path);
	Reader reader(result.ddsBytes);
	reader.Range(0, 128);
	if (reader.U32(0) != 0x20534444 || reader.U32(4) != 124 || reader.U32(76) != 32 ||
		!(reader.U32(80) & 4) || reader.U32(84) != 0x35545844)
		throw std::runtime_error("DDS diagnostic requires original legacy DXT5/BC3 RGBA format");
	const auto flags = reader.U32(8);
	if ((flags & 0x1007) != 0x1007 || !(reader.U32(108) & 0x1000))
		throw std::runtime_error("DDS missing mandatory 2D texture flags");
	if (reader.U32(112) != 0 || reader.U32(24) > 1 || (flags & 0x800000))
		throw std::runtime_error("DDS cubemap, volume and arrays are outside diagnostic scope");
	result.height = reader.Count(12, MAX_TEXTURE_DIMENSION);
	result.width = reader.Count(16, MAX_TEXTURE_DIMENSION);
	if (!result.width || !result.height)
		throw std::runtime_error("DDS dimensions are zero");
	uint32_t levels = std::max(1U, reader.Count(28, 14));
	uint32_t maximumLevels = 1;
	for (uint32_t size = std::max(result.width, result.height); size > 1; size >>= 1)
		++maximumLevels;
	if (levels > maximumLevels)
		throw std::runtime_error("DDS mip count exceeds dimensions");
	size_t offset = 128;
	uint32_t width = result.width;
	uint32_t height = result.height;
	size_t decodedTotal = 0;
	for (uint32_t level = 0; level < levels; ++level) {
		const size_t byteCount = size_t((width + 3) / 4) * ((height + 3) / 4) * 16;
		reader.Range(offset, byteCount);
		result.mips.push_back({width, height, offset, byteCount});
		offset += byteCount;
		decodedTotal += size_t(width) * height * 4;
		width = std::max(1U, width / 2);
		height = std::max(1U, height / 2);
	}
	if (decodedTotal > 384 * 1024 * 1024)
		throw std::runtime_error("DDS decoded mip storage exceeds diagnostic bound");
	if (offset != result.ddsBytes.size())
		throw std::runtime_error("DDS payload length does not exactly match declared mips");
	return result;
}

std::vector<uint8_t> DecodeDXT5Mip(const TextureData& texture, size_t mipIndex)
{
	if (mipIndex >= texture.mips.size())
		throw std::runtime_error("DDS mip index out of range");
	const auto& mip = texture.mips[mipIndex];
	Reader(texture.ddsBytes).Range(mip.offset, mip.byteCount);
	if (!mip.width || !mip.height || mip.width > MAX_TEXTURE_DIMENSION || mip.height > MAX_TEXTURE_DIMENSION ||
		mip.byteCount != size_t((mip.width + 3) / 4) * ((mip.height + 3) / 4) * 16)
		throw std::runtime_error("DDS mip layout invalid");
	std::vector<uint8_t> rgba(size_t(mip.width) * mip.height * 4);
	const uint32_t blocksWide = (mip.width + 3) / 4;
	const uint32_t blocksHigh = (mip.height + 3) / 4;
	for (uint32_t by = 0; by < blocksHigh; ++by) {
		for (uint32_t bx = 0; bx < blocksWide; ++bx) {
			const auto* block = texture.ddsBytes.data() + mip.offset + (size_t(by) * blocksWide + bx) * 16;
			std::array<uint8_t, 8> alpha = {block[0], block[1]};
			if (alpha[0] > alpha[1]) {
				for (unsigned i = 1; i <= 6; ++i)
					alpha[i + 1] = uint8_t(((7 - i) * alpha[0] + i * alpha[1]) / 7);
			} else {
				for (unsigned i = 1; i <= 4; ++i)
					alpha[i + 1] = uint8_t(((5 - i) * alpha[0] + i * alpha[1]) / 5);
				alpha[6] = 0;
				alpha[7] = 255;
			}
			uint64_t alphaBits = 0;
			for (unsigned i = 0; i < 6; ++i)
				alphaBits |= uint64_t(block[2 + i]) << (i * 8);
			const uint16_t color0 = uint16_t(block[8]) | uint16_t(block[9]) << 8;
			const uint16_t color1 = uint16_t(block[10]) | uint16_t(block[11]) << 8;
			std::array<std::array<uint8_t, 3>, 4> colors = {Decode565(color0), Decode565(color1)};
			// BC3's color block always uses four-color interpolation, irrespective of endpoint order.
			for (unsigned channel = 0; channel < 3; ++channel) {
				colors[2][channel] = uint8_t((2 * colors[0][channel] + colors[1][channel]) / 3);
				colors[3][channel] = uint8_t((colors[0][channel] + 2 * colors[1][channel]) / 3);
			}
			const uint32_t colorBits = uint32_t(block[12]) | uint32_t(block[13]) << 8 |
				uint32_t(block[14]) << 16 | uint32_t(block[15]) << 24;
			for (unsigned pixel = 0; pixel < 16; ++pixel) {
				const uint32_t x = bx * 4 + pixel % 4;
				const uint32_t y = by * 4 + pixel / 4;
				if (x >= mip.width || y >= mip.height)
					continue;
				const auto& color = colors[(colorBits >> (pixel * 2)) & 3];
				const size_t destination = (size_t(y) * mip.width + x) * 4;
				std::copy(color.begin(), color.end(), rgba.begin() + destination);
				rgba[destination + 3] = alpha[(alphaBits >> (pixel * 3)) & 7];
			}
		}
	}
	return rgba;
}

std::vector<Vertex> BuildRestPoseVertices(const ModelData& model)
{
	auto vertices = model.vertices;
	for (const auto& piece: model.pieces) {
		if (piece.firstVertex > vertices.size() || piece.vertexCount > vertices.size() - piece.firstVertex)
			throw std::runtime_error("Model piece vertex range invalid");
		for (uint32_t i = 0; i < piece.vertexCount; ++i)
			for (size_t axis = 0; axis < 3; ++axis)
				vertices[piece.firstVertex + i].position[axis] += piece.globalOffset[axis];
	}
	return vertices;
}

} // namespace metal4probe
