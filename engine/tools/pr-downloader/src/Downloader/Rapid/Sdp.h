/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "FileSystem/FileData.h"

#define LENGTH_SIZE 4

class IDownload;
class CFile;

class CSdp
{
public:
	CSdp(std::string shortname, std::string md5, std::string name, std::vector<std::string> depends,
	     std::string baseUrl);
	CSdp(CSdp&& sdp);

	~CSdp();

	static bool Download(std::vector<std::pair<CSdp*, IDownload*>> const& packages);
	/**
	 * returns md5 of a repo
	 */
	const std::string& getMD5() const
	{
		return md5;
	}
	/**
	 * returns the descriptional name
	 */
	const std::string& getName() const
	{
		return name;
	}
	/**
	 * returns the shortname, for example ba:stable
	 */
	const std::string& getShortName() const
	{
		return shortname;
	}
	/**
	 * returns the shortname, for example ba:stable
	 */
	const std::vector<std::string>& getDepends() const
	{
		return depends;
	}

	IDownload* m_download = nullptr;
	std::vector<FileData>::iterator list_it;
	std::vector<FileData> files;  // list with all files of an sdp
	std::unique_ptr<CFile> file_handle;
	std::string file_name;

	unsigned int file_pos = 0;
	unsigned int skipped = 0;
	unsigned char cursize_buf[LENGTH_SIZE];
	unsigned int cursize = 0;

private:
	/**
	 * If Sdp file is downloaded and succesfully parsed returns nullptr, else returns IDownload to
	 * download that file.
	 */
	std::unique_ptr<IDownload> parseOrGetDownload();

	/**
	 * Marks entries from `files` list to download or not depending on whatever there is entry is
	 * present in downloaded_md5 set. Returns true if there are any files to download.
	 */
	bool filterDownloaded(std::unordered_set<std::string> const& downloaded_md5);

	void parse();
	/**
	 * download files streamed
	 *
	 * streamer.cgi works as follows:
	 * - The client does a POST to /streamer.cgi?<hex>
	 *   Where hex = the name of the .sdp
	 * - The client then sends a gzipped bitarray representing the files it wishes to download.
	 *   Bitarray is formated in the obvious way, an array of characters where each file in the sdp
	 *   is represented by the (index mod 8) bit (shifted left) of the (index div 8) byte of the
	 *   array.
	 * - streamer.cgi then responds with
	 *   <big endian encoded int32 length> <data of gzipped pool file>
	 *   for all files requested. Files in the pool are also gzipped, so there is no need to
	 *   decompress unless you wish to verify integrity. Note: The filesize here isn't the same as
	 *   in the .sdp, the sdp-file contains the uncompressed size
	 * - streamer.cgi also sets the Content-Length header in the reply so you can implement a proper
	 *   progress bar.
	 */
	bool downloadStream();
	std::string getPoolFileUrl(const std::string& md5str) const;
	static bool downloadHTTP(std::vector<std::pair<CSdp*, IDownload*>> const& packages);

	std::string name;
	std::string md5;
	std::string shortname;
	std::string baseUrl;
	std::string tempSdpPath;
	std::string finalSdpPath;
	std::vector<std::string> depends;
};
