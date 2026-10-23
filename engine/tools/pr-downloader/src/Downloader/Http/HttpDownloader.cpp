/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "HttpDownloader.h"
#include "Downloader/Download.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <zlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

#include <json/reader.h>

#include "DownloadData.h"
#include "Downloader/CurlWrapper.h"
#include "ETag.h"
#include "FileSystem/File.h"
#include "FileSystem/FileSystem.h"
#include "FileSystem/HashMD5.h"
#include "IOThreadPool.h"
#include "Logger.h"
#include "Throttler.h"
#include "Tracer.h"
#include "Util.h"

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	if (IDownloader::AbortDownloads()) {
		return -1;
	}

	const size_t realsize = size * nmemb;
	std::string* res = static_cast<std::string*>(userp);
	res->append((char*)contents, realsize);
	return realsize;
}

static int progress_func(DownloadData* data, curl_off_t total, curl_off_t done, curl_off_t,
                         curl_off_t)
{
	// This functions will be called with 0 when redirections and header parsing happens in curl.
	if (total == 0) {
		return 0;
	}
	if (IDownloader::AbortDownloads()) {
		return -1;
	}
	data->updateProgress(total, done);
	return 0;
}

// downloads url into res
bool CHttpDownloader::DownloadUrl(const std::string& url, std::string& res)
{
	DownloadData d(std::nullopt);
	d.download = new IDownload();
	d.download->addMirror(url);
	d.download->name = url;
	d.download->origin_name = url;

	CurlWrapper curlw;
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_URL, url.c_str());
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEDATA, (void*)&res);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_XFERINFODATA, &d);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_XFERINFOFUNCTION, progress_func);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_NOPROGRESS, 0L);

	curl_easy_setopt(curlw.GetHandle(), CURLOPT_VERBOSE, 1L);

	const CURLcode curlres = curl_easy_perform(curlw.GetHandle());

	delete d.download;
	d.download = nullptr;
	if (curlres != CURLE_OK) {
		LOG_ERROR("Error in curl %s (%s)", curl_easy_strerror(curlres), curlw.GetError().c_str());
	}
	return curlres == CURLE_OK;
}

static std::string getRequestUrl(const std::string& name, DownloadEnum::Category cat)
{
	std::string http_search_url = HTTP_SEARCH_URL;
	const char* http_search_url_env = std::getenv("PRD_HTTP_SEARCH_URL");
	if (http_search_url_env != nullptr) {
		http_search_url = http_search_url_env;
	}

	std::string url = http_search_url + std::string("?");
	if (cat != DownloadEnum::CAT_NONE) {
		url += "category=" + CurlWrapper::EscapeUrl(DownloadEnum::getCat(cat)) + std::string("&");
	}
	return url + std::string("springname=") + CurlWrapper::EscapeUrl(name);
}

