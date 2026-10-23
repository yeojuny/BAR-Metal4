/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <string>
#include <vector>

#include "FileData.h"
#include "HashMD5.h"
#include "IHash.h"

class SRepository;
class CRepo;
class IDownload;

#ifdef _WIN32
struct _FILETIME;
#endif

#define IO_BUF_SIZE 4096

/* Creates std::filesystem::path from utf-8 encoded std::string
 *
 * All paths and string as used in the pr-downloader must be UTF-8
 * and not any other encoding. This helper is a replactement for the
 * `std::filesystem::u8path` deprecated in C++17 that makes sure that path
 * is opened using UTF-8 mode.
 */
inline std::filesystem::path u8ToPath(const std::string& path)
{
#if defined(__cpp_lib_char8_t)
	return std::filesystem::path(std::u8string(path.begin(), path.end()));
#else
	return std::filesystem::u8path(path);
#endif
}

/* Creates a utf-8 encoded std::string from std::filesystem::path */
inline std::string pathToU8(std::filesystem::path path)
{
	const auto s = path.u8string();
	return std::string(s.cbegin(), s.cend());
}

class CFileSystem
{
public:
	static CFileSystem* GetInstance();
	static void Shutdown();

	/**
	 * parses the file for a mod and creates
	 */
	bool parseSdp(const std::string& filename, std::vector<FileData>& files);

	bool hashFile(IHash* outHash, const std::string& path) const;

	/**
	 * Validates a pool-file, (checks the md5)
	 */
	bool fileIsValid(const FileData* mod, const std::string& filename) const;

	/**
	 * returns the spring writeable directory
	 */
	const std::string getSpringDir();

	/**
	 * checks if a directory exists
	 */
	static bool directoryExists(const std::string& path);

	/**
	 * creates directory if it doesn't exist, expects PATH_DELIMETER at the end of the path creates
	 * a directory with all subdirectorys (doesn't handle c:\ ...)
	 */
	static bool createSubdirs(const std::string& path);

	/**
	 * returns a list of pairs (file path, md5) of all files in pool.
	 */
	std::optional<std::vector<std::pair<std::string, HashMD5>>> getPoolFiles();

	/**
	 * Validate all files in /pool/ (check md5)
	 * @return Whatever all files were correct or not
	 */
	bool validatePool(bool deletebroken);

	/**
	 * check if file is older then secs, returns true if file is older or something goes wrong
	 */
	bool isOlder(const std::string& filename, int secs);
	/**
	 * check if a file is readable
	 */
	static bool fileExists(const std::string& filename);
	/**
	 *
	 *	parses the bencoded torrent data, strucutre is like this:
	 *	dict {
	 *		info => dict {
	 *			length => int = 21713638
	 *			name => str = ba750.sdz (len = 9)
	 *			piece length => int = 262144
	 *			pieces => str = <sha1 checksums>
	 *		}
	 *	}
	 *
	 */
	bool parseTorrent(const char* data, int size, IDownload* dl);
	/**
	 * dumps info about the given .sdp
	 */
	bool dumpSDP(const std::string& filename);
	/**
	 * validates the given .sdp
	 */
	bool validateSDP(const std::string& filename);
	/**
	 * extracts a 7z file to dstdir
	 */
	bool extract(const std::string& filename, const std::string& dstdir, bool overwrite = false);
	/**
	 * extract engine download
	 */
	bool extractEngine(const std::string& filename, const std::string& version,
	                   const std::string& platform);
	bool setWritePath(const std::string& path);
	void setEnginePortableDownload(const bool portable)
	{
		portableDownload = portable;
	}

	/**
	 * returns full filename for pool file from md5
	 */
	std::string getPoolFilename(const std::string& md5str) const;

	/**
	 * tries to rename a file, copies if rename fails
	 */
	bool Rename(const std::string& source, const std::string& destination);

	/**
	 * replaces all invalid chars, i.e. \ from filename
	 */
	static std::string EscapeFilename(const std::string& path);

	static FILE* propen(const std::string& filename, const std::string& mode);

#ifdef _WIN32
	time_t FiletimeToTimestamp(const _FILETIME& time);
	void TimestampToFiletime(const time_t t, _FILETIME& pft);
#endif

	static bool removeFile(const std::string& path);
	static bool removeDir(const std::string& path);

	/**
	 * returns the directory for the path, i.e: dir/filename -> dir
	 */
	static std::string DirName(const std::string& path);

	static std::uint64_t getMBsFree(const std::string& path);

	static std::int64_t getFileSize(const std::string& path);

	static std::int64_t getFileTimestamp(const std::string& path);

	static int dupFileFD(FILE* f);

private:
	bool portableDownload = false;
	std::list<FileData> mods;
	bool parse_repository_line(char* str, SRepository* repository, int size);
	std::string springdir;
};

#define fileSystem CFileSystem::GetInstance()

#ifdef _WIN32
#define PATH_DELIMITER '\\'
#else
#define PATH_DELIMITER '/'
#endif
