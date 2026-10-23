/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#ifndef PR_DOWNLOADER_VERSION
#error PR_DOWNLOADER_VERSION is not defined
#else

#include "Version.h"
#include <cstdlib>

#define QUOTEME_(x) #x
#define QUOTEME(x) QUOTEME_(x)

#define QUOTEDVERSION QUOTEME(PR_DOWNLOADER_VERSION)
#endif

const char* getVersion()
{
	const static char ver[] = QUOTEDVERSION;
	return ver;
}

const char* getAgent()
{
	const static char agent[] = "pr-downloader/" QUOTEDVERSION;
	return agent;
}

const char* platformToString(Platform platform)
{
	switch (platform) {
		case Platform::Linux_x64:
			return "linux64";
		case Platform::Windows_x64:
			return "windows64";
		case Platform::Windows_arm64:
			return "windows_arm64";
		case Platform::MacOS_arm64:
			return "macos_arm64";
		case Platform::Linux_arm64:
			return "linux_arm64";
		default:
			// unreachable
			std::abort();
	}
}