bool CHttpDownloader::ParseResult(const std::string& /*name*/, const std::string& json,
                                  std::list<IDownload*>& res)
{
	Json::Value result;  // will contains the root value after parsing.
	Json::Reader reader;
	const bool parsingSuccessful = reader.parse(json, result);
	if (!parsingSuccessful) {
		LOG_ERROR("Couldn't parse result: %s %s", reader.getFormattedErrorMessages().c_str(),
		          json.c_str());
		return false;
	}

	if (!result.isArray()) {
		LOG_ERROR("Returned json isn't an array!");
		return false;
	}

	for (Json::Value::ArrayIndex i = 0; i < result.size(); i++) {
		const Json::Value resfile = result[i];

		if (!resfile.isObject()) {
			LOG_ERROR("Entry isn't object!");
			return false;
		}
		if (!resfile["category"].isString()) {
			LOG_ERROR("No category in result");
			return false;
		}
		if (!resfile["springname"].isString()) {
			LOG_ERROR("No springname in result");
			return false;
		}
		std::string filename = fileSystem->getSpringDir();
		const std::string category = resfile["category"].asString();
		const std::string springname = resfile["springname"].asString();
		filename += PATH_DELIMITER;

		if (category == "map") {
			filename += "maps";
		} else if (category == "game") {
			filename += "games";
		} else if (category.find("engine") == 0) {  // engine_windows, engine_linux, engine_macosx
			filename += "engine";
		} else
			LOG_ERROR("Unknown Category %s", category.c_str());
		filename += PATH_DELIMITER;

		if ((!resfile["mirrors"].isArray()) || (!resfile["filename"].isString())) {
			LOG_ERROR("Invalid type in result");
			return false;
		}
		filename.append(CFileSystem::EscapeFilename(resfile["filename"].asString()));

		const DownloadEnum::Category cat = DownloadEnum::getCatFromStr(category);
		IDownload* dl = new IDownload(filename, springname, cat);
		const Json::Value mirrors = resfile["mirrors"];
		for (Json::Value::ArrayIndex j = 0; j < mirrors.size(); j++) {
			if (!mirrors[j].isString()) {
				LOG_ERROR("Invalid type in result");
			} else {
				dl->addMirror(mirrors[j].asString());
			}
		}

		if (resfile["version"].isString()) {
			const std::string& version = resfile["version"].asString();
			dl->version = version;
		}
		if (resfile["md5"].isString()) {
			dl->hash = std::make_unique<HashMD5>();
			dl->hash->Set(resfile["md5"].asString());
			dl->out_hash = std::make_unique<HashMD5>();
			dl->write_md5sum = true;
		}
		if (resfile["size"].isInt()) {
			dl->size = resfile["size"].asInt();
		}
		if (resfile["depends"].isArray()) {
			for (Json::Value::ArrayIndex i = 0; i < resfile["depends"].size(); i++) {
				if (resfile["depends"][i].isString()) {
					const std::string& dep = resfile["depends"][i].asString();
					dl->addDepend(dep);
				}
			}
		}
		res.push_back(dl);
	}
	LOG_DEBUG("Parsed %d results", res.size());
	return true;
}

bool CHttpDownloader::search(std::list<IDownload*>& res,
                             const std::vector<DownloadSearchItem*>& items)
{
	TRACE();
	for (auto& item : items) {
		if (item->found) {
			continue;
		}
		LOG_DEBUG("%s", item->name.c_str());
		std::string dlres;
		const std::string url = getRequestUrl(item->name, item->category);
		if (!DownloadUrl(url, dlres)) {
			LOG_ERROR("Error downloading %s %s", url.c_str(), dlres.c_str());
			return false;
		}
		if (!ParseResult(item->name, dlres, res)) {
			return false;
		}
		item->found = true;
	}
	return true;
}

template <class F>
IOThreadPool::WorkF ioFailureWrap(DownloadData* data, F&& f)
{
	return [data, f = std::move(f)]() -> IOThreadPool::OptRetF {
		if (data->io_failure) {
			return std::nullopt;
		}
		if (!f(data)) {
			data->io_failure = true;
			return [data] { *data->abort_download = true; };
		}
		return std::nullopt;
	};
}

static size_t multi_write_data(void* ptr, size_t size, size_t nmemb, DownloadData* data)
{
	if (IDownloader::AbortDownloads())
		return -1;

	// shared_ptr because std::function must be copyable.
	auto buffer = std::shared_ptr<char[]>(new char[size * nmemb]);
	memcpy(buffer.get(), ptr, size * nmemb);

	data->thread_handle->submit(
		ioFailureWrap(data, [buffer = std::move(buffer), size = size * nmemb](DownloadData* data) {
			data->download->state = IDownload::STATE_DOWNLOADING;
			if (data->download->out_hash != nullptr) {
				data->download->out_hash->Update(buffer.get(), size);
			}
			if (!data->download->file->Write(buffer.get(), size)) {
				return false;
			}
			return true;
		}));
	return size * nmemb;
}

// Computes the exponential retry duration to wait before making next request.
template <class D1, class D2, class DR = typename std::common_type<D1, D2>::type>
static DR retryAfter(int retry_num, D1 base_delay, D2 max_delay, double factor = 2.0)
{
	static std::default_random_engine gen(std::random_device{}());
	static std::uniform_real_distribution<> dis(0.7, 1.2);
	if (retry_num <= 0) {
		return DR(0);
	}
	auto backoff = base_delay * std::pow(factor, retry_num - 1) * dis(gen);
	return std::min(std::chrono::duration_cast<DR>(backoff),
	                std::chrono::duration_cast<DR>(max_delay));
}

