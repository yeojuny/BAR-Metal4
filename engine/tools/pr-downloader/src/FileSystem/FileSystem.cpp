/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "FileSystem.h"
#include "Downloader/IDownloader.h"
#include "FileData.h"
#include "HashGzip.h"
#include "HashMD5.h"
#include "IHash.h"
#include "Logger.h"
#include "SevenZipArchive.h"
#include "Tracer.h"
#include "Util.h"
#include "ZipArchive.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <zlib.h>

#ifdef _WIN32
#include <windows.h>

#include <fileapi.h>
#include <io.h>
#include <math.h>
#include <shlobj.h>
#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0
#endif
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

static CFileSystem* singleton = nullptr;

FILE* CFileSystem::propen(const std::string& filename, const std::string& mode)
{
#ifdef _WIN32
	FILE* ret = _wfopen(s2ws(filename).c_str(), s2ws(mode).c_str());
#else
	FILE* ret = fopen(filename.c_str(), mode.c_str());
#endif
	if (ret == nullptr) {
		LOG_ERROR("Couldn't open %s: %s", filename.c_str(), strerror(errno));
	}
	return ret;
}

bool CFileSystem::hashFile(IHash* outHash, const std::string& path) const
{
	char data[IO_BUF_SIZE];
	FILE* f = propen(path, "rb");
	if (f == nullptr) {
		return false;
	}
	outHash->Init();
	size_t size;
	do {
		size = fread(data, 1, IO_BUF_SIZE, f);
		outHash->Update(data, size);
	} while (size == IO_BUF_SIZE);
	bool ok = !ferror(f);
	if (!ok) {
		LOG_ERROR("Failed to read from %s", path.c_str());
	}
	outHash->Final();
	fclose(f);
	return ok;
}

bool CFileSystem::fileIsValid(const FileData* mod, const std::string& filename) const
{
	HashGzip gzipHash(std::make_unique<HashMD5>());
	if (!hashFile(&gzipHash, filename)) {
		return false;
	}
	return gzipHash.compare(mod->md5, sizeof(mod->md5));
}

std::string getMD5fromFilename(const std::string& path)
{
	const size_t start = path.rfind(PATH_DELIMITER) + 1;
	const size_t end = path.find(".", start);
	return path.substr(start, end - start);
}

bool CFileSystem::parseSdp(const std::string& filename, std::vector<FileData>& files)
{
	TRACE();
	char c_name[255];
	unsigned char c_md5[16];
	unsigned char c_crc32[4];
	unsigned char c_size[4];
	unsigned char length;

	FILE* f = propen(filename, "rb");
	if (f == nullptr) {
		return false;
	}
	int fd = fileSystem->dupFileFD(f);
	if (fd < 0) {
		fclose(f);
		return false;
	}
	gzFile in = gzdopen(fd, "rb");
	if (in == Z_NULL) {
		LOG_ERROR("Could not open %s", filename.c_str());
		fclose(f);
		return false;
	}
	files.clear();
	HashMD5 sdpmd5;
	sdpmd5.Init();
	while (true) {

		if (!gzread(in, &length, 1)) {
			if (gzeof(in)) {
				break;
			}
			LOG_ERROR("Unexpected eof in %s", filename.c_str());
			gzclose(in);
			fclose(f);
			return false;
		}
		if (!((gzread(in, &c_name, length)) && (gzread(in, &c_md5, 16)) &&
		      (gzread(in, &c_crc32, 4)) && (gzread(in, &c_size, 4)))) {
			LOG_ERROR("Error reading %s", filename.c_str());
			gzclose(in);
			fclose(f);
			return false;
		}
		FileData fd;
		fd.name = std::string(c_name, length);
		memcpy(fd.md5, &c_md5, 16);
		memcpy(fd.crc32, &c_crc32, 4);
		fd.size = parse_int32(c_size);
		files.push_back(fd);

		HashMD5 nameMd5;
		nameMd5.Init();
		nameMd5.Update(fd.name.data(), fd.name.size());
		nameMd5.Final();
		assert(nameMd5.getSize() == 16);
		assert(sizeof(fd.md5) == 16);
		sdpmd5.Update((const char*)nameMd5.Data(), nameMd5.getSize());
		sdpmd5.Update((const char*)&fd.md5[0], sizeof(fd.md5));
	}
	gzclose(in);
	fclose(f);
	sdpmd5.Final();
	const std::string filehash = getMD5fromFilename(filename);
	if (filehash != sdpmd5.toString()) {
		LOG_ERROR("%s is invalid (%s vs %s)", filename.c_str(), filehash.c_str(),
		          sdpmd5.toString().c_str());
		return false;
	}
	LOG_DEBUG("Parsed %s with %d files", filename.c_str(), (int)files.size());
	return true;
}

