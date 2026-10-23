#include "pr-downloader.h"
#include "Downloader/Download.h"
#include "Downloader/DownloadEnum.h"
#include "Downloader/IDownloader.h"
#include "FileSystem/FileSystem.h"
#include "Logger.h"
#include "Tracer.h"
#include "Version.h"
#include "lib/md5/md5.h"
#include <base64.h>

#include <assert.h>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_set>
#include <vector>

static bool fetchDepends = true;

void SetDownloadListener(IDownloaderProcessUpdateListener listener)
{
	IDownloader::setProcessUpdateListener(listener);
}

bool isEngineDownload(DownloadEnum::Category cat)
{
	return (cat == DownloadEnum::CAT_ENGINE) || (cat == DownloadEnum::CAT_ENGINE_LINUX) ||
	       (cat == DownloadEnum::CAT_ENGINE_LINUX64) ||
	       (cat == DownloadEnum::CAT_ENGINE_LINUX_ARM64) ||
	       (cat == DownloadEnum::CAT_ENGINE_MACOSX) ||
	       (cat == DownloadEnum::CAT_ENGINE_MACOSX_ARM64) ||
	       (cat == DownloadEnum::CAT_ENGINE_WINDOWS) ||
	       (cat == DownloadEnum::CAT_ENGINE_WINDOWS64) ||
	       (cat == DownloadEnum::CAT_ENGINE_WINDOWS_ARM64);
}

DownloadEnum::Category getPlatformEngineCat()
{
	switch (PRD_CURRENT_PLATFORM) {
		case Platform::Linux_x64:
			return DownloadEnum::CAT_ENGINE_LINUX64;
		case Platform::Windows_x64:
			return DownloadEnum::CAT_ENGINE_WINDOWS64;
		case Platform::Windows_arm64:
			return DownloadEnum::CAT_ENGINE_WINDOWS_ARM64;
		case Platform::Linux_arm64:
			return DownloadEnum::CAT_ENGINE_LINUX_ARM64;
		case Platform::MacOS_arm64:
			return DownloadEnum::CAT_ENGINE_MACOSX_ARM64;
		default:
			// unreachable
			std::abort();
	}
}

bool download_engine(std::list<IDownload*>& dllist)
{
	bool res = true;
	std::list<IDownload*> enginedls;

	for (IDownload* dl : dllist) {
		if (isEngineDownload(dl->cat)) {
			enginedls.push_back(dl);
		}
	}
	if (enginedls.empty())
		return res;

	httpDownload->download(enginedls);
	for (const IDownload* dl : enginedls) {
		if (!dl->isFinished())
			continue;
		if (!fileSystem->extractEngine(dl->name, dl->version,
		                               platformToString(PRD_CURRENT_PLATFORM))) {
			LOG_ERROR("Failed to extract engine %s", dl->version.c_str());
			res = false;
		}
	}
	return res;
}

// helper function
IDownload* GetIDownloadByID(std::list<IDownload*>& dllist, int id)
{
	std::list<IDownload*>::iterator it;
	int pos = 0;
	for (it = dllist.begin(); it != dllist.end(); ++it) {
		if (pos == id) {
			return *it;
		}
		pos++;
	}
	LOG_ERROR("%s: Couln't find dl %d", __FUNCTION__, id);
	return nullptr;
}

std::list<IDownload*> searchres;
std::list<int> downloads;

int DownloadAddByUrl(DownloadEnum::Category cat, const char* filename, const char* url)
{
	IDownload* dl = new IDownload(filename, url, cat);
	dl->addMirror(url);
	searchres.push_back(dl);
	return searchres.size();
}

