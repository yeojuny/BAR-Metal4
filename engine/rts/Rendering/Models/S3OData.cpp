/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "S3OData.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace s3o {
namespace {

constexpr size_t HEADER_BYTES = 52;
constexpr size_t PIECE_BYTES = 52;
constexpr size_t VERTEX_BYTES = 32;
// Match the engine's uint16 piece-number format without importing engine types.
constexpr uint32_t MAX_PIECES = std::numeric_limits<uint16_t>::max() - 1;
constexpr unsigned MAX_DEPTH = 256;

class Reader {
public:
	explicit Reader(std::span<const uint8_t> bytes): bytes(bytes) {}

	void Range(size_t offset, size_t count) const
	{
		if (offset > bytes.size() || count > bytes.size() - offset)
			throw std::runtime_error("S3O range points outside file");
	}

	void Table(uint32_t offset, uint32_t count, size_t stride) const
	{
		// Widely used exporters leave invalid unused offsets on empty tables.
		if (count == 0)
			return;
		if (offset > bytes.size() || count > (bytes.size() - offset) / stride)
			throw std::runtime_error("S3O table range points outside file");
	}

	uint32_t U32(size_t offset) const
	{
		Range(offset, 4);
		return uint32_t(bytes[offset]) | (uint32_t(bytes[offset + 1]) << 8) |
			(uint32_t(bytes[offset + 2]) << 16) | (uint32_t(bytes[offset + 3]) << 24);
	}

	uint32_t Count(size_t offset) const
	{
		const uint32_t value = U32(offset);
		if (value > uint32_t(std::numeric_limits<int32_t>::max()))
			throw std::runtime_error("S3O table count is negative or unrepresentable");
		return value;
	}

	float RawFloat(size_t offset) const
	{
		return std::bit_cast<float>(U32(offset));
	}

	float FiniteFloat(size_t offset) const
	{
		const float value = RawFloat(offset);
		if (!std::isfinite(value))
			throw std::runtime_error("S3O contains non-finite geometry or header data");
		return value;
	}

	std::string_view String(uint32_t offset, bool optional = false) const
	{
		if (optional && offset == 0)
			return {};
		Range(offset, 1);
		const char* begin = reinterpret_cast<const char*>(bytes.data() + offset);
		const char* end = static_cast<const char*>(std::memchr(begin, 0, bytes.size() - offset));
		if (end == nullptr)
			throw std::runtime_error("S3O string is not terminated within file");
		return std::string_view(begin, static_cast<size_t>(end - begin));
	}

private:
	std::span<const uint8_t> bytes;
};

class Decoder {
public:
	explicit Decoder(std::span<const uint8_t> bytes): reader(bytes)
	{
		// Extent checks alone do not bound copies when many pieces alias a large
		// table or string. Bound payload storage before allocating it.
		dataBudget = (bytes.size() > std::numeric_limits<size_t>::max() / 4)
			? std::numeric_limits<size_t>::max() : bytes.size() * 4;
	}

	DecodedModel Load(std::span<const uint8_t> bytes)
	{
		reader.Range(0, HEADER_BYTES);
		constexpr char magic[] = "Spring unit";
		if (std::memcmp(bytes.data(), magic, sizeof(magic)) != 0 || reader.U32(12) != 0)
			throw std::runtime_error("S3O magic or version is unsupported");
		model.radius = reader.FiniteFloat(16);
		model.height = reader.FiniteFloat(20);
		for (size_t axis = 0; axis < model.midpoint.size(); ++axis)
			model.midpoint[axis] = reader.FiniteFloat(24 + axis * 4);
		model.textures[0] = ReadString(reader.U32(44), true);
		model.textures[1] = ReadString(reader.U32(48), true);
		LoadPiece(reader.U32(36), NO_PARENT, 0);
		return std::move(model);
	}

private:
	void AccountData(size_t count, size_t stride)
	{
		if (dataBytes > dataBudget || count > (dataBudget - dataBytes) / stride)
			throw std::runtime_error("S3O cumulative decoded payload exceeds 4x input-byte budget (table or string alias amplification)");
		dataBytes += count * stride;
	}

	std::string ReadString(uint32_t offset, bool optional = false)
	{
		const std::string_view value = reader.String(offset, optional);
		// Include terminator storage even for strings using the small-string buffer.
		AccountData(value.size() + 1, 1);
		return std::string(value);
	}