static bool setupDownload(CURLM* curlm, DownloadData* piece)
{
	static std::default_random_engine gen(std::random_device{}());

	if (piece->download->isFinished())
		return false;

	if (piece->download->getMirrorCount() < 1) {
		LOG_ERROR("No mirror found for %s", piece->download->name.c_str());
		return false;
	}
	std::uniform_int_distribution<> dist(0, piece->download->getMirrorCount() - 1);
	piece->mirror = piece->download->getMirror(dist(gen));

	piece->thread_handle->submit(ioFailureWrap(piece, [](DownloadData* piece) {
		assert(piece->download->file == nullptr);
		piece->download->file = std::make_unique<CFile>();
		if (!piece->download->file->Open(piece->download->name)) {
			piece->download->file = nullptr;
			return false;
		}
		if (piece->download->out_hash != nullptr) {
			piece->download->out_hash->Init();
		}
		return true;
	}));

	piece->curlw = std::make_unique<CurlWrapper>();
	CURL* curle = piece->curlw->GetHandle();

	curl_easy_setopt(curle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
	curl_easy_setopt(curle, CURLOPT_PRIVATE, piece);
	curl_easy_setopt(curle, CURLOPT_WRITEFUNCTION, multi_write_data);
	curl_easy_setopt(curle, CURLOPT_WRITEDATA, piece);
	curl_easy_setopt(curle, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curle, CURLOPT_XFERINFODATA, piece);
	curl_easy_setopt(curle, CURLOPT_XFERINFOFUNCTION, progress_func);
	curl_easy_setopt(curle, CURLOPT_URL, piece->mirror.c_str());
	curl_easy_setopt(curle, CURLOPT_PIPEWAIT, 1L);
	curl_easy_setopt(curle, CURLOPT_BUFFERSIZE, 16384);

	piece->curlw->AddHeader("X-Prd-Retry-Num: " + std::to_string(piece->retry_num));

	if (piece->download->noCache) {
		piece->curlw->AddHeader("Cache-Control: no-cache");
	}
	if (!piece->download->validateTLS) {
		LOG_DEBUG("Not Validating TLS");
		curl_easy_setopt(curle, CURLOPT_SSL_VERIFYPEER, 0);
	}

	if (piece->download->useETags) {
		if (auto etag = getETag(piece->download->name); etag) {
			piece->curlw->AddHeader("If-None-Match: " + etag.value());
		}
	}

	curl_multi_add_handle(curlm, curle);
	return true;
}

static bool cleanupDownload(DownloadData* data)
{
	bool ok = true;
	auto dl = data->download;
	if (dl->file != nullptr) {
		if (dl->state == IDownload::STATE_DOWNLOADING) {
			// Some other error interrupted overall transfer (this or other file).
			dl->state = IDownload::STATE_FAILED;
		}
		// Drop temp written file when downloading didn't succeed.
		ok = dl->file->Close(/*discard=*/dl->state != IDownload::STATE_FINISHED ||
		                     data->force_discard);
		dl->file = nullptr;
	}
	return ok;
}

struct HTTPStats {
	long http_version = -1;
	int num_errors = 0;
	std::vector<std::chrono::microseconds> time_to_first_byte;
	std::vector<std::chrono::microseconds> total_transfer_time;
};

static std::string curlHttpVersionToString(long version)
{
	switch (version) {
		case CURL_HTTP_VERSION_1_0:
			return "HTTP/1.0";
		case CURL_HTTP_VERSION_1_1:
			return "HTTP/1.1";
		case CURL_HTTP_VERSION_2_0:
			return "HTTP/2";
		case CURL_HTTP_VERSION_3:
			return "HTTP/3";
		default:
			return "HTTP/Unknown(" + std::to_string(version) + ")";
	}
}

static std::string computeStats(std::vector<std::chrono::microseconds> in)
{
	assert(in.size() > 0);
	char buf[200];
	if (in.size() == 1) {
		snprintf(buf, sizeof(buf), "%.3fms", static_cast<double>(in[0].count()) / 1000.0);
		return std::string(buf);
	}
	std::vector<double> t(in.size());
	std::transform(in.begin(), in.end(), t.begin(),
	               [](std::chrono::microseconds i) { return i.count() / 1000.0; });
	std::sort(t.begin(), t.end(), std::greater<double>());
	double mean = std::accumulate(t.begin(), t.end(), 0.0) / t.size();
	snprintf(buf, sizeof(buf), "[max: %.3fms 95%%: %.3fms median: %.3fms mean: %.3fms]", t[0],
	         t[static_cast<int>(0.05 * t.size())], t[t.size() / 2], mean);
	return std::string(buf);
}

static void writeMd5SumFile(DownloadData* data)
{
	const auto checksumFile = data->download->name + ".md5.gz";
	FILE* f = fileSystem->propen(checksumFile, "wb");
	if (f == nullptr) {
		return;
	}
	int fd = fileSystem->dupFileFD(f);
	if (fd < 0) {
		fclose(f);
		return;
	}
	gzFile out = gzdopen(fd, "wb");
	if (out == Z_NULL) {
		LOG_ERROR("Could not gzdopen %s", checksumFile.c_str());
		fclose(f);
		return;
	}

	std::stringstream outBuf;
	outBuf << data->download->hash->toString() << "  "
		   << pathToU8(u8ToPath(data->download->name).filename()) << "\n";
	const std::string outBufStr = outBuf.str();

	for (std::size_t pos = 0; pos < outBufStr.size();) {
		int written = gzwrite(out, outBufStr.data() + pos, outBufStr.size() - pos);
		if (written <= 0) {
			int errnum = Z_OK;
			const char* errstr = gzerror(out, &errnum);
			LOG_ERROR("Failed to gzwrite to %s: (%d) %s", checksumFile.c_str(), errnum, errstr);
			gzclose(out);
			fclose(f);
			return;
		}
		pos += written;
	}
	if (gzclose(out) != Z_OK) {
		LOG_ERROR("Error in gzclose of %s", checksumFile.c_str());
	}
	fclose(f);
}

static bool handleSuccessTransfer(DownloadData* data, bool http_not_modified,
                                  std::optional<std::string> etag)
{
	if (http_not_modified) {
		LOG_DEBUG("ETag matched for file %s", data->download->name.c_str());
		data->download->state = IDownload::STATE_FINISHED;
		// To not override existing file with empty one.
		data->force_discard = true;
		return true;
	}
	if (data->download->out_hash != nullptr) {
		// Verify that hash matches with expected when we are done.
		data->download->out_hash->Final();
		if (!data->download->hash->compare(data->download->out_hash.get())) {
			data->download->state = IDownload::STATE_FAILED;
			LOG_ERROR("File %s hash validation failed.", data->download->name.c_str());
			return false;
		}
		if (data->download->write_md5sum) {
			writeMd5SumFile(data);
		}
	}
	data->download->state = IDownload::STATE_FINISHED;
	if (data->download->useETags && etag) {
		// We call cleanupDownload here easly so that ETag  computation has a
		// final not tmp file on disk to work with.
		if (!cleanupDownload(data)) {
			return false;
		}
		setETag(data->download->name, etag.value());
	}
	return true;
}

static bool processMessages(CURLM* curlm, std::vector<DownloadData*>* to_retry, HTTPStats* stats)
{
	int msgs_left;
	bool ok = true;
	while (struct CURLMsg* msg = curl_multi_info_read(curlm, &msgs_left)) {
		if (msg->msg != CURLMSG_DONE) {
			// Currently CURLMSG_DONE is the only message defined
			LOG_ERROR("Unhandled message %d", msg->msg);
			continue;
		}
		DownloadData* data;
		curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &data);
		long http_code = 0;
		bool retry = false;
		switch (msg->data.result) {
			case CURLE_OK: {
				curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);

				struct curl_header* etagHeader;
				std::optional<std::string> etag;
				if (curl_easy_header(msg->easy_handle, "ETag", 0, CURLH_HEADER, -1, &etagHeader) ==
				    CURLHE_OK) {
					etag = std::string(etagHeader->value);
				}

				data->thread_handle->submit(
					ioFailureWrap(data, [http_code, etag = std::move(etag)](DownloadData* data) {
						return handleSuccessTransfer(data, http_code == 304, std::move(etag));
					}));
				// Fill in stats for the transfer
				curl_off_t ttfb, totalt;
				long http_version;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_STARTTRANSFER_TIME_T, &ttfb);
				curl_easy_getinfo(msg->easy_handle, CURLINFO_TOTAL_TIME_T, &totalt);
				curl_easy_getinfo(msg->easy_handle, CURLINFO_HTTP_VERSION, &http_version);
				if (stats->http_version != -1 && stats->http_version != http_version) {
					LOG_WARN("Multiple http versions used for transfer, %s and %s",
					         curlHttpVersionToString(stats->http_version).c_str(),
					         curlHttpVersionToString(http_version).c_str());
				}
				stats->http_version = http_version;
				stats->time_to_first_byte.emplace_back(ttfb);
				stats->total_transfer_time.emplace_back(totalt);

				break;
			}
			// We consider this list of errors worth retrying.
			case CURLE_COULDNT_CONNECT:
			case CURLE_WEIRD_SERVER_REPLY:
			case CURLE_HTTP2:
			case CURLE_PARTIAL_FILE:
			case CURLE_OPERATION_TIMEDOUT:
			case CURLE_SSL_CONNECT_ERROR:
			case CURLE_GOT_NOTHING:
			case CURLE_SEND_ERROR:
			case CURLE_RECV_ERROR:
			case CURLE_HTTP2_STREAM:
			case CURLE_HTTP3:
			case CURLE_QUIC_CONNECT_ERROR:
			case CURLE_HTTP_RETURNED_ERROR:
				retry = true;
				[[fallthrough]];
			default:
				++stats->num_errors;
				curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);
				// Let's not retry on client errors except for the 429 Too Many Requests
				if (http_code >= 400 && http_code < 500 && http_code != 429) {
					retry = false;
				}
				if (retry) {
					curl_off_t retry_after_wait_s = 0;
					curl_easy_getinfo(msg->easy_handle, CURLINFO_RETRY_AFTER, &retry_after_wait_s);
					data->retry_after_from_server = std::chrono::seconds(retry_after_wait_s);

					to_retry->push_back(data);
				}

				LOG_ERROR("CURL error(%d:%d): %s %d (%s)%s", msg->msg, msg->data.result,
				          curl_easy_strerror(msg->data.result), http_code, data->mirror.c_str(),
				          retry ? ", will retry" : ", aborting");

				ok = ok && retry;
		}
		data->thread_handle->submit(ioFailureWrap(data, cleanupDownload));
		if (data->curlw != nullptr) {
			curl_multi_remove_handle(curlm, data->curlw->GetHandle());
			data->curlw = nullptr;
		}
	}
	return ok;
}

