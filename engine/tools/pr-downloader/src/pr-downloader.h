/**
 * header for pr-downloader
 */

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Downloader/DownloadEnum.h"

#define NAME_LEN 1024
struct downloadInfo {
	char filename[NAME_LEN];
	DownloadEnum::Category cat;
};
/**
 * downloads all downloads that where added with @DownloadAdd clears search results
 */
extern int DownloadStart();

/**
 * adds a download by url without searching
 */
extern int DownloadAddByUrl(DownloadEnum::Category cat, const char* filename, const char* url);

/**
 * adds a download, see @DownloadSearch & @DownloadGetSearchInfo
 */
extern bool DownloadAdd(unsigned int id);
/**
 * search for name, calling this will overwrite results from the last call
 * @return count of results
 * @see downloadSearchGetId
 */
extern int DownloadSearch(DownloadEnum::Category category, const char* name);

struct DownloadSearchItem {
	DownloadSearchItem(DownloadEnum::Category category_, std::string name_)
		: name(name_)
		, category(category_)
	{
	}

	const std::string name;
	DownloadEnum::Category category;
	bool found = false;
};

/**
 * Same as DownloadSearch but allows to search for multiple items in single call returns -1 on
 * issues, else count of succesfully found items.
 */
extern int DownloadSearch(std::vector<DownloadSearchItem>& items);

/**
 * get info about a result / current download
 */
extern bool DownloadGetInfo(int id, downloadInfo& info);

/**
 * Initialize the lib
 */
extern void DownloadInit();
/**
 * shut down the lib
 */
extern void DownloadShutdown();

enum CONFIG {
	CONFIG_FILESYSTEM_WRITEPATH = 1,  // const char, sets the output directory
	CONFIG_FETCH_DEPENDS,             // bool, automaticly fetch depending files
};

/**
 * Set an option string
 */
extern bool DownloadSetConfig(CONFIG type, const void* value);

/**
 * returns config value, NULL when failed
 */
extern bool DownloadGetConfig(CONFIG type, const void** value);

/**
 * validate rapid pool
 * @param deletebroken files
 */
extern bool DownloadRapidValidate(bool deletebroken);

/**
 * dump contents of a sdp
 */
extern bool DownloadDumpSDP(const char* path);

/**
 * validate sdp files
 */
extern bool ValidateSDP(const char* path);

/**
 * control printing to stdout
 */
extern void DownloadDisableLogging(bool disableLogging);

typedef void (*IDownloaderProcessUpdateListener)(int done, int size);

extern void SetDownloadListener(IDownloaderProcessUpdateListener listener);

/**
 * Calculate hash and return it in base64 format.
 * Accepted values for type are:
 *   0 - md5
 */
extern char* CalcHash(const char* str, int size, int type);

/**
 * abort all downloads - must be called before shutting down,
 * all downloads must return before calling shutdown
 */
extern void SetAbortDownloads(bool value);

/**
 * return the platform dependant engine category
 */
extern DownloadEnum::Category getPlatformEngineCat();