bool CFileSystem::setWritePath(const std::string& path)
{

	if (!path.empty()) {
		springdir = path;
	} else {
#ifndef _WIN32
		const char* buf = getenv("HOME");
		if (buf != nullptr) {
			springdir = buf;
			springdir.append("/.spring");
		} else {  // no home: use cwd
			LOG_INFO("HOME isn't set, using CWD./spring");
			springdir = ".spring";
		}
#else
		wchar_t my_documents[MAX_PATH];
		HRESULT result =
			SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, my_documents);
		if (result == S_OK) {
			springdir = ws2s(my_documents);
		}
		springdir.append("\\My Games\\Spring");
#endif
	}
	if (!springdir.empty()) {  // dir has to be without slash at the end
		if (springdir[springdir.length() - 1] == PATH_DELIMITER) {
			springdir = springdir.substr(0, springdir.size() - 1);
		}
	}
	LOG_INFO("Using filesystem-writepath: %s", springdir.c_str());
	return createSubdirs(springdir.c_str());
}

CFileSystem* CFileSystem::GetInstance()
{
	if (singleton == nullptr) {
		singleton = new CFileSystem();
	}
	return singleton;
}

void CFileSystem::Shutdown()
{
	delete singleton;
	singleton = nullptr;
}

const std::string CFileSystem::getSpringDir()
{
	if (springdir.empty())
		(setWritePath(""));
	return springdir;
}

bool CFileSystem::directoryExists(const std::string& path)
{
	if (path.empty())
		return false;
#ifdef _WIN32
	const std::wstring wpath = s2ws(path);
	DWORD dwAttrib = GetFileAttributesW(wpath.c_str());
	return ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
	struct stat fileinfo;
	const int res = stat(path.c_str(), &fileinfo);
	return (res == 0) && ((fileinfo.st_mode & S_IFDIR) != 0);
#endif
}

bool CreateDir(const std::string& path)
{
	assert(!path.empty());
#ifdef _WIN32
	if (CreateDirectory(s2ws(path).c_str(), nullptr) == 0) {
		LOG_ERROR("Error creating directory %s: code %d", path.c_str(), GetLastError());
		return false;
	}
#else
	if (mkdir(path.c_str(), 0755) == -1) {
		LOG_ERROR("Error creating directory %s: %s", path.c_str(), strerror(errno));
		return false;
	}
#endif
	return true;
}

bool CFileSystem::createSubdirs(const std::string& path)
{
	assert(!path.empty());
	if (directoryExists(path)) {
		return true;
	}
	for (size_t i = 2; i < path.size(); i++) {
		char c = path.at(i);
#ifdef _WIN32
		/* skip for example mkdir(C:\) */
		if ((i == 2) && (c == PATH_DELIMITER))
			continue;
#endif
		if (c != PATH_DELIMITER) {
			continue;
		}

		const std::string tocreate = path.substr(0, i);
		if (fileSystem->directoryExists(tocreate)) {
			continue;
		}

		if (!CreateDir(tocreate)) {
			return false;
		}
	}

	if (directoryExists(path)) {
		return true;
	}
	return CreateDir(path);
}

