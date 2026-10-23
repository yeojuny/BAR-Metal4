/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SevenZipArchive.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <optional>

#include <7zAlloc.h>
#include <7zCrc.h>

#include "Logger.h"
#include "Util.h"

static Byte kUtf8Limits[5] = {0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

/**
 * Converts from UTF16 to UTF8. The destLen must be set to the size of the dest.
 * If the function succeeds, destLen contains the number of written bytes and dest
 * contains utf-8 \0 terminated string.
 */
static bool Utf16_To_Utf8(char* dest, size_t* destLen, const UInt16* src, size_t srcLen)
{
	size_t destPos = 0;
	size_t srcPos = 0;
	while (destPos < *destLen) {
		unsigned numAdds;
		UInt32 value;

		if (srcPos == srcLen) {
			dest[destPos] = '\0';
			*destLen = destPos;
			return true;
		}

		if ((value = src[srcPos++]) < 0x80) {
			dest[destPos++] = (char)value;
			continue;
		}

		if (value >= 0xD800 && value < 0xE000) {
			if (value >= 0xDC00 || srcPos == srcLen)
				break;

			const UInt32 c2 = src[srcPos++];

			if (c2 < 0xDC00 || c2 >= 0xE000)
				break;

			value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
		}
		for (numAdds = 1; numAdds < 5; numAdds++)
			if (value < (((UInt32)1) << (numAdds * 5 + 6)))
				break;

		dest[destPos++] = (char)(kUtf8Limits[numAdds - 1] + (value >> (6 * numAdds)));

		while (destPos < *destLen && numAdds > 0) {
			numAdds--;
			dest[destPos++] = (char)(0x80 + ((value >> (6 * numAdds)) & 0x3F));
		}
	}

	return false;
}

static std::optional<std::string> GetFileName(const CSzArEx* db, int i)
{
	constexpr size_t bufferSize = 2048;
	uint16_t utf16Buffer[bufferSize];
	char tempBuffer[bufferSize];

	// caller has archiveLock
	const size_t utf16len = SzArEx_GetFileNameUtf16(db, i, nullptr);
	if (utf16len >= bufferSize)
		return std::nullopt;

	SzArEx_GetFileNameUtf16(db, i, utf16Buffer);
	size_t tempBufferLen = bufferSize;
	if (!Utf16_To_Utf8(tempBuffer, &tempBufferLen, utf16Buffer, utf16len - 1)) {
		return std::nullopt;
	}

	return std::string(tempBuffer, tempBufferLen);
}

static inline const char* GetErrorStr(int err)
{
	switch (err) {
		case SZ_OK:
			return "OK";
		case SZ_ERROR_FAIL:
			return "Extracting failed";
		case SZ_ERROR_CRC:
			return "CRC error (archive corrupted?)";
		case SZ_ERROR_INPUT_EOF:
			return "Unexpected end of file (truncated?)";
		case SZ_ERROR_MEM:
			return "Out of memory";
		case SZ_ERROR_UNSUPPORTED:
			return "Unsupported archive";
		case SZ_ERROR_NO_ARCHIVE:
			return "Archive not found";
	}

	return "Unknown error";
}

CSevenZipArchive::CSevenZipArchive(const std::string& name)
	: IArchive(name)
	, allocImp({SzAlloc, SzFree})
	, allocTempImp({SzAllocTemp, SzFreeTemp})
{
	constexpr const size_t kInputBufSize = (size_t)1 << 18;

#ifdef _WIN32
	WRes wres = InFile_OpenW(&archiveStream.file, s2ws(name).c_str());
#else
	WRes wres = InFile_Open(&archiveStream.file, name.c_str());
#endif
	if (wres != 0) {
		LOG_ERROR("Error opening %s %s", name.c_str(), strerror(wres));
		return;
	}

	FileInStream_CreateVTable(&archiveStream);
	archiveStream.wres = 0;

	LookToRead2_CreateVTable(&lookStream, false);
	lookStream.realStream = &archiveStream.vt;
	lookStream.buf = static_cast<Byte*>(ISzAlloc_Alloc(&allocImp, kInputBufSize));
	assert(lookStream.buf != NULL);
	lookStream.bufSize = kInputBufSize;
	LookToRead2_Init(&lookStream);

	CrcGenerateTable();

	SzArEx_Init(&db);

	const SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
	if (res == SZ_OK) {
		isOpen = true;
	} else {
		LOG_ERROR("Error opening %s: %s", name.c_str(), GetErrorStr(res));
		return;
	}

	fileData.reserve(db.NumFiles);

	// Get contents of archive and store name->int mapping
	for (unsigned int i = 0; i < db.NumFiles; ++i) {
		if (SzArEx_IsDir(&db, i)) {
			continue;
		}

		auto fileName = GetFileName(&db, i);
		if (!fileName) {
			LOG_ERROR("Error getting filename in Archive, file skipped in %s", name.c_str());
			continue;
		}

		FileData fd;
		fd.origName = std::move(fileName.value());
		fd.fp = i;
		fd.size = SzArEx_GetFileSize(&db, i);
		if (SzBitWithVals_Check(&db.Attribs, i)) {
			// LOG_DEBUG("%s %d", fd.origName.c_str(), db.Attribs.Vals[i])
			if (db.Attribs.Vals[i] & 1 << 16)
				fd.mode = 0755;
			else
				fd.mode = 0644;
		}

		fileData.emplace_back(std::move(fd));
	}
}

CSevenZipArchive::~CSevenZipArchive()
{
	if (outBuffer != nullptr) {
		IAlloc_Free(&allocImp, outBuffer);
	}
	if (isOpen) {
		File_Close(&archiveStream.file);
	}
	ISzAlloc_Free(&allocImp, lookStream.buf);
	SzArEx_Free(&db, &allocImp);
}

unsigned int CSevenZipArchive::NumFiles() const
{
	return fileData.size();
}

bool CSevenZipArchive::GetFile(unsigned int fid, std::vector<unsigned char>& buffer)
{
	// Get 7zip to decompress it
	size_t offset;
	size_t outSizeProcessed;
	const SRes res =
		SzArEx_Extract(&db, &lookStream.vt, fileData[fid].fp, &blockIndex, &outBuffer,
	                   &outBufferSize, &offset, &outSizeProcessed, &allocImp, &allocTempImp);
	if (res == SZ_OK) {
		buffer.resize(outSizeProcessed);
		if (outSizeProcessed > 0) {
			memcpy(&buffer[0], (char*)outBuffer + offset, outSizeProcessed);
		}
		return true;
	} else {
		LOG_ERROR("Error extracting %s: %s", fileData[fid].origName.c_str(), GetErrorStr(res));
		return false;
	}
}

void CSevenZipArchive::FileInfo(unsigned int fid, std::string& name, int& size, int& mode) const
{
	name = fileData[fid].origName;
	size = fileData[fid].size;
	mode = fileData[fid].mode;
}
