/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "DownloadData.h"
#include "Downloader/CurlWrapper.h"
#include "Downloader/Download.h"
#include "Downloader/IDownloader.h"
#include "Logger.h"

DownloadData::DownloadData(std::optional<IOThreadPool::Handle> handle)
	: curlw(new CurlWrapper())
	, thread_handle(std::move(handle))
{
}

void DownloadData::updateProgress(int64_t total, int64_t done)
{
	if (data_pack != nullptr) {
		// Because we can have only approximate size, we map real size
		// to the approximate size scale to keep the total during
		// the download constant.
		const uint64_t old_progress = download->getProgress();
		const uint64_t progress = done;
		download->updateProgress(progress);
		const double at = static_cast<double>(approx_size) / total;
		data_pack->progress +=
			static_cast<uint64_t>(at * progress) - static_cast<uint64_t>(at * old_progress);

		if (IDownloader::listener != nullptr) {
			IDownloader::listener(data_pack->progress, data_pack->size);
		}
		LOG_PROGRESS(data_pack->progress, data_pack->size, data_pack->progress >= data_pack->size);
	} else {
		download->updateProgress(done);
		if (IDownloader::listener != nullptr) {
			IDownloader::listener(done, total);
		}
		LOG_PROGRESS(done, total, done >= total);
	}
}
