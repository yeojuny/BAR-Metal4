/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "AssetData.h"
#include "../../rts/Rendering/Models/S3OData.h"

#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0;

void Check(bool value, const char* message)
{
	++checks;
	if (!value) throw std::runtime_error(message);
}

void Put(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
	for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = uint8_t(value >> (i * 8));
}

void Float(std::vector<uint8_t>& bytes, size_t offset, float value)
{
	Put(bytes, offset, std::bit_cast<uint32_t>(value));
}

std::vector<uint8_t> S3O(uint32_t primitive, const std::vector<uint32_t>& indices)
{
	std::vector<uint8_t> bytes(240 + indices.size() * 4);
	std::memcpy(bytes.data(), "Spring unit", 12);
	Put(bytes, 36, 52);
	Put(bytes, 52, 104);
	Put(bytes, 64, 4);
	Put(bytes, 68, 112);
	Put(bytes, 76, primitive);
	Put(bytes, 80, indices.size());
	Put(bytes, 84, 240);
	std::memcpy(bytes.data() + 104, "root", 5);
	for (unsigned i = 0; i < 4; ++i) {
		Float(bytes, 112 + i * 32, float(i % 2));
		Float(bytes, 116 + i * 32, float(i / 2));
		Float(bytes, 128 + i * 32, 1.0f); // Unit +Y normal.
		Float(bytes, 136 + i * 32, float(i % 2));
		Float(bytes, 140 + i * 32, float(i / 2));
	}
	for (size_t i = 0; i < indices.size(); ++i) Put(bytes, 240 + i * 4, indices[i]);
	return bytes;
}

std::vector<uint8_t> DDS(uint32_t width = 4, uint32_t height = 4)
{
	std::vector<uint8_t> bytes(144);
	Put(bytes, 0, 0x20534444);
	Put(bytes, 4, 124);
	Put(bytes, 8, 0x1007);
	Put(bytes, 12, height);
	Put(bytes, 16, width);
	Put(bytes, 28, 1);
	Put(bytes, 76, 32);
	Put(bytes, 80, 4);
	Put(bytes, 84, 0x35545844);
	Put(bytes, 108, 0x1000);
	bytes[128] = 255;
	bytes[129] = 0;
	// First pixel endpoint 0; second pixel endpoint 1. Remaining pixels endpoint 0.
	bytes[130] = 8;
	bytes[136] = 0;
	bytes[137] = 248; // Red RGB565.
	bytes[138] = 224;
	bytes[139] = 7; // Green RGB565.
	bytes[140] = 4;
	return bytes;
}

std::vector<uint8_t> EmptyS3O()
{
	auto bytes = S3O(0, {});
	Put(bytes, 56, 0); Put(bytes, 60, UINT32_MAX);
	Put(bytes, 64, 0); Put(bytes, 68, UINT32_MAX);
	Put(bytes, 80, 0); Put(bytes, 84, UINT32_MAX);
	return bytes;
}

std::vector<uint8_t> Chain(unsigned depth)
{
	auto bytes = EmptyS3O();
	const std::vector<uint8_t> piece(bytes.begin() + 52, bytes.begin() + 104);
	std::vector<uint32_t> offsets = {52};
	for (unsigned i = 0; i < depth; ++i) {
		offsets.push_back(bytes.size());
		bytes.insert(bytes.end(), piece.begin(), piece.end());
	}
	for (unsigned i = 0; i < depth; ++i) {
		Put(bytes, offsets[i] + 4, 1);
		Put(bytes, offsets[i] + 8, bytes.size());
		const auto table = bytes.size();
		bytes.resize(bytes.size() + 4);
		Put(bytes, table, offsets[i + 1]);
	}
	return bytes;
}

void Write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
	std::ofstream file(path, std::ios::binary);
	file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	Check(bool(file), "Fixture write failed");
}

template<typename F> void Reject(F action)
{
	bool rejected = false;
	try { action(); } catch (const std::runtime_error&) { rejected = true; }
	Check(rejected, "Malformed asset was not rejected");
}

class FixtureDirectory {
public:
	explicit FixtureDirectory(const std::filesystem::path& root)
	{
		path = root / ("asset-probe-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::filesystem::create_directories(path);
	}
	~FixtureDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
	std::filesystem::path path;
};
} // namespace