bool search(std::vector<DownloadSearchItem>& items, std::list<IDownload*>& searchres)
{
	std::vector<DownloadSearchItem*> http_search;
	std::vector<DownloadSearchItem*> rapid_search;

	for (auto& item : items) {
		if (item.category == DownloadEnum::CAT_ENGINE) {
			// no platform specified, "translate" to current platform
			item.category = getPlatformEngineCat();
		}
		switch (item.category) {
			case DownloadEnum::CAT_HTTP:  // no search possible!
			case DownloadEnum::CAT_SPRINGLOBBY: {
				return false;
			}
			case DownloadEnum::CAT_GAME:
			case DownloadEnum::CAT_COUNT:
			case DownloadEnum::CAT_NONE:
				rapid_search.push_back(&item);
				[[fallthrough]];
			case DownloadEnum::CAT_MAP:
			case DownloadEnum::CAT_ENGINE_LINUX:
			case DownloadEnum::CAT_ENGINE_LINUX64:
			case DownloadEnum::CAT_ENGINE_LINUX_ARM64:
			case DownloadEnum::CAT_ENGINE_WINDOWS:
			case DownloadEnum::CAT_ENGINE_WINDOWS64:
			case DownloadEnum::CAT_ENGINE_WINDOWS_ARM64:
			case DownloadEnum::CAT_ENGINE_MACOSX:
			case DownloadEnum::CAT_ENGINE_MACOSX_ARM64:
				http_search.push_back(&item);
				break;
			default:
				LOG_ERROR("%s: type invalid", __FUNCTION__);
				assert(false);
				return false;
		}
	}
	return (rapid_search.empty() || rapidDownload->search(searchres, rapid_search)) &&
	       (http_search.empty() || httpDownload->search(searchres, http_search));
}

int DownloadSearch(std::vector<DownloadSearchItem>& items)
{
	TRACE();
	IDownloader::freeResult(searchres);
	downloads.clear();
	if (!search(items, searchres)) {
		return -1;
	}
	return searchres.size();
}

int DownloadSearch(DownloadEnum::Category cat, const char* name)
{
	std::vector<DownloadSearchItem> items = {DownloadSearchItem(cat, name)};
	DownloadSearch(items);
	return searchres.size();
}

bool DownloadGetInfo(int id, downloadInfo& info)
{
	IDownload* dl = GetIDownloadByID(searchres, id);
	if (dl != nullptr) {
		strncpy(info.filename, dl->name.c_str(),
		        NAME_LEN - 1);  // -1 because of 0 termination
		info.cat = dl->cat;
		return true;
	}
	return false;
}

void DownloadInit()
{
	IDownloader::Initialize();
}

void DownloadShutdown()
{
	IDownloader::freeResult(searchres);
	IDownloader::Shutdown();
	CFileSystem::Shutdown();
}

bool DownloadSetConfig(CONFIG type, const void* value)
{
	switch (type) {
		case CONFIG_FILESYSTEM_WRITEPATH: {
			std::string path((const char*)value);
			fileSystem->setWritePath(path);
			path = fileSystem->getSpringDir();
			LOG_INFO("Free disk space: %d MB", CFileSystem::getMBsFree(path));
			return true;
		}
		case CONFIG_FETCH_DEPENDS:
			fetchDepends = *static_cast<const bool*>(value);
			return true;
	}
	return false;
}

bool DownloadGetConfig(CONFIG type, const void** value)
{
	switch (type) {
		case CONFIG_FILESYSTEM_WRITEPATH:
			*value = fileSystem->getSpringDir().c_str();
			return true;
		case CONFIG_FETCH_DEPENDS:
			*value = (const bool*)fetchDepends;
			return true;
	}
	return false;
}

bool DownloadAdd(unsigned int id)
{
	if (id > searchres.size()) {
		LOG_ERROR("%s Invalid id %d", __FUNCTION__, id);
		return false;
	}
	downloads.push_back(id);
	return true;
}