std::string CFileSystem::getPoolFilename(const std::string& md5str) const
{
	return fileSystem->getSpringDir() + PATH_DELIMITER + "pool" + PATH_DELIMITER + md5str.at(0) +
	       md5str.at(1) + PATH_DELIMITER + md5str.substr(2) + ".gz";
}

#ifdef _WIN32
std::optional<std::vector<std::pair<std::string, HashMD5>>> CFileSystem::getPoolFiles()
{
	TRACE();
	std::vector<std::pair<std::string, HashMD5>> files;
	const std::string basePath = getSpringDir() + PATH_DELIMITER + "pool" + PATH_DELIMITER;
	const std::wstring baseWPath = s2ws(basePath);

	static constexpr uint32_t MAX_THREADS = 8;
	const size_t num_threads = std::min(std::thread::hardware_concurrency(), MAX_THREADS);

	std::array<decltype(files), MAX_THREADS> files_parts;
	std::array<bool, MAX_THREADS> failed;
	std::array<std::thread, MAX_THREADS> threads;

	for (int tid = 0; tid < num_threads; ++tid) {
		threads[tid] = std::thread([tid, num_threads, &files = files_parts[tid],
		                            &failed = failed[tid], &basePath, &baseWPath]() {
			for (int i = tid; i < 256; i += num_threads) {
				failed = false;
				std::string firstByte;
				{
					char buf[10];
					sprintf(buf, "%02x", i);
					firstByte = buf;
				}

				wchar_t dir[MAX_PATH];
				_snwprintf_s(dir, _TRUNCATE, L"%s%02x/*", baseWPath.c_str(), i);

				WIN32_FIND_DATAW fileInfo;
				HANDLE searchHandle =
					FindFirstFileExW(dir, FindExInfoBasic, &fileInfo, FindExSearchNameMatch, NULL,
				                     FIND_FIRST_EX_LARGE_FETCH);

				if (searchHandle == INVALID_HANDLE_VALUE) {
					if (GetLastError() == ERROR_PATH_NOT_FOUND) {
						continue;
					}
					LOG_ERROR("Failed start file listing: code %d", GetLastError());
					failed = true;
					return;
				}

				do {
					std::string filename = ws2s(fileInfo.cFileName);
					constexpr int filenameLen = 33;  // like: 0235f418e51337469e445417853f76.gz
					if (filename.size() != filenameLen) {
						continue;
					}
					HashMD5 md5;
					if (!md5.IHash::Set(firstByte + filename.substr(0, filenameLen - 3))) {
						LOG_WARN("Invalid file name, ignoring: %s/%s", firstByte.c_str(),
						         filename.c_str());
						continue;
					}
					files.emplace_back(basePath + firstByte + PATH_DELIMITER + filename, md5);
				} while (FindNextFileW(searchHandle, &fileInfo) != 0);

				if (GetLastError() != ERROR_NO_MORE_FILES) {
					LOG_ERROR("Failed list files in directory: code %d", GetLastError());
					failed = true;
					return;
				}

				if (FindClose(searchHandle) == 0) {
					LOG_ERROR("Failed to close search directory: code %d", GetLastError());
					failed = true;
					return;
				}
			}
		});
	}
	size_t total_files_size = 0;
	for (int tid = 0; tid < num_threads; ++tid) {
		threads[tid].join();
		total_files_size += files_parts[tid].size();
	}
	files.reserve(total_files_size);
	for (int tid = 0; tid < num_threads; ++tid) {
		if (failed[tid]) {
			return std::nullopt;
		}
		files.insert(files.end(), std::make_move_iterator(files_parts[tid].begin()),
		             std::make_move_iterator(files_parts[tid].end()));
	}

	return files;
}
#else
std::optional<std::vector<std::pair<std::string, HashMD5>>> CFileSystem::getPoolFiles()
try {
	TRACE();
	const auto path = u8ToPath(getSpringDir() + PATH_DELIMITER + "pool");
	std::vector<std::pair<std::string, HashMD5>> files;
	for (const std::filesystem::directory_entry& dir_entry :
	     std::filesystem::recursive_directory_iterator(path)) {
		auto const& p = dir_entry.path();
		if (!dir_entry.is_regular_file() || p.extension() == ".tmp") {
			continue;
		}
		HashMD5 md5;
		if (!md5.IHash::Set(p.parent_path().filename().string() + p.stem().string())) {
			LOG_WARN("Invalid file name, ignoring: %s", pathToU8(p).c_str());
			continue;
		}
		files.emplace_back(pathToU8(p), md5);
	}
	return files;
} catch (std::filesystem::filesystem_error const& ex) {
	LOG_ERROR("Failed to read pool files: %s", ex.what());
	return std::nullopt;
}
#endif