bool computeRetry(DownloadData* data)
{
	const auto now = std::chrono::steady_clock::now();
	using namespace std::chrono_literals;
	constexpr int retry_num_limit = 10;
	++data->retry_num;
	if (data->retry_num > retry_num_limit) {
		LOG_ERROR("Limit of retried (%d) reached for %s, aborting", retry_num_limit,
		          data->download->name.c_str());
		return false;
	}
	if (data->retry_after_from_server > 30s) {
		LOG_ERROR("Server asked us to retry after %ds which is longer then max of 30s, aborting",
		          data->retry_after_from_server.count());
		return false;
	}
	auto backoff = retryAfter(data->retry_num, 100ms, 5s);
	if (data->retry_after_from_server > 0s) {
		backoff = data->retry_after_from_server;
	}
	data->next_retry = now + backoff;
	return true;
}

static unsigned getMaxReqsPerSecLimit()
{
	unsigned long max_req_per_sec = 0;  // unlimited
	const char* max_req_per_sec_env = std::getenv("PRD_MAX_HTTP_REQS_PER_SEC");
	if (max_req_per_sec_env != nullptr) {
		char* end;
		max_req_per_sec = std::strtoul(max_req_per_sec_env, &end, 10);
		if (max_req_per_sec == ULONG_MAX || *end != '\0') {
			LOG_ERROR("PRD_MAX_HTTP_REQS_PER_SEC env variable value is not valid.");
			return false;
		}
	}
	return max_req_per_sec;
}

