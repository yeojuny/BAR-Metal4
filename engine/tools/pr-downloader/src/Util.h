/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#define HTTP_SEARCH_URL "https://springfiles.springrts.com/json.php"
#define MAX_PARALLEL_DOWNLOADS 10

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class FileData;

/**
 * creates a url from fileinfo, for example
 * <path>/<first2chars of md5>/<last 30 chars of md5>.gz
 */
std::string getUrl(const FileData* info, const std::string& path);

/**
 * tokenizes a string into a vector split by c
 * empty tokens aren't ignored
 */
std::vector<std::string> tokenizeString(const std::string& str, char c);

/**
 * decompresses in to out
 */
int gzip_str(const char* in, const int inlen, char* out, int* outlen);

/**
 * parses an int, read from file or network
 */
unsigned int parse_int32(unsigned char c[4]);

/**
 * returns minimum
 */
unsigned int intmin(int x, int y);

/**
 * reates a path from an url, for example:
 * http://www.server.com/path/file.gz is translated to:
 * www.server.com\path\file.gz
 */
bool urlToPath(const std::string& url, std::string& path);

/**
 * convert std::wstring to std::string
 */
std::string ws2s(const std::wstring& s);
std::wstring s2ws(const std::string& s);

/**
 * To be called as first thing in the program, ensures that argv is utf8 encoded on all platforms.
 */
void ensureUtf8Argv(int* argc, char*** argv);

/**
 * Function for fetching environment variables under all platforms with correct encoding
 */
std::optional<std::string> getEnvVar(const std::string& name);

/**
 * Takes argc, args and a arg_name -> has_value map with valid options and returns a parsed
 * arguments separated into options and positional arguments.
 *
 * Throws FlagParseEx in case of issues.
 */
std::pair<std::unordered_map<std::string, std::vector<std::string>>, std::vector<std::string>>
parseArguments(int argc, char** argv, std::unordered_map<std::string, bool> const& valid_options);

class ArgumentParseEx : public std::runtime_error
{
public:
	template <class T>
	ArgumentParseEx(T&& arg)
		: std::runtime_error(std::forward<T>(arg)){};
};