bool addDepends(std::list<IDownload*>& dls)
{
	std::unordered_set<std::string> dls_set, depends_set;
	for (auto dl : dls) {
		dls_set.insert(dl->name);
		dls_set.insert(dl->origin_name);
	}
	std::vector<IDownload*> download_dep(dls.begin(), dls.end());
	while (!download_dep.empty()) {
		std::vector<DownloadSearchItem> depend_items;
		for (IDownload* dl : download_dep) {
			for (const std::string& depend : dl->depend) {
				if (depends_set.find(depend) == depends_set.end() &&
				    dls_set.find(depend) == dls_set.end()) {
					depends_set.insert(depend);
					depend_items.emplace_back(DownloadEnum::CAT_NONE, depend);
				}
			}
		}

		std::list<IDownload*> depends_dls;
		if (!depend_items.empty() && !search(depend_items, depends_dls)) {
			return false;
		}
		for (auto const& item : depend_items) {
			if (!item.found) {
				LOG_ERROR("Failed to find dependency %s", item.name.c_str());
				return false;
			}
		}

		download_dep.clear();
		for (IDownload* dl : depends_dls) {
			if (dls_set.find(dl->name) == dls_set.end()) {
				dls.push_back(dl);
				// TODO FIXME: Not a nice hack to make sure that download
				// is goind to be freed when searchres is cleaned up.
				searchres.push_back(dl);
				download_dep.push_back(dl);
				dls_set.insert(dl->name);
				dls_set.insert(dl->origin_name);
			}
		}
	}
	return true;
}

int DownloadStart()
{
	TRACE();
	std::list<IDownload*> dls;
	std::list<int>::iterator it;
	const std::string dldir = fileSystem->getSpringDir();
	const uint64_t MBsFree = CFileSystem::getMBsFree(dldir);
	uint64_t dlsize = 0;
	for (it = downloads.begin(); it != downloads.end(); ++it) {
		IDownload* dl = GetIDownloadByID(searchres, *it);
		if (dl->size > 0) {
			dlsize += dl->size;
		}
		if (dl == nullptr) {
			continue;
		}
		dls.push_back(dl);
	}
	// at least 1024MB free disk space are required (else fragmentation will make file access way to
	// slow!)
	const uint64_t MBsNeeded = (dlsize / (1024 * 1024)) + 1024;

	if (MBsFree < MBsNeeded) {
		LOG_ERROR("Insufficient free disk space (%" PRIu64 " MiB) on %s: %" PRIu64 " MiB needed",
		          MBsFree, dldir.c_str(), MBsNeeded);
		return 5;
	}

	if (fetchDepends && !addDepends(dls)) {
		return 6;
	}

	if (dls.empty()) {
		LOG_DEBUG("Nothing to do, did you forget to call DownloadAdd()?");
		return 1;
	}
	rapidDownload->download(dls);
	httpDownload->download(dls, 1);
	download_engine(dls);
	int res = 0;
	for (const IDownload* dl : dls) {
		if (dl->state != IDownload::STATE_FINISHED) {
			res = 2;
		}
	}
	dls.clear();
	return res;
}

bool DownloadRapidValidate(bool deletebroken)
{
	return fileSystem->validatePool(deletebroken);
}

bool DownloadDumpSDP(const char* path)
{
	return fileSystem->dumpSDP(path);
}

bool ValidateSDP(const char* path)
{
	return fileSystem->validateSDP(path);
}

void DownloadDisableLogging(bool disableLogging)
{
	LOG_DISABLE(disableLogging);
}

// TODO: Add support for other hash types (SHA1, CRC32, ..?)
char* CalcHash(const char* str, int size, int type)
{
	const unsigned char* hash;
	MD5_CTX ctx;
	if (type == 0) {
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char*)str, size);
		MD5Final(&ctx);
		hash = ctx.digest;
	} else {
		return nullptr;
	}

	const std::string encoded = base64_encode(hash, 16);
	char* ret = (char*)malloc((encoded.size() + 1) * sizeof(char));
	strncpy(ret, encoded.c_str(), encoded.size());
	ret[encoded.size()] = '\0';
	return ret;
}

void SetAbortDownloads(bool value)
{
	IDownloader::SetAbortDownloads(value);
}