bool CHttpDownloader::download(std::list<IDownload*>& download, int max_parallel)
{
	TRACE();

	// With CURLOPT_BUFFERSIZE = 16KiB, this ends up with a very theoretical
	// max 250MiB buffered in memory before it's written to disk.
	IOThreadPool thread_pool(
		download.size() < 10 ? 1 : std::min(16u, std::thread::hardware_concurrency()), 1000);

	// Prepare downloads from input.
	std::vector<std::unique_ptr<DownloadData>> downloads;
	DownloadDataPack download_pack;
	bool abort_download = false;
	for (IDownload* dl : download) {
		if (dl->isFinished()) {
			continue;
		}
		if (dl->dltype != IDownload::TYP_HTTP) {
			LOG_DEBUG("skipping non http-dl")
			continue;
		}
		if (dl->getMirrorCount() <= 0) {
			LOG_WARN("No mirrors found");
			return false;
		}
		downloads.emplace_back(std::make_unique<DownloadData>(thread_pool.getHandle()));
		auto dlData = downloads.back().get();
		dlData->abort_download = &abort_download;
		dlData->download = dl;
		dlData->data_pack = &download_pack;
		if (dl->size > 0) {
			dlData->approx_size = dl->size;
			download_pack.size += dl->size;
		} else {
			dlData->approx_size = dl->approx_size;
			download_pack.size += dl->approx_size;
		}
	}
	if (downloads.empty()) {
		LOG_DEBUG("Nothing to download!");
		return true;
	}

	// Shuffle files to try to distribute big and small files evenly during
	// download to maximize bandwidth and distribute required file IO.
	std::shuffle(downloads.begin(), downloads.end(),
	             std::default_random_engine(std::random_device{}()));
	auto downloads_it = downloads.begin();

	// Perform actual download using the Curl multi interface.
	HTTPStats stats;
	CURLM* curlm = CurlWrapper::GetMultiHandle();

	std::vector<DownloadData*> to_retry;
	auto queue_comparator = [](DownloadData* a, DownloadData* b) {
		return a->next_retry < b->next_retry;
	};
	std::priority_queue<DownloadData*, std::vector<DownloadData*>, decltype(queue_comparator)>
		wait_queue(queue_comparator);
	const unsigned max_req_per_sec = getMaxReqsPerSecLimit();
	Throttler throttler(max_req_per_sec, std::min(static_cast<unsigned>(max_parallel),
	                                              std::max(max_req_per_sec / 10, 5U)));

	int running = 0;      // Number of currently running downloads + waiting for retry
	bool aborted = true;  // We use goto with aborted because we have nested loops.
	do {
		CURLMcode ret = curl_multi_perform(curlm, &running);
		if (ret != CURLM_OK) {
			LOG_ERROR("curl_multi_perform failed, code %d.\n", ret);
			goto abort;
		}
		to_retry.clear();
		if (!processMessages(curlm, &to_retry, &stats)) {
			goto abort;
		}

		// Add all requests that should be retried to the wait_queue with delay
		// or fail if we did too many retries already.
		for (DownloadData* data : to_retry) {
			if (!computeRetry(data)) {
				goto abort;
			}
			wait_queue.push(data);
		}

		throttler.refill_bucket();

		// If enough time passed for element in the wait_queue, retry the request.
		running += wait_queue.size();
		auto now = std::chrono::steady_clock::now();
		while (!wait_queue.empty() && wait_queue.top()->next_retry <= now &&
		       throttler.get_token()) {
			if (!setupDownload(curlm, wait_queue.top())) {
				goto abort;
			}
			wait_queue.pop();
		}

		// Start more new requests so we have up to max_parallel happening.
		for (; running < max_parallel && downloads_it != downloads.end() && throttler.get_token();
		     ++running) {
			if (!setupDownload(curlm, (*downloads_it++).get())) {
				goto abort;
			}
		}

		ret = curl_multi_poll(curlm, NULL, 0, 20, NULL);
		if (ret != CURLM_OK) {
			LOG_ERROR("curl_multi_poll, code %d.\n", ret);
			goto abort;
		}

		thread_pool.pullResults();
		if (abort_download) {
			goto abort;
		}
	} while (running > 0 || downloads_it != downloads.end());
	aborted = false;
	LOG_INFO("Download: num files: %u, protocol: %s, to first byte: %s, transfer: %s, num retried "
	         "errors: %d",
	         static_cast<unsigned>(downloads.size()),
	         curlHttpVersionToString(stats.http_version).c_str(),
	         computeStats(stats.time_to_first_byte).c_str(),
	         computeStats(stats.total_transfer_time).c_str(), stats.num_errors);
abort:
	thread_pool.finish();
	// Cleanup
	for (auto& data : downloads) {
		cleanupDownload(data.get());
		if (data->curlw != nullptr) {
			curl_multi_remove_handle(curlm, data->curlw->GetHandle());
			data->curlw = nullptr;
		}
	}
	return !aborted;
}