bool CFileSystem::validatePool(bool deletebroken)
{
	const auto res = getPoolFiles();
	if (!res) {
		return false;
	}
	auto const& files_to_validate = *res;
	bool ok = true;
	unsigned progress = 0;
	LOG_PROGRESS(progress, files_to_validate.size());
	for (const auto& [path, md5] : files_to_validate) {
		FileData filedata;
		for (unsigned i = 0; i < 16; i++) {
			filedata.md5[i] = md5.get(i);
		}
		if (!fileIsValid(&filedata, path)) {
			ok = false;
			LOG_ERROR("Invalid File in pool: %s", path.c_str());
			if (deletebroken) {
				removeFile(path);
			}
		}
		++progress;
		LOG_PROGRESS(progress, files_to_validate.size(), progress == files_to_validate.size());
	}
	return ok;
}

bool CFileSystem::isOlder(const std::string& filename, int secs)
{
	if (secs <= 0)
		return true;
	struct stat sb;
	if (stat(filename.c_str(), &sb) < 0) {
		return true;
	}
#ifdef _WIN32
	SYSTEMTIME pTime;
	FILETIME pFTime;
	GetSystemTime(&pTime);
	SystemTimeToFileTime(&pTime, &pFTime);
	const time_t t = FiletimeToTimestamp(pFTime);
	LOG_DEBUG("%s is %d seconds old, redownloading at %d", filename.c_str(), (int)(t - sb.st_ctime),
	          secs);
	return (t < sb.st_ctime + secs);

#else
	time_t t;
	time(&t);
	struct tm lt;
	localtime_r(&sb.st_mtime, &lt);

	const time_t filetime = mktime(&lt);
	const double diff = difftime(t, filetime);

	LOG_DEBUG("checking time: %s  %.0fs >  %ds res: %d", filename.c_str(), diff, secs,
	          (bool)(diff > secs));
	return (diff > secs);
#endif
}

bool CFileSystem::fileExists(const std::string& path)
{
	if (path.empty())
		return false;
#ifdef _WIN32
	DWORD dwAttrib = GetFileAttributesW(s2ws(path).c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES);
#else
	struct stat buffer;
	return stat(path.c_str(), &buffer) == 0;
#endif
}

bool CFileSystem::removeFile(const std::string& path)
{
#ifdef _WIN32
	const bool res = _wunlink(s2ws(path).c_str()) == 0;
#else
	const bool res = unlink(path.c_str()) == 0;
#endif
	if (!res) {
		LOG_ERROR("Couldn't delete file %s: %s", path.c_str(), strerror(errno));
	}
	return res;
}

bool CFileSystem::removeDir(const std::string& path)
{
#ifdef _WIN32
	const bool res = _wrmdir(s2ws(path).c_str()) == 0;
#else
	const bool res = rmdir(path.c_str()) == 0;
#endif
	if (!res) {
		LOG_ERROR("Couldn't delete dir %s: %s", path.c_str(), strerror(errno));
	}
	return res;
}

bool CFileSystem::dumpSDP(const std::string& filename)
{
	std::vector<FileData> files;
	if (!parseSdp(filename, files))
		return false;
	LOG_INFO("md5 (filename in pool)           crc32        size filename");
	HashMD5 md5;
	for (const FileData& fd : files) {
		md5.Set(fd.md5, sizeof(fd.md5));
		LOG_INFO("%s %.8X %8d %s", md5.toString().c_str(), fd.crc32, fd.size, fd.name.c_str());
	}
	return true;
}

