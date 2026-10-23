/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "SMFTileData.h"

#include "SMFMapFile.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/Platform/byteorder.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>

namespace smf {
namespace {

void RequireBytes(CFileHandler& file, size_t bytes, const char* description)
{
	const int position = file.GetPos();
	const int size = file.FileSize();
	if (position < 0 || size < position || bytes > static_cast<size_t>(size - position))
		throw content_error(std::string("Truncated SMF/SMT ") + description);
}

void ReadExact(CFileHandler& file, void* destination, size_t bytes, const char* description)
{
	RequireBytes(file, bytes, description);
	if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()) ||
		file.Read(destination, static_cast<int>(bytes)) != static_cast<int>(bytes))
		throw content_error(std::string("Failed reading SMF/SMT ") + description);
}

uint32_t ReadCount(CFileHandler& file)
{
	uint32_t value = 0;
	ReadExact(file, &value, sizeof(value), "tile count");
	swabDWordInPlace(value);
	if (value > static_cast<uint32_t>(std::numeric_limits<int>::max()))
		throw content_error("Negative SMF/SMT tile count");
	return value;
}

std::string ReadFilename(CFileHandler& file)
{
	std::string name;
	for (size_t i = 0; i < 255; ++i) {
		char value = 0;
		ReadExact(file, &value, 1, "tile filename");
		if (value == 0)
			return name;
		name.push_back(value);
	}
	throw content_error("SMF tile filename exceeds its 255-byte field");
}

struct FileSpec {
	uint32_t count = 0;
	std::string path;
	std::unique_ptr<CFileHandler> file;
	bool missing = false;
};

} // namespace

TileData LoadTileData(CSMFMapFile& mapFile, const std::string& mapPath,
	std::span<const std::string> filenameOverrides, MissingTilePolicy missingPolicy)
{
	static_assert(SMALL_TILE_SIZE == 680);
	const SMFHeader& header = mapFile.GetHeader();
	if (header.mapx <= 0 || header.mapy <= 0 || header.mapx % 4 != 0 || header.mapy % 4 != 0)
		throw content_error("Invalid SMF terrain dimensions");
	if (header.tilesize != 32 || header.texelPerSquare != 8 || header.squareSize != 8)
		throw content_error("Unsupported SMF terrain tile layout");
	CFileHandler& source = *mapFile.GetFileHandler();
	if (header.tilesPtr < 0 || header.tilesPtr > source.FileSize())
		throw content_error("SMF tile table offset is outside the map file");
	source.Seek(header.tilesPtr);
	RequireBytes(source, 8, "tile header");
	MapTileHeader tileHeader;
	CSMFMapFile::ReadMapTileHeader(tileHeader, source);
	if (tileHeader.numTileFiles < 0 || tileHeader.numTiles <= 0)
		throw content_error("Invalid SMF tile header counts");

	TileData data;
	data.gridWidth = static_cast<uint32_t>(header.mapx / 4);
	data.gridHeight = static_cast<uint32_t>(header.mapy / 4);
	data.uniqueTileCount = static_cast<uint32_t>(tileHeader.numTiles);
	data.declaredFileCount = static_cast<uint32_t>(tileHeader.numTileFiles);
	const size_t indexCount = static_cast<size_t>(data.gridWidth) * data.gridHeight;
	if (indexCount > std::numeric_limits<size_t>::max() / sizeof(uint32_t) ||
		SMALL_TILE_SIZE > std::numeric_limits<size_t>::max() / static_cast<size_t>(data.uniqueTileCount))
		throw content_error("SMF tile allocation size overflow");
	const size_t indexBytes = indexCount * sizeof(uint32_t);
	RequireBytes(source, indexBytes, "tile indices");
	const size_t remaining = static_cast<size_t>(source.FileSize() - source.GetPos());
	if (data.declaredFileCount > (remaining - indexBytes) / 5)
		throw content_error("SMF tile file count exceeds the table extent");
	const bool overrides = !filenameOverrides.empty() && filenameOverrides.size() == data.declaredFileCount;
	data.ignoredFilenameOverrides = !filenameOverrides.empty() && !overrides;
	const std::string directory = FileSystem::GetDirectory(mapPath);
	std::vector<FileSpec> files;
	files.reserve(data.declaredFileCount);
	uint32_t totalTiles = 0;
	for (uint32_t i = 0; i < data.declaredFileCount; ++i) {
		const uint32_t count = ReadCount(source);
		const std::string declaredName = ReadFilename(source);
		if (count > data.uniqueTileCount - totalTiles)
			throw content_error("SMT tile file counts exceed the SMF unique tile count");
		totalTiles += count;
		const std::string& name = overrides ? filenameOverrides[i] : declaredName;
		FileSpec spec;
		spec.count = count;
		spec.path = directory + name;
		spec.file = std::make_unique<CFileHandler>(spec.path);
		if (!spec.file->FileExists()) {
			spec.path = name;
			spec.file->Open(spec.path); // Preserve the original absolute/VFS fallback.
		}
		if (!spec.file->FileExists()) {
			if (missingPolicy == MissingTilePolicy::Fail)
				throw content_error("Missing original SMT tile file: " + spec.path);
			spec.missing = true;
			data.missingFiles.push_back({i, spec.path, count});
		} else {
			RequireBytes(*spec.file, 32, "SMT header");
			TileFileHeader tileFileHeader;
			CSMFMapFile::ReadMapTileFileHeader(tileFileHeader, *spec.file);
			constexpr std::array<char, 16> magic = {'s','p','r','i','n','g',' ','t','i','l','e','f','i','l','e',0};
			if (std::memcmp(tileFileHeader.magic, magic.data(), magic.size()) != 0 ||
				tileFileHeader.version != 1 || tileFileHeader.tileSize != 32 || tileFileHeader.compressionType != 1)
				throw content_error("Unsupported original SMT tile format: " + spec.path);
			RequireBytes(*spec.file, static_cast<size_t>(count) * SMALL_TILE_SIZE, "SMT tile payload");
		}
		data.sourcePaths.push_back(spec.path);
		files.push_back(std::move(spec));
	}
	if (totalTiles != data.uniqueTileCount)
		throw content_error("SMT file counts do not equal the SMF unique tile count");

	data.tileIndices.resize(indexCount);
	ReadExact(source, data.tileIndices.data(), indexBytes, "tile index table");
	for (uint32_t& index : data.tileIndices) {
		swabDWordInPlace(index);
		if (index >= data.uniqueTileCount)
			throw content_error("SMF tile index exceeds the unique tile count");
	}
	// Present SMT extents are validated before the combined output allocation.
	data.bc1Bytes.resize(static_cast<size_t>(data.uniqueTileCount) * SMALL_TILE_SIZE);
	size_t offset = 0;
	for (const auto& spec : files) {
		const size_t bytes = static_cast<size_t>(spec.count) * SMALL_TILE_SIZE;
		if (spec.missing)
			std::fill_n(data.bc1Bytes.data() + offset, bytes, uint8_t{0xaa});
		else
			ReadExact(*spec.file, data.bc1Bytes.data() + offset, bytes, "SMT tiles");
		offset += bytes;
	}
	return data;
}

} // namespace smf
