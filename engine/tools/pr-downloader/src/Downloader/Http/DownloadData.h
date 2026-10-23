/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "IOThreadPool.h"

class Mirror;
class IDownload;
class CurlWrapper;

// Used for computing progress across multiple DownloadData downloaded in
// parallel.
class DownloadDataPack
{
public:
	uint64_t size = 0;
	uint64_t progress = 0;
};

class DownloadData
{
public:
	DownloadData(std::optional<IOThreadPool::Handle> handle);
	std::unique_ptr<CurlWrapper> curlw;  // curl_easy_handle
	std::string mirror;                  // mirror used
	IDownload* download;
	DownloadDataPack* data_pack = nullptr;
	uint64_t approx_size = 0;  // Either approx or real size from the IDownload.
	int retry_num = 0;
	std::chrono::seconds retry_after_from_server{0};
	std::chrono::steady_clock::time_point next_retry;
	bool force_discard = false;
	std::optional<IOThreadPool::Handle> thread_handle;
	bool io_failure = false;  // Used by IO threads
	bool* abort_download = nullptr;

	void updateProgress(int64_t total, int64_t done);
};