bool CFileSystem::validateSDP(const std::string& sdpPath)
{
	LOG_DEBUG("CFileSystem::validateSDP() ...");
	if (!fileExists(sdpPath)) {
		LOG_ERROR("SDP file doesn't exist: %s", sdpPath.c_str());
		return false;
	}

	std::vector<FileData> files;
	if (!parseSdp(sdpPath, files)) {  // parse downloaded file
		LOG_ERROR("Removing invalid SDP file: %s", sdpPath.c_str());
		if (!removeFile(sdpPath)) {
			LOG_ERROR("Failed removing %s, aborting", sdpPath.c_str());
			return false;
		}
		return false;
	}

	bool valid = true;
	for (FileData& fd : files) {
		HashMD5 fileMd5;
		fileMd5.Set(fd.md5, sizeof(fd.md5));
		const std::string filePath = getPoolFilename(fileMd5.toString());
		if (!fileExists(filePath)) {
			valid = false;
			LOG_INFO("Missing file: %s", filePath.c_str());
		} else if (!fileIsValid(&fd, filePath)) {
			valid = false;
			LOG_INFO("Removing invalid file: %s", filePath.c_str());
			if (!removeFile(filePath)) {
				LOG_ERROR("Failed removing %s, aborting", filePath.c_str());
				return false;
			}
		}
	}
	LOG_DEBUG("CFileSystem::validateSDP() done");
	return valid;
}

bool CFileSystem::extractEngine(const std::string& filename, const std::string& version,
                                const std::string& platform)
{
	const std::string output = getSpringDir() + PATH_DELIMITER + "engine" + PATH_DELIMITER +
	                           platform + PATH_DELIMITER + CFileSystem::EscapeFilename(version);
	if (!extract(filename, output)) {
		LOG_DEBUG("Failed to extract %s %s", filename.c_str(), output.c_str());
		return false;
	}
	if (portableDownload)
		return true;
	const std::string cfg = output + PATH_DELIMITER + "springsettings.cfg";
	if (fileExists(cfg)) {
		return removeFile(cfg);
	}
	return true;
}

bool CFileSystem::extract(const std::string& filename, const std::string& dstdir, bool overwrite)
{
	LOG_INFO("Extracting %s to %s", filename.c_str(), dstdir.c_str());
	const int len = filename.length();
	IArchive* archive;
	if ((len > 4) && (filename.compare(len - 3, 3, ".7z") == 0)) {
		archive = new CSevenZipArchive(filename);
	} else {
		archive = new CZipArchive(filename);
	}

	const unsigned int num = archive->NumFiles();
	if (num <= 0) {
		LOG_WARN("Empty archive:  %s", filename.c_str());
		delete archive;
		return false;
	}
	std::vector<unsigned char> buf;
	for (unsigned int i = 0; i < num; i++) {
		buf.clear();
		std::string name;
		int size, mode;
		archive->FileInfo(i, name, size, mode);
		if (!archive->GetFile(i, buf)) {
			LOG_ERROR("Error extracting %s from %s", name.c_str(), filename.c_str());
			delete archive;
			return false;
		}
#ifdef _WIN32
		for (unsigned int i = 0; i < name.length(); i++) {  // replace / with \ on win32
			if (name[i] == '/')
				name[i] = PATH_DELIMITER;
		}
#endif
		std::string tmp = dstdir;

		if (!tmp.empty() && tmp[tmp.length() - 1] != PATH_DELIMITER) {
			tmp += PATH_DELIMITER;
		}

		tmp += name.c_str();  // FIXME: concating UTF-16
		createSubdirs(DirName(tmp));
		if (fileSystem->fileExists(tmp)) {
			LOG_WARN("File already exists: %s", tmp.c_str());
			if (!overwrite)
				continue;
		}
		LOG_INFO("extracting (%s)", tmp.c_str());
		FILE* f = propen(tmp, "wb+");
		if (f == nullptr) {
			LOG_ERROR("Error creating %s", tmp.c_str());
			delete archive;
			return false;
		}
		int res = 1;
		if (!buf.empty())
			res = fwrite(&buf[0], buf.size(), 1, f);
#ifndef _WIN32
		fchmod(fileno(f), mode);
#endif
		if (res <= 0) {
			const int err = ferror(f);
			LOG_ERROR("fwrite(%s): %d %s", name.c_str(), err, strerror(err));
			fclose(f);
			delete archive;
			return false;
		}
		fclose(f);
	}
	delete archive;
	LOG_INFO("done");
	return true;
}

