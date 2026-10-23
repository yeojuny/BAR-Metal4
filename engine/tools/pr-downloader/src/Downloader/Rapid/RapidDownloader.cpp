/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "RapidDownloader.h"
#include "Downloader/Download.h"
#include "FileSystem/FileSystem.h"
#include "Logger.h"
#include "Repo.h"
#include "Sdp.h"
#include "Tracer.h"
#include "Util.h"

#include <algorithm>  //std::min
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <zlib.h>

#ifndef _WIN32
#include <regex.h>
#endif
#undef min
#undef max

CRapidDownloader::CRapidDownloader()
{
	char* master_repo_env = std::getenv("PRD_RAPID_REPO_MASTER");
	if (master_repo_env == nullptr) {
		reposgzurl = REPO_MASTER;
	} else {
		reposgzurl = master_repo_env;
	}
}

void CRapidDownloader::addRemoteSdp(CSdp&& sdp)
{
	sdps.push_back(std::move(sdp));
}

bool CRapidDownloader::list_compare(const CSdp& first, const CSdp& second)
{
	const std::string& name1 = first.getShortName();
	const std::string& name2 = second.getShortName();
	const unsigned len = std::min(name1.size(), name2.size());
	for (unsigned i = 0; i < len; i++) {
		int c1 = tolower(name1[i]);
		int c2 = tolower(name2[i]);
		if (c1 == c2) {
			continue;
		}
		return c1 < c2;
	}
	return false;
}

bool CRapidDownloader::download_name(std::list<IDownload*>& downloads)
{
	std::unordered_map<std::string, IDownload*> found_packages;
	std::unordered_map<IDownload*, IDownload*> deduped;
	std::vector<std::pair<CSdp*, IDownload*>> packages;
	for (auto download : downloads) {
		LOG_DEBUG("Using rapid to download %s", download->name.c_str());
		for (CSdp& sdp : sdps) {
			if (!match_download_name(sdp.getName(), download->name)) {
				continue;
			}
			if (auto it = found_packages.find(sdp.getMD5()); it != found_packages.end()) {
				if (it->second != download) {
					deduped[download] = it->second;
				}
			} else {
				found_packages[sdp.getMD5()] = download;
				LOG_INFO("[Download] %s", sdp.getName().c_str());
				packages.emplace_back(&sdp, download);
			}
		}
	}
	bool ok = CSdp::Download(packages);
	for (auto [deduped, orig] : deduped) {
		deduped->state = orig->state;
	}
	return ok;
}

static std::string stripRapidUri(std::string_view name)
{
	if (name.find("rapid://") == 0) {
		return std::string(name.substr(8));
	};
	return std::string(name);
}

static std::string ensureRapidUri(std::string_view name)
{
	if (name.find("rapid://") == 0) {
		return std::string(name);
	}
	return "rapid://" + std::string(name);
}

bool CRapidDownloader::search(std::list<IDownload*>& result,
                              const std::vector<DownloadSearchItem*>& items)
{
	TRACE();
	std::vector<std::string> to_update;
	for (auto& item : items) {
		if (item->found) {
			continue;
		}
		to_update.emplace_back(stripRapidUri(item->name));
	}

	if (!updateRepos(to_update)) {
		return false;
	}

	sdps.sort(list_compare);

	for (auto& item : items) {
		if (item->found) {
			continue;
		}
		// To make sure that both rapid://zk:stable and zk:stable works.
		auto name = stripRapidUri(item->name);
		for (const CSdp& sdp : sdps) {
			if (match_download_name(sdp.getShortName(), name) ||
			    match_download_name(sdp.getName(), name)) {
				item->found = true;
				// We ensure "rapid://" uri for origin name to have better
				// deduplication when resolving depends that are using uri scheme for
				// rapid tags.
				IDownload* dl = new IDownload(sdp.getName().c_str(), ensureRapidUri(item->name),
				                              item->category, IDownload::TYP_RAPID);
				dl->addMirror(sdp.getShortName().c_str());
				for (auto const& dep : sdp.getDepends()) {
					assert(!dep.empty());
					dl->addDepend(dep);
				}
				result.push_back(dl);
			}
		}
	}
	return true;
}

bool CRapidDownloader::download(std::list<IDownload*>& downloads, int /*max_parallel*/)
{
	TRACE();
	std::list<IDownload*> rapid_downloads;
	for (auto download : downloads) {
		if (download->dltype == IDownload::TYP_RAPID) {
			rapid_downloads.emplace_back(download);
		}
	}
	return rapid_downloads.empty() || download_name(rapid_downloads);
}

