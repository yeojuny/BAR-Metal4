/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "BufferedArchive.h"
#include "System/GlobalConfig.h"
#include "System/MainDefines.h"
#include "System/Log/ILog.h"

#include <cassert>

CBufferedArchive::~CBufferedArchive()
{
 const auto stats=fileCache.GetStatistics();
 LOG_L(L_INFO, "[BufferedArchiveCache127][name=%s] retained=%zu peak=%zu files=%zu limit=%zu hits=%llu misses=%llu evictions=%llu",
  archiveFile.c_str(),stats.retained,stats.peak,stats.files,fileCache.Limit(),
  static_cast<unsigned long long>(stats.hits),static_cast<unsigned long long>(stats.misses),static_cast<unsigned long long>(stats.evictions));
}

bool CBufferedArchive::GetFile(uint32_t fid, std::vector<std::uint8_t>& buffer)
{
	assert(IsFileId(fid));

	int ret = 0;

	auto scopedSemAcq = AcquireSemaphoreScoped();

	if (!globalConfig.vfsCacheArchiveFiles || noCache) {
		if ((ret = GetFileImpl(fid, buffer)) != 1)
			LOG_L(L_ERROR, "[BufferedArchive::%s(fid=%u)][noCache=%d,vfsCache=%d] name=%s ret=%d size=" _STPF_, __func__, fid, static_cast<int>(noCache), static_cast<int>(globalConfig.vfsCacheArchiveFiles), archiveFile.c_str(), ret, buffer.size());

		return (ret == 1);
	}

 // NumFiles is fixed for this archive. The cache lock covers only metadata;
 // the original archive semaphore and distinct-file parallel I/O stay intact.
 const auto cached=fileCache.Lookup(fid,NumFiles());
 if(cached.data){buffer.assign(cached.data->begin(),cached.data->end());return true;}

	if ((ret = GetFileImpl(fid, buffer)) != 1)
		LOG_L(L_ERROR, "[BufferedArchive::%s(fid=%u)][noCache=%d,vfsCache=%d] name=%s ret=%d size=" _STPF_, __func__, fid, static_cast<int>(noCache), static_cast<int>(globalConfig.vfsCacheArchiveFiles), archiveFile.c_str(), ret, buffer.size());

 // Re-admit successful evicted entries; failed/partial reads are never cached.
 if(cached.eligible && ret==1)fileCache.Store(fid,buffer);
 
	return (ret == 1);
}
