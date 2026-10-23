/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include "DownloadEnum.h"
#include "FileSystem/File.h"
#include "FileSystem/IHash.h"
#include "Rapid/Sdp.h"
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

class DownloadData;

class IDownload
{
public:
	DownloadEnum::Category cat;

	enum download_type { TYP_RAPID, TYP_HTTP } dltype;

	IDownload(std::string filename = "", std::string orig_name = "",
	          DownloadEnum::Category cat = DownloadEnum::CAT_NONE, download_type typ = TYP_HTTP);
	/**
	 * Add a mirror to the download specified
	 */
	bool addMirror(const std::string& url);
	bool addDepend(const std::string& depend);
	bool isFinished() const
	{
		return state == STATE_FINISHED;
	}
	std::string name;         // name, in most cases the filename to save to
	std::string origin_name;  // name of object. Not the filename

	std::list<std::string> depend;  // list of all depends

	std::string getMirror(unsigned i = 0) const;
	int getMirrorCount() const;

	enum PIECE_STATE {
		STATE_NONE,         // nothing was done with this piece
		STATE_DOWNLOADING,  // piece is currently downloaded, something
		                    // was writen to the file
		STATE_FAILED,       // piece failed to download or verify
		STATE_FINISHED,     // piece downloaded successfully and verified
	};

	bool write_md5sum = false;
	// What the hash the download is supposed to have.
	std::unique_ptr<IHash> hash;
	// To store the actual hash of the download. Need to be set by the
	// caller because we don't know the concrete type.
	std::unique_ptr<IHash> out_hash;
	std::unique_ptr<CFile> file;

	/**
	 * file size
	 */
	int64_t size = -1;

	/**
	 * Approximate file size, for cases where real size isn't entirely
	 * known, e.g. when downloading compressed files from spd.
	 * We default it to 1, as a simple file counter.
	 */
	uint64_t approx_size = 1;

	std::map<CSdp*, uint64_t> rapid_size;
	std::map<CSdp*, uint64_t> map_rapid_progress;

	/**
	 * state for whole file
	 */
	PIECE_STATE state = IDownload::STATE_NONE;

	/**
	 * returns number of bytes downloaded
	 */
	uint64_t getProgress() const;
	void updateProgress(uint64_t progress);
	std::string version;

	bool validateTLS = true;
	bool noCache = false;
	bool useETags = false;

private:
	uint64_t progress = 0;
	std::vector<std::string> mirrors;
	static void initCategories();
};