	uint32_t LoadPiece(uint32_t offset, uint32_t parent, unsigned depth)
	{
		if (depth > MAX_DEPTH || model.pieces.size() >= MAX_PIECES)
			throw std::runtime_error("S3O piece count or hierarchy depth exceeds safety bound");
		if (!activeOffsets.insert(offset).second)
			throw std::runtime_error("S3O piece hierarchy contains a cycle");
		reader.Range(offset, PIECE_BYTES);
		DecodedPiece piece;
		const size_t pieceOffset = offset;
		piece.name = ReadString(reader.U32(pieceOffset));
		piece.parent = parent;
		const uint32_t childCount = reader.Count(pieceOffset + 4);
		const uint32_t childrenOffset = reader.U32(pieceOffset + 8);
		const uint32_t vertexCount = reader.Count(pieceOffset + 12);
		const uint32_t verticesOffset = reader.U32(pieceOffset + 16);
		piece.primitiveType = reader.U32(pieceOffset + 24);
		const uint32_t indexCount = reader.Count(pieceOffset + 28);
		const uint32_t indicesOffset = reader.U32(pieceOffset + 32);
		if (reader.U32(pieceOffset + 20) != 0 || piece.primitiveType > PRIMTYPE_QUADS)
			throw std::runtime_error("S3O vertex or primitive type is unsupported");
		if (childCount >= MAX_PIECES)
			throw std::runtime_error("S3O child count exceeds piece format bound");
		if (piece.primitiveType == PRIMTYPE_TRIANGLES && indexCount % 3 != 0)
			throw std::runtime_error("S3O triangle list has incomplete triangles");
		if (piece.primitiveType == PRIMTYPE_TRIANGLE_STRIP && indexCount >= 3 && vertexCount == 0)
			throw std::runtime_error("S3O non-empty triangle strip has no vertices");
		reader.Table(childrenOffset, childCount, 4);
		reader.Table(verticesOffset, vertexCount, VERTEX_BYTES);
		reader.Table(indicesOffset, indexCount, 4);
		AccountData(vertexCount, VERTEX_BYTES);
		AccountData(indexCount, 4);
		AccountData(childCount, 4);
		for (size_t axis = 0; axis < piece.offset.size(); ++axis)
			piece.offset[axis] = reader.FiniteFloat(pieceOffset + 40 + axis * 4);

		piece.vertices.resize(vertexCount);
		for (uint32_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
			const size_t base = verticesOffset + size_t(vertexIndex) * VERTEX_BYTES;
			auto& vertex = piece.vertices[vertexIndex];
			for (size_t axis = 0; axis < vertex.position.size(); ++axis) {
				vertex.position[axis] = reader.FiniteFloat(base + axis * 4);
				// Keep invalid raw normal bits for the engine's existing zero fallback.
				vertex.normal[axis] = reader.RawFloat(base + 12 + axis * 4);
			}
			vertex.uv = {reader.FiniteFloat(base + 24), reader.FiniteFloat(base + 28)};
		}

		piece.indices.reserve(indexCount);
		for (uint32_t indexIndex = 0; indexIndex < indexCount; ++indexIndex) {
			const uint32_t index = reader.U32(indicesOffset + size_t(indexIndex) * 4);
			if (index == UINT32_MAX) {
				if (piece.primitiveType != PRIMTYPE_TRIANGLE_STRIP)
					throw std::runtime_error("S3O strip restart appears in non-strip primitive");
			} else if (index >= vertexCount) {
				throw std::runtime_error("S3O index references a missing local vertex");
			}
			piece.indices.push_back(index);
		}

		const uint32_t pieceIndex = static_cast<uint32_t>(model.pieces.size());
		piece.children.reserve(childCount);
		model.pieces.push_back(std::move(piece));
		for (uint32_t childIndex = 0; childIndex < childCount; ++childIndex) {
			const uint32_t child = LoadPiece(reader.U32(childrenOffset + size_t(childIndex) * 4), pieceIndex, depth + 1);
			// Recursion may reallocate model.pieces, so reacquire the parent by index.
			model.pieces[pieceIndex].children.push_back(child);
		}
		activeOffsets.erase(offset);
		return pieceIndex;
	}

	Reader reader;
	DecodedModel model;
	std::unordered_set<uint32_t> activeOffsets;
	size_t dataBudget = 0;
	size_t dataBytes = 0;
};

} // namespace

DecodedModel Decode(std::span<const uint8_t> bytes)
{
	return Decoder(bytes).Load(bytes);
}

void Triangulate(uint32_t& primitiveType, std::vector<uint32_t>& indices)
{
	switch (primitiveType) {
		case PRIMTYPE_TRIANGLES: {
		} break;
		case PRIMTYPE_TRIANGLE_STRIP: {
			if (indices.size() < 3) {
				primitiveType = PRIMTYPE_TRIANGLES;
				indices.clear();
				return;
			}

			std::vector<uint32_t> newIndices;
			newIndices.resize(indices.size() * 3); // Pinned zero prefix is intentional here.

			for (size_t i = 0; (i + 2) < indices.size(); ++i) {
				if (indices[i + 0] == UINT32_MAX || indices[i + 1] == UINT32_MAX || indices[i + 2] == UINT32_MAX)
					continue;

				newIndices.push_back(indices[i + 0]);
				newIndices.push_back(indices[i + 1]);
				newIndices.push_back(indices[i + 2]);
			}

			primitiveType = PRIMTYPE_TRIANGLES;
			indices.swap(newIndices);
		} break;
		case PRIMTYPE_QUADS: {
			if (indices.size() % 4 != 0) {
				primitiveType = PRIMTYPE_TRIANGLES;
				indices.clear();
				return;
			}

			std::vector<uint32_t> newIndices;
			const size_t oldCount = indices.size();
			newIndices.resize(oldCount + oldCount / 2);

			for (size_t i = 0, j = 0; i < indices.size(); i += 4) {
				newIndices[j++] = indices[i + 0];
				newIndices[j++] = indices[i + 1];
				newIndices[j++] = indices[i + 2];

				newIndices[j++] = indices[i + 0];
				newIndices[j++] = indices[i + 2];
				newIndices[j++] = indices[i + 3];
			}

			primitiveType = PRIMTYPE_TRIANGLES;
			indices.swap(newIndices);
		} break;
		default: {
		} break;
	}
}

} // namespace s3o
