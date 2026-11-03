/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

// Immutable cache snapshots let readers copy outside the metadata lock. The
// limit bounds retained payload, not caller buffers or in-progress reads.
class BufferedArchiveCache {
public:
 using Bytes=std::vector<std::uint8_t>;
 using Snapshot=std::shared_ptr<const Bytes>;
 static constexpr std::size_t Unlimited=std::numeric_limits<std::size_t>::max();
 struct Read { Snapshot data; bool eligible; };
 struct Statistics { std::size_t retained=0,peak=0,files=0; std::uint64_t hits=0,misses=0,evictions=0; };
 explicit BufferedArchiveCache(std::size_t limit=Unlimited):limit(limit) {}

 Read Lookup(std::uint32_t fid,std::uint32_t count) {
  std::lock_guard lock(mutex);
  if(entries.empty())entries.resize(count);
  assert(fid<entries.size());auto& entry=entries[fid];
  entry.accesses=std::min<unsigned>(2,entry.accesses+1);
  if(entry.data){Touch(fid);++stats.hits;}else ++stats.misses;
  return {entry.data,entry.accesses>=2};
 }
 void Store(std::uint32_t fid,const Bytes& bytes) {
  if(limit==0 || bytes.size()>limit)return;
  // Copying/decompression and destruction of evicted payloads never hold the
  // metadata lock. A racing reader retains its own immutable shared snapshot.
  auto data=std::make_shared<const Bytes>(bytes);
  std::vector<Snapshot> retired;
  {
   std::lock_guard lock(mutex);assert(fid<entries.size());
   auto& entry=entries[fid];if(entry.data)return;
   while(bytes.size()>limit-stats.retained){
    assert(oldest!=None);const auto victim=oldest;
    retired.push_back(std::move(entries[victim].data));
    stats.retained-=retired.back()->size();--stats.files;++stats.evictions;
    Remove(victim);
   }
   entry.data=std::move(data);Append(fid);stats.retained+=bytes.size();
   ++stats.files;stats.peak=std::max(stats.peak,stats.retained);
  }
 }
 Statistics GetStatistics() const {std::lock_guard lock(mutex);return stats;}
 std::size_t Limit() const {return limit;}
private:
 static constexpr std::uint32_t None=std::numeric_limits<std::uint32_t>::max();
 struct Entry {Snapshot data;std::uint32_t previous=None,next=None;unsigned char accesses=0;};
 void Remove(std::uint32_t fid) {
  auto& e=entries[fid];
  if(e.previous!=None)entries[e.previous].next=e.next;else oldest=e.next;
  if(e.next!=None)entries[e.next].previous=e.previous;else newest=e.previous;
  e.previous=e.next=None;
 }
 void Append(std::uint32_t fid) {
  auto& e=entries[fid];e.previous=newest;e.next=None;
  if(newest!=None)entries[newest].next=fid;else oldest=fid;
  newest=fid;
 }
 void Touch(std::uint32_t fid) {if(fid!=newest){Remove(fid);Append(fid);}}
 const std::size_t limit;
 mutable std::mutex mutex;
 std::vector<Entry> entries;
 std::uint32_t oldest=None,newest=None;
 Statistics stats;
};
