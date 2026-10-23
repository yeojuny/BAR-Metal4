/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include <curl/curl.h>
#include <string>

class CurlWrapper
{
public:
	CurlWrapper();
	~CurlWrapper();
	CURL* GetHandle() const
	{
		return handle;
	}
	std::string GetError() const;
	static std::string EscapeUrl(const std::string& url);
	static void InitCurl();
	static void KillCurl();
	static CURLM* GetMultiHandle();
	void AddHeader(const std::string& header);

private:
	CURL* handle;
	char* errbuf;
	curl_slist* list = nullptr;
};
