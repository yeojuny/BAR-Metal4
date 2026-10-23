/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include <cstdlib>
#include <curl/curl.h>
#include <errno.h>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unordered_set>

#include "Downloader/CurlWrapper.h"
#include "Downloader/Download.h"
#include "Downloader/IDownloader.h"
#include "FileSystem/File.h"
#include "FileSystem/FileData.h"
#include "FileSystem/FileSystem.h"
#include "FileSystem/HashGzip.h"
#include "FileSystem/HashMD5.h"
#include "Logger.h"
#include "RapidDownloader.h"
#include "Sdp.h"
#include "Tracer.h"
#include "Util.h"

CSdp::CSdp(std::string shortname_, std::string md5_, std::string name_,
           std::vector<std::string> depends_, std::string baseUrl_)
	: name(std::move(name_))
	, md5(std::move(md5_))
	, shortname(std::move(shortname_))
	, baseUrl(std::move(baseUrl_))
	, depends(std::move(depends_))
{
	memset(cursize_buf, 0, LENGTH_SIZE);
	const std::string dir =
		fileSystem->getSpringDir() + PATH_DELIMITER + "packages" + PATH_DELIMITER;
	finalSdpPath = dir + md5 + ".sdp";
	tempSdpPath = finalSdpPath + ".incomplete";
}

CSdp::CSdp(CSdp&& sdp) = default;

CSdp::~CSdp() = default;

bool createPoolDirs(const std::string& root)
{
	TRACE();
	for (int i = 0; i < 256; i++) {
		char buf[1024];
		const int len = snprintf(buf, sizeof(buf), "%s%02x%c", root.c_str(), i, PATH_DELIMITER);
		const std::string tmp(buf, len);
		if ((!fileSystem->directoryExists(tmp)) && (!fileSystem->createSubdirs(tmp))) {
			LOG_ERROR("Couldn't create %s", tmp.c_str());
			return false;
		}
	}
	return true;
}

std::unique_ptr<IDownload> CSdp::parseOrGetDownload()
{
	for (auto path : {finalSdpPath, tempSdpPath}) {
		if (!fileSystem->fileExists(path)) {
			continue;
		}
		if (fileSystem->parseSdp(path, files)) {
			return nullptr;
		}
		fileSystem->removeFile(path);
		files.clear();
	}
	auto dl = std::make_unique<IDownload>(tempSdpPath);
	dl->addMirror(baseUrl + "/packages/" + md5 + ".sdp");
	return dl;
}

static std::list<IDownload*>
getDownloadsList(const std::vector<std::unique_ptr<IDownload>>& downloads)
{
	std::list<IDownload*> res;
	for (auto& dl : downloads) {
		res.emplace_back(dl.get());
	}
	return res;
}

static bool useStreamerDownload()
{
	const char* use_streamer_env = std::getenv("PRD_RAPID_USE_STREAMER");
	return use_streamer_env == nullptr || std::string(use_streamer_env) != "false";
}

