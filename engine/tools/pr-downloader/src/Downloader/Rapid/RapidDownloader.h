/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include "Downloader/IDownloader.h"

#include <list>
#include <stdio.h>
#include <string>

#define REPO_MASTER_RECHECK_TIME \
	86400  // how long to cache the repo-master file in secs without rechecking
#define REPO_RECHECK_TIME 0
#define REPO_MASTER "https://repos.springrts.com/repos.gz"

class CSdp;
class CHttpDownload;
class CFileSystem;
class CRepo;

class CRapidDownloader : public IDownloader
{
public:
	CRapidDownloader();

	/**
	 * search for a mod, searches for the short + long name
	 */
	bool search(std::list<IDownload*>& result,
	            const std::vector<DownloadSearchItem*>& items) override;
	/**
	 * start a download
	 */
	bool download(std::list<IDownload*>& downloads, int max_parallel = 10) override;

	bool setOption(const std::string& key, const std::string& value) override;

	void addRemoteSdp(CSdp&& dsp);

private:
	bool updateRepos(const std::vector<std::string>& searchstrs);
	bool parse();
	bool UpdateReposGZ();
	std::string path;
	std::string reposgzurl;
	std::list<CRepo> repos;

	/**
	 * download by name, for example "Complete Annihilation revision 1234"
	 */
	bool download_name(std::list<IDownload*>& downloads);

	/**
	 * compare str1 with str2
	 * if str2==* or "" it matches
	 * used for search in downloaders
	 */
	static bool match_download_name(const std::string& str1, const std::string& str2);

	static bool list_compare(const CSdp& first, const CSdp& second);
	std::list<CSdp> sdps;
};
