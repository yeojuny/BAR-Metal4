/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include <string>
#include <vector>

class CSdp;
class CRapidDownloader;
class IDownload;

class CRepo
{
public:
	CRepo(const std::string& repourl, const std::string& shortname, CRapidDownloader* rapid);

	/**
	 * returns download for a repo file
	 */
	IDownload* getDownload();

	/**
	 * parse a repo file (versions.gz)
	 * a line looks like
	 * nota:revision:1,52a86b5de454a39db2546017c2e6948d,,NOTA test-1
	 * <tag>,<md5>,<depends on (descriptive name)>,<descriptive name>
	 */
	bool parse();

	bool deleteRepoFile();

	const std::string& getShortName() const
	{
		return shortname;
	}

private:
	std::string repourl;
	CRapidDownloader* rapid;
	std::vector<CSdp*> sdps;
	std::string tmpFile;
	std::string shortname;
};