bool CSdp::Download(std::vector<std::pair<CSdp*, IDownload*>> const& packages)
{
	TRACE();

	// Download Sdp packages
	{
		TRACE("DownloadSdpPackapes");
		std::vector<std::unique_ptr<IDownload>> downloads;
		for (auto [pkg, _] : packages) {
			if (auto dl = pkg->parseOrGetDownload(); dl != nullptr) {
				downloads.emplace_back(std::move(dl));
			}
		}
		auto downloads_list = getDownloadsList(downloads);
		if (!httpDownload->download(downloads_list)) {
			return false;
		}
		for (auto [pkg, _] : packages) {
			if (pkg->files.empty() && pkg->parseOrGetDownload() != nullptr) {
				return false;
			}
		}
	}

	const std::string root = fileSystem->getSpringDir() + PATH_DELIMITER + "pool" + PATH_DELIMITER;
	if (!createPoolDirs(root)) {
		LOG_ERROR("Creating pool directories failed");
		return false;
	}

	// Prepare files in Sdp packages for download
	std::vector<std::pair<CSdp*, IDownload*>> to_download;
	{
		TRACE("ComputePoolFilesToFetch");
		const auto pool_files = fileSystem->getPoolFiles();
		if (!pool_files) {
			return false;
		}
		std::unordered_set<std::string> downloaded_md5;
		for (const auto& [_, md5] : *pool_files) {
			downloaded_md5.insert(md5.toString());
		}
		for (auto [pkg, dl] : packages) {
			if (pkg->filterDownloaded(downloaded_md5)) {
				to_download.emplace_back(pkg, dl);
			} else {
				dl->state = IDownload::STATE_FINISHED;
			}
		}
	}

	// Do actual download.
	if (useStreamerDownload()) {
		for (auto [pkg, dl] : to_download) {
			pkg->m_download = dl;
			if (!pkg->downloadStream()) {
				LOG_ERROR("Couldn't download files for %s", pkg->md5.c_str());
				return false;
			}
			dl->state = IDownload::STATE_FINISHED;
		}
	} else {
		if (!downloadHTTP(to_download)) {
			return false;
		}
		for (auto [pkg, dl] : to_download) {
			dl->state = IDownload::STATE_FINISHED;
		}
	}

	for (auto [pkg, dl] : packages) {
		if (dl->state == IDownload::STATE_FINISHED && fileSystem->fileExists(pkg->tempSdpPath) &&
		    !fileSystem->Rename(pkg->tempSdpPath, pkg->finalSdpPath)) {
			return false;
		}
	}

	return true;
}

bool CSdp::filterDownloaded(std::unordered_set<std::string> const& downloaded_md5)
{
	TRACE();
	bool need_to_download = false;
	for (FileData& filedata : files) {  // check which file are available on local
		                                // disk -> create list of files to download
		HashMD5 fileMd5;
		fileMd5.Set(filedata.md5, sizeof(filedata.md5));
		if (downloaded_md5.find(fileMd5.toString()) == downloaded_md5.end()) {
			need_to_download = true;
			filedata.download = true;
		} else {
			filedata.download = false;
		}
	}
	return need_to_download;
}

static bool OpenNextFile(CSdp& sdp)
{
	// file already open, return
	if (sdp.file_handle != nullptr) {
		return true;
	}

	// get next file + open it
	while (!sdp.list_it->download) {
		// LOG_ERROR("next file");
		sdp.list_it++;
	}
	assert(sdp.list_it != sdp.files.end());

	HashMD5 fileMd5;
	FileData& fd = *(sdp.list_it);

	fd.compsize = parse_int32(sdp.cursize_buf);
	// LOG_DEBUG("Read length of %d, uncompressed size from sdp: %d", fd.compsize, fd.size);
	assert(fd.size + 5000 >=
	       fd.compsize);  // compressed file should be smaller than uncompressed file

	fileMd5.Set(fd.md5, sizeof(fd.md5));
	sdp.file_name = fileSystem->getPoolFilename(fileMd5.toString());
	sdp.file_handle = std::unique_ptr<CFile>(new CFile());
	if (sdp.file_handle == nullptr) {
		LOG_ERROR("couldn't open %s", fd.name.c_str());
		return false;
	}
	sdp.file_handle->Open(sdp.file_name);
	sdp.file_pos = 0;
	return true;
}

static int GetLength(CSdp& sdp, const char* const buf_pos, const char* const buf_end)
{
	// calculate bytes we can skip, could overlap received bufs
	const int toskip = intmin(buf_end - buf_pos, LENGTH_SIZE - sdp.skipped);
	assert(toskip > 0);
	// copy bufs avaiable
	memcpy(sdp.cursize_buf + sdp.skipped, buf_pos, toskip);
	sdp.skipped += toskip;

	//	if (sdp.skipped > 0) { //size was in at least two packets
	LOG_DEBUG("%.2x %.2x %.2x %.2x", sdp.cursize_buf[0], sdp.cursize_buf[1], sdp.cursize_buf[2],
	          sdp.cursize_buf[3]);
	//	}

	return toskip;
}

