/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "Util.h"
#include "FileSystem/FileSystem.h"
#include "Logger.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <zlib.h>

std::vector<std::string> tokenizeString(const std::string& str, char c)
{
	std::vector<std::string> res;
	size_t start = 0;
	size_t pos;
	for (pos = 0; pos < str.size(); pos++) {
		if (str[pos] == c) {
			const size_t len = pos - start;
			res.push_back(str.substr(start, len));
			start = pos + 1;
		}
	}

	if (start < str.size()) {
		res.push_back(str.substr(start, start - str.size()));
	} else if (start == str.size()) {
		res.push_back(std::string());
	}
	return res;
}

int gzip_str(const char* in, const int inlen, char* out, int* outlen)
{
	z_stream zlibStreamStruct;
	zlibStreamStruct.zalloc = Z_NULL;       // Set zalloc, zfree, and opaque to Z_NULL so
	zlibStreamStruct.zfree = Z_NULL;        // that when we call deflateInit2 they will be
	zlibStreamStruct.opaque = Z_NULL;       // updated to use default allocation functions.
	zlibStreamStruct.total_out = 0;         // Total number of output bytes produced so far
	zlibStreamStruct.next_in = (Bytef*)in;  // Pointer to input bytes
	zlibStreamStruct.avail_in = inlen;      // Number

	int res = deflateInit2(&zlibStreamStruct, Z_DEFAULT_COMPRESSION, Z_DEFLATED, (15 + 16), 8,
	                       Z_DEFAULT_STRATEGY);
	if (res != Z_OK)
		return res;
	do {
		zlibStreamStruct.next_out = (Bytef*)out + zlibStreamStruct.total_out;
		zlibStreamStruct.avail_out = *outlen - zlibStreamStruct.total_out;
		res = deflate(&zlibStreamStruct, Z_FINISH);
	} while (res == Z_OK);
	deflateEnd(&zlibStreamStruct);
	*outlen = zlibStreamStruct.total_out;
	if (zlibStreamStruct.avail_in != 0) {
		LOG_ERROR("Couldn't compress string");
		return -1;
	}
	return Z_OK;
}

unsigned int parse_int32(unsigned char c[4])
{
	unsigned int i = 0;
	i = c[0] << 24 | i;
	i = c[1] << 16 | i;
	i = c[2] << 8 | i;
	i = c[3] << 0 | i;
	return i;
}

unsigned int intmin(int x, int y)
{
	if (x < y)
		return x;
	return y;
}

bool urlToPath(const std::string& url, std::string& path)
{
	size_t pos = url.find("//");
	if (pos == std::string::npos) {  // not found
		LOG_ERROR("urlToPath failed: %s", path.c_str());
		return false;
	}
	path = url.substr(pos + 2);
	pos = path.find("/", pos + 1);
	while (pos != std::string::npos) {  // replace / with "\\"
		path.replace(pos, 1, 1, PATH_DELIMITER);
		pos = path.find("/", pos + 1);
	}
	for (size_t i = 0; i < path.length(); i++) {  // replace : in url
		if (path[i] == ':') {
			path[i] = '-';
		}
	}
	return true;
}

#ifdef _WIN32
// clang-format off
// windows.h must be included before shellapi.h
#include <windows.h>
#include <shellapi.h>
// clang-format on

std::wstring s2ws(const std::string& s)
{
	const size_t slength = s.length();
	const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf, len);
	delete[] buf;
	return r;
}

std::string ws2s(const std::wstring& s)
{
	const size_t slength = s.length();
	const int len =
		WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, nullptr, 0, nullptr, nullptr);
	char* buf = new char[len];
	WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, buf, len, nullptr, nullptr);
	std::string r(buf, len);
	delete[] buf;
	return r;
}

void ensureUtf8Argv(int* argc, char*** argv)
{
	static std::vector<std::string> argv_strings;
	static std::vector<const char*> argv_data;
	static int win32_argc = 0;
	if (win32_argc == 0) {
		wchar_t** argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
		argv_strings.reserve(win32_argc);
		argv_data.reserve(win32_argc);
		for (int i = 0; i < win32_argc; ++i) {
			argv_strings.emplace_back(ws2s(argv_w[i]));
			argv_data.emplace_back(argv_strings.back().c_str());
		}
		LocalFree(argv_w);
	}
	*argc = win32_argc;
	*argv = const_cast<char**>(argv_data.data());
}

std::optional<std::string> getEnvVar(const std::string& name)
{
	std::wstring wname = s2ws(name);
	DWORD size = GetEnvironmentVariableW(wname.c_str(), NULL, 0);
	if (size == 0) {
		if (GetLastError() != ERROR_ENVVAR_NOT_FOUND) {
			LOG_ERROR("Failed to get environment variable %s: %u", name.c_str(), GetLastError());
		}
		return std::nullopt;
	}
	auto buf = std::make_unique<wchar_t[]>(size);
	GetEnvironmentVariableW(wname.c_str(), buf.get(), size);
	return ws2s(buf.get());
}

#else

void ensureUtf8Argv(int*, char***)
{
}

std::optional<std::string> getEnvVar(const std::string& name)
{
	const char* value = std::getenv(name.c_str());
	if (value == nullptr) {
		return std::nullopt;
	}
	return std::string(value);
}

#endif

std::pair<std::unordered_map<std::string, std::vector<std::string>>, std::vector<std::string>>
parseArguments(int argc, char** argv, std::unordered_map<std::string, bool> const& valid_options)
{
	std::vector<std::string> positional;
	std::unordered_map<std::string, std::vector<std::string>> args;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg.substr(0, 2) != "--") {
			positional.emplace_back(std::move(arg));
			continue;
			;
		}
		const auto eqpos = arg.find("=");
		const std::string arg_name = arg.substr(2, eqpos - 2);
		const auto vo = valid_options.find(arg_name);
		if (vo == valid_options.end()) {
			throw ArgumentParseEx("unknown option --" + arg_name);
		}
		if (!vo->second) {
			if (eqpos != std::string::npos) {
				throw ArgumentParseEx("option --" + arg_name + " is not taking value, got value");
			}
			args[arg_name] = {};
			continue;
		}
		std::string arg_val;
		if (eqpos != std::string::npos) {
			arg_val = arg.substr(eqpos + 1);
		} else {
			++i;
			if (i >= argc) {
				throw ArgumentParseEx("option --" + arg_name + " is taking value, didn't got any");
			}
			arg_val = argv[i];
		}
		args[arg_name].emplace_back(std::move(arg_val));
	}
	return std::make_pair(std::move(args), std::move(positional));
}