bool CRapidDownloader::match_download_name(const std::string& str1, const std::string& str2)
{
	return str2 == "" || str2 == "*" || str1 == str2;
	// FIXME: add regex support for win32
	/*
  #ifndef _WIN32
	      regex_t regex;
	      if (regcomp(&regex, str2.c_str(), 0)==0) {
	              int res=regexec(&regex, str1.c_str(),0, nullptr, 0 );
	              regfree(&regex);
	              if (res==0) {
	                      return true;
	              }
	      }
  #endif
  */
}

bool CRapidDownloader::setOption(const std::string& key, const std::string& value)
{
	LOG_INFO("setOption %s = %s", key.c_str(), value.c_str());
	if (key == "masterurl") {
		reposgzurl = value;
		return true;
	}
	return IDownloader::setOption(key, value);
}

bool CRapidDownloader::UpdateReposGZ()
{
	TRACE();
	std::string tmp;
	if (!urlToPath(reposgzurl, tmp)) {
		LOG_ERROR("Invalid path: %s", tmp.c_str());
		return false;
	}
	path = fileSystem->getSpringDir() + PATH_DELIMITER + "rapid" + PATH_DELIMITER + tmp;
	fileSystem->createSubdirs(CFileSystem::DirName(path));
	LOG_DEBUG("%s", reposgzurl.c_str());
	// first try already downloaded file, as repo master file rarely changes
	if ((fileSystem->fileExists(path)) && (!fileSystem->isOlder(path, REPO_MASTER_RECHECK_TIME)) &&
	    parse())
		return true;
	IDownload dl(path);
	dl.noCache = true;
	dl.addMirror(reposgzurl);
	return httpDownload->download(&dl) && parse();
}

static bool ParseFD(FILE* f, const std::string& path, std::list<CRepo>& repos,
                    CRapidDownloader* rapid)
{
	repos.clear();
	int fd = fileSystem->dupFileFD(f);
	if (fd < 0) {
		return false;
	}
	gzFile fp = gzdopen(fd, "rb");
	if (fp == Z_NULL) {
		LOG_ERROR("Could not open %s", path.c_str());
		return false;
	}
	char buf[IO_BUF_SIZE];
	int i = 0;
	while (gzgets(fp, buf, sizeof(buf)) != Z_NULL) {
		const std::string line = buf;
		const std::vector<std::string> items = tokenizeString(line, ',');
		if (items.size() <= 2) {  // create new repo from url
			LOG_ERROR("Parse Error %s, Line %d: %s", path.c_str(), i, buf);
			gzclose(fp);
			return false;
		}
		i++;
		CRepo repotmp = CRepo(items[1], items[0], rapid);
		repos.push_back(repotmp);
	}
	int errnum = Z_OK;
	bool ok = true;
	const char* errstr = gzerror(fp, &errnum);
	if (errnum != Z_OK && errnum != Z_STREAM_END) {
		LOG_ERROR("Decompression error: %d %s\n", errnum, errstr);
		ok = false;
	}
	gzclose(fp);
	if (!ok) {
		return false;
	}
	if (i <= 0) {
		LOG_ERROR("Broken %s: %d", path.c_str(), i);
		return false;
	}
	LOG_INFO("Found %d repos in %s", repos.size(), path.c_str());
	return true;
}

bool CRapidDownloader::parse()
{
	FILE* f = fileSystem->propen(path, "rb");
	if (f == nullptr) {
		return false;
	}

	const bool res = ParseFD(f, path, repos, this);
	fclose(f);
	if (!res) {
		CFileSystem::removeFile(path);
	}

	return res;
}

bool CRapidDownloader::updateRepos(const std::vector<std::string>& searchstrs)
{
	TRACE();
	LOG_DEBUG("%s", "Updating repos...");
	if (!UpdateReposGZ()) {
		return false;
	}

	std::list<IDownload*> dls;
	std::unordered_set<CRepo*> usedrepos;

	for (auto const& searchstr : searchstrs) {
		std::string tag = "";
		const std::string::size_type pos = searchstr.find(':');
		if (pos != std::string::npos) {  // a tag is found, set it
			tag = searchstr.substr(0, pos);
		}
		for (CRepo& repo : repos) {
			if (tag != "" && repo.getShortName() != tag) {
				continue;
			}
			if (usedrepos.find(&repo) == usedrepos.end()) {
				IDownload* dl = repo.getDownload();
				if (dl == nullptr) {
					continue;
				}
				usedrepos.insert(&repo);
				dls.push_back(dl);
			}
		}
	}

	LOG_DEBUG("Downloading version.gz updates...");
	if (!httpDownload->download(dls)) {
		IDownloader::freeResult(dls);
		return false;
	}
	bool ok = true;
	for (CRepo* repo : usedrepos) {
		if (!repo->parse()) {
			repo->deleteRepoFile();
			ok = false;
			break;
		}
	}
	IDownloader::freeResult(dls);
	return ok;
}
