/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "File.h"
#include "FileSystem.h"
#include "Logger.h"

CFile::~CFile()
{
	Close();
}

bool CFile::Close(bool discard)
{
	if (handle == nullptr)
		return true;

	fclose(handle);
	handle = nullptr;

	if (discard) {
		fileSystem->removeFile(tmpfile);
		return true;
	}
	// delete possible existing destination file
	if (fileSystem->fileExists(filename) && !fileSystem->removeFile(filename)) {
		return false;
	}
	return fileSystem->Rename(tmpfile, filename);
}

bool CFile::Open(const std::string& filename)
{
	assert(handle == nullptr);
	this->filename = filename;
	fileSystem->createSubdirs(CFileSystem::DirName(filename));
	tmpfile = filename + ".tmp";
	handle = fileSystem->propen(tmpfile, "wb");
	if (handle == nullptr) {
		return false;
	}
	return true;
}

bool CFile::Write(const char* buf, int bufsize)
{
	assert(bufsize > 0);
	clearerr(handle);
	constexpr int PIECES = 1;
	const int res = fwrite(buf, bufsize, PIECES, handle);
	if (res != PIECES) {
		LOG_ERROR("write error %s (%d):  %s", filename.c_str(), res, strerror(errno));
		return false;
	}
	if (ferror(handle) != 0) {
		LOG_ERROR("Error in write(): %s %s", strerror(errno), filename.c_str());
		return false;
	}
	if (feof(handle)) {
		LOG_ERROR("EOF in write(): %s %s", strerror(errno), filename.c_str());
		return false;
	}
	return true;
}