static void SafeCloseFile(CSdp& sdp)
{
	if (sdp.file_handle == nullptr)
		return;

	sdp.file_handle->Close();
	sdp.file_handle = nullptr;
	sdp.file_pos = 0;
	sdp.skipped = 0;
}

static int WriteData(CSdp& sdp, const char* const buf_pos, const char* const buf_end)
{
	// minimum of bytes to write left in file and bytes to write left in buf
	const FileData& fd = *(sdp.list_it);
	const long towrite = intmin(fd.compsize - sdp.file_pos, buf_end - buf_pos);
	//	LOG_DEBUG("towrite: %d total size: %d, uncomp size: %d pos: %d", towrite,
	// fd.compsize,fd.size, sdp.file_pos);
	assert(towrite >= 0);
	assert(fd.compsize > 0);  //.gz are always > 0
	if (towrite == 0) {
		return 0;
	}

	if (!sdp.file_handle->Write(buf_pos, towrite)) {
		return -1;
	}
	sdp.file_pos += towrite;

	// file finished -> next file
	if (sdp.file_pos >= fd.compsize) {
		SafeCloseFile(sdp);
		if (!fileSystem->fileIsValid(&fd, sdp.file_name.c_str())) {
			LOG_ERROR("File is broken?!: %s", sdp.file_name.c_str());
			fileSystem->removeFile(sdp.file_name.c_str());
			return -1;
		}
		++sdp.list_it;
		memset(sdp.cursize_buf, 0, 4);  // safety
	}
	return towrite;
}

void dump_data(CSdp& sdp, const char* const /*buf_pos*/, const char* const /*buf_end*/)
{
	LOG_WARN("%s %d\n", sdp.file_name.c_str(), sdp.list_it->compsize);
}


/**
        write the data received from curl to the rapid pool.

        the filename is read from the sdp-list (created at request start)
        filesize is read from the http-data received (could overlap!)
*/
static size_t write_streamed_data(const void* buf, size_t size, size_t nmemb, CSdp* psdp)
{
	// LOG_DEBUG("write_stream_data bytes read: %d", size * nmemb);
	if (psdp == nullptr) {
		LOG_ERROR("nullptr in write_stream_data");
		return -1;
	}
	CSdp& sdp = *psdp;

	if (IDownloader::AbortDownloads())
		return -1;
	const char* buf_start = (const char*)buf;
	const char* buf_end = buf_start + size * nmemb;
	const char* buf_pos = buf_start;

	// all bytes written?
	while (buf_pos < buf_end) {
		// check if we skipped all 4 bytes for
		if (sdp.skipped < LENGTH_SIZE) {
			const int skipped = GetLength(sdp, buf_pos, buf_end);
			buf_pos += skipped;
		}
		if (sdp.skipped < LENGTH_SIZE) {
			LOG_DEBUG("packed end, skipped: %d, bytes left: %d", sdp.skipped, buf_end - buf_pos);
			assert(buf_pos == buf_end);
			break;
		}

		assert(sdp.skipped == LENGTH_SIZE);

		if (!OpenNextFile(sdp))
			return -1;

		assert(sdp.file_handle != nullptr);
		assert(sdp.list_it != sdp.files.end());

		const int written = WriteData(sdp, buf_pos, buf_end);
		if (written < 0) {
			dump_data(sdp, buf_pos, buf_end);
			return -1;
		}
		buf_pos += written;
	}
	return buf_pos - buf_start;
}