bool CFileSystem::Rename(const std::string& source, const std::string& destination)
try {
	std::filesystem::rename(u8ToPath(source), u8ToPath(destination));
	return true;
} catch (std::filesystem::filesystem_error const& ex) {
	LOG_ERROR("Failed to rename %s to %s: %s", source.c_str(), destination.c_str(), ex.what());
	return false;
}

std::string CFileSystem::DirName(const std::string& path)
{
	const std::string::size_type pos = path.rfind(PATH_DELIMITER);
	if (pos != std::string::npos) {
		return path.substr(0, pos);
	} else {
		return path;
	}
}

#ifdef _WIN32
time_t CFileSystem::FiletimeToTimestamp(const _FILETIME& time)
{
	LARGE_INTEGER date, adjust;
	date.HighPart = time.dwHighDateTime;
	date.LowPart = time.dwLowDateTime;
	adjust.QuadPart = 11644473600000 * 10000;
	date.QuadPart -= adjust.QuadPart;
	return (date.QuadPart / 10000000);
}

void CFileSystem::TimestampToFiletime(const time_t t, _FILETIME& pft)
{
	LONGLONG ll;
	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft.dwLowDateTime = (DWORD)ll;
	pft.dwHighDateTime = ll >> 32;
}
#endif

std::string CFileSystem::EscapeFilename(const std::string& str)
{
	std::string s = str;
	const static std::string illegalChars = "\\/:?\"<>|";

	for (auto it = s.begin(); it < s.end(); ++it) {
		const bool found = illegalChars.find(*it) != std::string::npos;
		if (found) {
			*it = '_';
		}
	}
	return s;
}

uint64_t CFileSystem::getMBsFree(const std::string& path)
{
	std::error_code ec;
	auto sp = std::filesystem::space(u8ToPath(path), ec);
	if (ec) {
		LOG_ERROR("Error getting free disk space on %s: %s", path.c_str(), ec.message().c_str());
		return 0;
	}
	return sp.available / (1024 * 1024);
}

int64_t CFileSystem::getFileSize(const std::string& path)
{
	std::error_code ec;
	std::uintmax_t size = std::filesystem::file_size(u8ToPath(path), ec);
	if (ec) {
		LOG_ERROR("Failed to get size of %s: %s", path.c_str(), ec.message().c_str());
		return -1;
	} else if (size > static_cast<std::uintmax_t>(std::numeric_limits<int64_t>::max())) {
		LOG_ERROR("File size of %s doesn't fit in int64_t", path.c_str());
		return -1;
	}
	return static_cast<int64_t>(size);
}

int64_t CFileSystem::getFileTimestamp(const std::string& path)
{
#if defined(__WIN32__) || defined(_MSC_VER)
	struct __stat64 sb;
	int res = _wstat64(s2ws(path).c_str(), &sb);
#else
	struct stat sb;
	int res = stat(path.c_str(), &sb);
#endif
	if (res != 0) {
		LOG_ERROR("Couldn't get timestamp of file %s: %s", path.c_str(), strerror(errno));
		return -1;
	}
	return sb.st_mtime;
}

int CFileSystem::dupFileFD(FILE* f)
{
#if _WIN32
	int res = _dup(_fileno(f));
#else
	int res = dup(fileno(f));
#endif
	if (res < 0) {
		LOG_ERROR("Couldn't duplicate file descriptor: %s", strerror(errno));
	}
	return res;
}
