/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <7z.h>
#include <7zFile.h>

#include "IArchive.h"
#include <string>
#include <vector>

/**
 * An LZMA/7zip compressed, single-file archive.
 */
class CSevenZipArchive : public IArchive
{
public:
	explicit CSevenZipArchive(const std::string& name);
	virtual ~CSevenZipArchive();

	virtual unsigned int NumFiles() const override;
	virtual bool GetFile(unsigned int fid, std::vector<unsigned char>& buffer) override;
	virtual void FileInfo(unsigned int fid, std::string& name, int& size, int& mode) const override;

private:
	struct FileData {
		int fp;
		/**
		 * Real/unpacked size of the file in bytes.
		 */
		int size;
		std::string origName;

		/**
		 * file mode
		 */
		int mode;
	};

	std::vector<FileData> fileData;

	UInt32 blockIndex = 0xFFFFFFFF;
	Byte* outBuffer = nullptr;
	size_t outBufferSize = 0;

	CFileInStream archiveStream;
	CSzArEx db;
	CLookToRead2 lookStream;
	ISzAlloc allocImp;
	ISzAlloc allocTempImp;

	bool isOpen = false;
};
