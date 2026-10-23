/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include "Downloader/IDownloader.h"

#include <curl/curl.h>
#include <list>
#include <string>

class CHttpDownloader : public IDownloader
{
public:
	virtual bool search(std::list<IDownload*>& result,
	                    const std::vector<DownloadSearchItem*>& items) override;
	virtual bool download(std::list<IDownload*>& download, int max_parallel = 10) override;
	static bool DownloadUrl(const std::string& url, std::string& res);
	static bool ParseResult(const std::string& name, const std::string& json,
	                        std::list<IDownload*>& res);
};