/** *
        draw a nice download status-bar
*/
static int progress_func(CSdp& sdp, curl_off_t TotalToDownload, curl_off_t NowDownloaded,
                         curl_off_t, curl_off_t)
{
	if (IDownloader::AbortDownloads())
		return -1;
	sdp.m_download->rapid_size[&sdp] = TotalToDownload;
	sdp.m_download->map_rapid_progress[&sdp] = NowDownloaded;
	uint64_t total = 0;
	for (auto it : sdp.m_download->rapid_size) {
		total += it.second;
	}
	sdp.m_download->size = total;
	if (IDownloader::listener != nullptr) {
		IDownloader::listener(NowDownloaded, TotalToDownload);
	}
	total = 0;
	for (auto it : sdp.m_download->map_rapid_progress) {
		total += it.second;
	}
	sdp.m_download->updateProgress(total);
	if (TotalToDownload == NowDownloaded)  // force output when download is
	                                       // finished
		LOG_PROGRESS(NowDownloaded, TotalToDownload, true);
	else
		LOG_PROGRESS(NowDownloaded, TotalToDownload);
	return 0;
}

bool CSdp::downloadStream()
{
	TRACE();
	std::string downloadUrl = baseUrl + "/streamer.cgi?" + md5;
	CurlWrapper curlw;

	CURLcode res;
	LOG_INFO("Using rapid");
	LOG_INFO(downloadUrl.c_str());

	curl_easy_setopt(curlw.GetHandle(), CURLOPT_URL, downloadUrl.c_str());

	SafeCloseFile(*this);

	list_it = files.begin();
	file_name = "";

	const int buflen = (files.size() / 8) + 1;
	std::vector<char> buf(buflen, 0);

	int i = 0;
	for (FileData& fd : files) {
		if (fd.download) {
			buf[i / 8] |= (1 << (i % 8));
		}
		i++;
	}

	int destlen = files.size() * 2 + 1024;
	std::vector<char> dest(destlen, 0);
	LOG_DEBUG("Files: %d Buflen: %d Destlen: %d", (int)files.size(), buflen, destlen);

	gzip_str(&buf[0], buflen, &dest[0], &destlen);

	curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEFUNCTION, write_streamed_data);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_WRITEDATA, this);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_POSTFIELDS, &dest[0]);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_POSTFIELDSIZE, destlen);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_XFERINFOFUNCTION, progress_func);
	curl_easy_setopt(curlw.GetHandle(), CURLOPT_XFERINFODATA, this);

	res = curl_easy_perform(curlw.GetHandle());

	SafeCloseFile(*this);

	/* always cleanup */
	if (res != CURLE_OK) {
		LOG_ERROR("Curl error: %s", curl_easy_strerror(res));
		return false;
	}


	return true;
}

std::string CSdp::getPoolFileUrl(const std::string& md5s) const
{
	return baseUrl + "/pool/" + md5s.substr(0, 2) + "/" + md5s.substr(2) + ".gz";
}

bool CSdp::downloadHTTP(std::vector<std::pair<CSdp*, IDownload*>> const& packages)
{
	TRACE();
	std::unordered_set<std::string> md5_in_queue;
	std::list<IDownload*> dls;
	for (auto [pkg, _] : packages) {
		for (FileData& fd : pkg->files) {
			if (!fd.download)
				continue;
			auto fileMd5 = std::make_unique<HashMD5>();
			fileMd5->Set(fd.md5, sizeof(fd.md5));
			// Multiple files in sdp can map to a single file in the pool,
			// we need to skip duplicates.
			if (md5_in_queue.find(fileMd5->toString()) != md5_in_queue.end()) {
				continue;
			}
			md5_in_queue.insert(fileMd5->toString());
			std::string url = pkg->getPoolFileUrl(fileMd5->toString());
			std::string filename = fileSystem->getPoolFilename(fileMd5->toString());
			IDownload* dl = new IDownload(filename);
			dl->addMirror(url);
			dl->approx_size = fd.size;
			dl->hash = std::move(fileMd5);
			dl->out_hash = std::make_unique<HashGzip>(std::make_unique<HashMD5>());
			dls.push_back(dl);
		}
	}
	bool ok = httpDownload->download(dls, 100);
	IDownloader::freeResult(dls);
	return ok;
}
