#include "ETag.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>

#include "FileSystem/File.h"
#include "FileSystem/FileSystem.h"
#include "FileSystem/HashMD5.h"
#include "Logger.h"

std::optional<std::string> getETag(const std::string& file)
{
	auto etagFile = file + ".etag";
	if (!fileSystem->fileExists(file) || !fileSystem->fileExists(etagFile)) {
		return std::nullopt;
	}

	char data[IO_BUF_SIZE];
	FILE* f = fileSystem->propen(etagFile, "rb");
	if (f == nullptr) {
		return std::nullopt;
	}
	if (fgets(data, IO_BUF_SIZE, f) == NULL) {
		LOG_ERROR("Failed to read %s contents", etagFile.c_str());
		fclose(f);
		return std::nullopt;
	}
	fclose(f);
	char* colon = strchr(data, ':');
	if (colon == NULL) {
		LOG_ERROR("ETag file %s is in wrong format", etagFile.c_str());
		return std::nullopt;
	}

	HashMD5 fileHash;
	if (!fileSystem->hashFile(&fileHash, file)) {
		return std::nullopt;
	}
	if (fileHash.toString() != std::string(data, colon)) {
		return std::nullopt;
	}
	return std::string(colon + 1);
}

void setETag(const std::string& file, const std::string& value)
{
	auto etagFile = file + ".etag";
	if (value[0] != '"') {
		return;
	}
	HashMD5 fileHash;
	if (!fileSystem->hashFile(&fileHash, file)) {
		return;
	}
	CFile f;
	if (!f.Open(etagFile)) {
		return;
	}
	std::stringstream out;
	out << fileHash.toString() << ":" << value;
	if (!f.Write(out.str())) {
		f.Close(/*discard=*/true);
		return;
	}
	f.Close();
}