int main(int argc, char** argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Usage: AssetDataTests WORK_DIRECTORY");
		FixtureDirectory directory(argv[1]);
		const auto modelPath = directory.path / "fixture.s3o";
		const auto texturePath = directory.path / "fixture.dds";
		for (size_t field: {size_t(0), size_t(12), size_t(56), size_t(64), size_t(80), size_t(68), size_t(72), size_t(76)}) {
			auto malformed = S3O(0, {0, 1, 2});
			Put(malformed, field, UINT32_MAX);
			Reject([&] { s3o::Decode(malformed); });
		}
		for (size_t field: {size_t(16), size_t(20), size_t(24), size_t(92), size_t(112), size_t(136)}) {
			auto malformed = S3O(0, {0, 1, 2});
			Put(malformed, field, 0x7F800000);
			Reject([&] { s3o::Decode(malformed); });
		}
		auto unterminated = S3O(0, {0, 1, 2});
		Put(unterminated, 52, unterminated.size() - 1);
		unterminated.back() = 'x';
		Reject([&] { s3o::Decode(unterminated); });
		const auto empty = s3o::Decode(EmptyS3O());
		Check(empty.pieces.size() == 1 && empty.pieces[0].vertices.empty() && empty.pieces[0].indices.empty() && empty.pieces[0].children.empty(), "Shared empty unused-table offsets failed");
		Check(s3o::Decode(Chain(256)).pieces.size() == 257, "Shared maximum hierarchy depth should be accepted");
		Reject([&] { s3o::Decode(Chain(257)); });
		auto bytes = S3O(0, {0, 1, 2});
		Float(bytes, 16, 12.5f); Float(bytes, 20, 20.0f);
		Float(bytes, 24, 1.0f); Float(bytes, 28, 2.0f); Float(bytes, 32, 3.0f);
		Write(modelPath, bytes);
		auto model = metal4probe::LoadS3O(modelPath);
		const auto decoded = s3o::Decode(bytes);
		Check(decoded.radius == 12.5f && decoded.height == 20.0f && decoded.midpoint == std::array<float, 3>({1, 2, 3}), "Shared header metadata transport failed");
		Check(model.sourceRadius == decoded.radius && model.sourceHeight == decoded.height && model.sourceMidpoint == decoded.midpoint, "Diagnostic shared-header adapter failed");
		Check(decoded.pieces[0].indices == std::vector<uint32_t>({0, 1, 2}), "Shared decoder mutated source triangle indices");
		Check(model.pieces.size() == 1 && model.vertices.size() == 4 && model.indices.size() == 3, "Triangle fixture shape failed");
		Check(model.vertices[1].normal[1] == 1 && model.vertices[1].uv[0] == 1, "Normal/UV transport failed");
		bytes = S3O(2, {0, 1, 2, 3});
		Write(modelPath, bytes);
		model = metal4probe::LoadS3O(modelPath);
		Check(model.indices == std::vector<uint32_t>({0, 1, 2, 0, 2, 3}), "Pinned quad diagonal/order failed");
		bytes = S3O(1, {0, 1, 2, UINT32_MAX, 0, 2, 3});
		Write(modelPath, bytes);
		model = metal4probe::LoadS3O(modelPath);
		Check(model.stripZeroPrefixTriangles == 7 && model.indices.size() == 27 && model.degenerateTriangles == 7, "Pinned strip prefix/restart semantics failed");
		Check(std::vector<uint32_t>(model.indices.begin() + 21, model.indices.end()) == std::vector<uint32_t>({0, 1, 2, 0, 2, 3}), "Strip restart crossed boundary");
		// A root with no mesh and intentionally unusable zero-count offsets, plus one child mesh.
		bytes = S3O(0, {0, 1, 2});
		const uint32_t childOffset = bytes.size();
		std::vector<uint8_t> child(bytes.begin() + 52, bytes.begin() + 104);
		bytes.insert(bytes.end(), child.begin(), child.end());
		const uint32_t childTable = bytes.size();
		bytes.resize(bytes.size() + 4);
		Put(bytes, childTable, childOffset);
		Put(bytes, 56, 1);
		Put(bytes, 60, childTable);
		Put(bytes, 64, 0); Put(bytes, 68, UINT32_MAX);
		Put(bytes, 80, 0); Put(bytes, 84, UINT32_MAX);
		Float(bytes, 92, 4); Float(bytes, 96, 5); Float(bytes, 100, 6);
		Float(bytes, childOffset + 40, 1); Float(bytes, childOffset + 44, 2); Float(bytes, childOffset + 48, 3);
		Write(modelPath, bytes);
		model = metal4probe::LoadS3O(modelPath);
		Check(model.pieces.size() == 2 && model.pieces[1].parent == 0, "Hierarchy parent failed");
		const auto hierarchy = s3o::Decode(bytes);
		Check(hierarchy.pieces[0].children == std::vector<uint32_t>({1}) && hierarchy.pieces[1].parent == 0, "Shared decoder hierarchy child/parent contract failed");
		Check(model.pieces[1].globalOffset == std::array<float, 3>({5, 7, 9}), "Hierarchy translation accumulation failed");
		Check(metal4probe::BuildRestPoseVertices(model)[0].position == std::array<float, 3>({5, 7, 9}), "Rest pose bake failed");
		Put(bytes, childTable, 52);
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		bytes = S3O(0, {0, 1, 8});
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		bytes = S3O(0, {0, 1, 2}); bytes.pop_back();
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		bytes = S3O(0, {0, 1, 2}); Put(bytes, 112, 0x7F800000);
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		bytes = S3O(0, {0, 1, 2}); Put(bytes, 124, 0x7FC01234);
		const auto invalidNormal = s3o::Decode(bytes);
		Check(std::bit_cast<uint32_t>(invalidNormal.pieces[0].vertices[0].normal[0]) == 0x7FC01234, "Shared decoder must retain invalid-normal bits for engine fallback");
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		uint32_t primitive = s3o::PRIMTYPE_TRIANGLE_STRIP;
		std::vector<uint32_t> shortStrip = {0, 1};
		s3o::Triangulate(primitive, shortStrip);
		Check(primitive == s3o::PRIMTYPE_TRIANGLES && shortStrip.empty(), "Shared short-strip clearing failed");
		primitive = s3o::PRIMTYPE_QUADS;
		std::vector<uint32_t> malformedQuad = {0, 1, 2};
		s3o::Triangulate(primitive, malformedQuad);
		Check(primitive == s3o::PRIMTYPE_TRIANGLES && malformedQuad.empty(), "Shared malformed-quad clearing failed");
		bytes = S3O(0, {0, 1});
		Reject([&] { s3o::Decode(bytes); });
		primitive = s3o::PRIMTYPE_TRIANGLES;
		std::vector<uint32_t> incompleteTriangles = {0, 1};
		s3o::Triangulate(primitive, incompleteTriangles);
		Check(primitive == s3o::PRIMTYPE_TRIANGLES && incompleteTriangles == std::vector<uint32_t>({0, 1}), "Shared primitive helper must preserve triangle-list passthrough");
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		// Distinct legal piece records alias one large vertex table. Expanded geometry
		// must be bounded by the shared decoder's input-relative allocation policy.
		bytes = S3O(0, {0, 1, 2});
		const uint32_t aliasedVertices = bytes.size();
		bytes.resize(bytes.size() + 512 * 32);
		for (unsigned i = 0; i < 512; ++i) Float(bytes, aliasedVertices + i * 32 + 16, 1.0f);
		Put(bytes, 64, 512); Put(bytes, 68, aliasedVertices);
		const std::vector<uint8_t> aliasPiece(bytes.begin() + 52, bytes.begin() + 104);
		std::vector<uint32_t> aliasChildren;
		for (unsigned i = 0; i < 8; ++i) {
			aliasChildren.push_back(bytes.size());
			bytes.insert(bytes.end(), aliasPiece.begin(), aliasPiece.end());
		}
		const uint32_t aliasTable = bytes.size();
		bytes.resize(bytes.size() + aliasChildren.size() * 4);
		for (size_t i = 0; i < aliasChildren.size(); ++i) Put(bytes, aliasTable + i * 4, aliasChildren[i]);
		Put(bytes, 56, aliasChildren.size()); Put(bytes, 60, aliasTable);
		Reject([&] { s3o::Decode(bytes); });
		Write(modelPath, bytes); Reject([&] { metal4probe::LoadS3O(modelPath); });
		// A long aliased piece name also consumes the shared payload budget.
		bytes = S3O(0, {0, 1, 2});
		const auto longName = bytes.size();
		bytes.resize(bytes.size() + 1025, 'n'); bytes.back() = 0;
		Put(bytes, 52, longName);
		const std::vector<uint8_t> namedPiece(bytes.begin() + 52, bytes.begin() + 104);
		aliasChildren.clear();
		for (unsigned i = 0; i < 8; ++i) {
			aliasChildren.push_back(bytes.size());
			bytes.insert(bytes.end(), namedPiece.begin(), namedPiece.end());
		}
		const auto namedTable = bytes.size();
		bytes.resize(bytes.size() + aliasChildren.size() * 4);
		for (size_t i = 0; i < aliasChildren.size(); ++i) Put(bytes, namedTable + i * 4, aliasChildren[i]);
		Put(bytes, 56, aliasChildren.size()); Put(bytes, 60, namedTable);
		Reject([&] { s3o::Decode(bytes); });
		// Reusing one broad subtree must account for each instance's child storage.
		bytes = EmptyS3O();
		const std::vector<uint8_t> emptyPiece(bytes.begin() + 52, bytes.begin() + 104);
		const auto branch = bytes.size(); bytes.insert(bytes.end(), emptyPiece.begin(), emptyPiece.end());
		const auto leaf = bytes.size(); bytes.insert(bytes.end(), emptyPiece.begin(), emptyPiece.end());
		const auto broadTable = bytes.size(); bytes.resize(bytes.size() + 1000 * 4);
		for (unsigned i = 0; i < 1000; ++i) Put(bytes, broadTable + i * 4, leaf);
		Put(bytes, branch + 4, 1000); Put(bytes, branch + 8, broadTable);
		const auto rootTable = bytes.size(); bytes.resize(bytes.size() + 4 * 4);
		for (unsigned i = 0; i < 4; ++i) Put(bytes, rootTable + i * 4, branch);
		Put(bytes, 56, 4); Put(bytes, 60, rootTable);
		Reject([&] { s3o::Decode(bytes); });
		bytes = DDS(); Write(texturePath, bytes);
		auto texture = metal4probe::LoadDXT5DDS(texturePath);
		auto rgba = metal4probe::DecodeDXT5Mip(texture, 0);
		Check(rgba.size() == 64 && rgba[0] == 255 && rgba[1] == 0 && rgba[3] == 255, "DXT5 red/opaque endpoint failed");
		Check(rgba[4] == 0 && rgba[5] == 255 && rgba[7] == 0, "DXT5 green/transparent endpoint failed");
		bytes[128] = 10; bytes[129] = 20; bytes[130] = 6 | (7 << 3);
		Write(texturePath, bytes); texture = metal4probe::LoadDXT5DDS(texturePath); rgba = metal4probe::DecodeDXT5Mip(texture, 0);
		Check(rgba[3] == 0 && rgba[7] == 255, "DXT5 alternate alpha-palette sentinel failed");
		bytes = DDS(2, 2); Write(texturePath, bytes); texture = metal4probe::LoadDXT5DDS(texturePath);
		Check(metal4probe::DecodeDXT5Mip(texture, 0).size() == 16, "Partial DXT block cropping failed");
		Reject([&] { metal4probe::DecodeDXT5Mip(texture, 1); });
		bytes = DDS(); bytes.pop_back(); Write(texturePath, bytes); Reject([&] { metal4probe::LoadDXT5DDS(texturePath); });
		bytes = DDS(); bytes.push_back(0); Write(texturePath, bytes); Reject([&] { metal4probe::LoadDXT5DDS(texturePath); });
		bytes = DDS(); Put(bytes, 28, 8); Write(texturePath, bytes); Reject([&] { metal4probe::LoadDXT5DDS(texturePath); });
		bytes = DDS(); Put(bytes, 112, 0x200); Write(texturePath, bytes); Reject([&] { metal4probe::LoadDXT5DDS(texturePath); });
		std::cout << "Asset boundary tests passed: " << checks << " checks\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "Asset boundary test failed: " << error.what() << '\n';
		return 1;
	}
}
