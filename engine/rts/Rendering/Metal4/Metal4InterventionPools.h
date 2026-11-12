/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdlib>
#include <string_view>
#include <vector>
namespace rendering::metal4 {
// Separate intervention: opt-in, no changes to the release defaults.
inline bool InterventionPoolsEnabled() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_INTERVENTION_POOLS");return value&&std::string_view(value)=="1";}();
 return enabled;
}
// Texture storage and view properties are immutable. Strong source ownership
// prevents pointer reuse; sampling base/count changes select a different view.
// Eviction is safe: each submitting lease also retains/registers its view.
inline bool InterventionPrivateTexturesEnabled() {
 static const bool enabled=[] {const char* value=std::getenv("BAR_NATIVE_INTERVENTION_PRIVATE_TEXTURES");return value&&std::string_view(value)=="1";}();return enabled;
}
class InterventionShadowViews {
 struct Entry {__strong id<MTLTexture> source=nil,view=nil;bool colored=false;size_t bytes=0;};
 std::vector<Entry> entries;
 size_t bytes=0;
 __strong id<MTLDevice> device=nil;
 const void* context=nullptr;
public:
 template<class Create> id<MTLTexture> Acquire(RuntimeLease& lease,id<MTLTexture> source,bool colored,Create create) {
  if(device!=lease.Device()||context!=lease.ContextIdentity()){entries.clear();bytes=0;device=lease.Device();context=lease.ContextIdentity();}
  for(const auto& entry:entries)if(entry.source==source&&entry.colored==colored)return entry.view;
  auto view=create();if(!view)return nil;
  const size_t footprint=source.allocatedSize;constexpr size_t ByteLimit=64u*1024u*1024u;
  if(!footprint||footprint>ByteLimit)return view;
  while(!entries.empty()&&(entries.size()>=8||footprint>ByteLimit-bytes)){bytes-=entries.front().bytes;entries.erase(entries.begin());}
  entries.push_back(Entry{source,view,colored,footprint});bytes+=footprint;return view;
 }
};
class InterventionMipViews {
 struct Entry {__strong id<MTLTexture> source=nil,view=nil;uint32_t base=0,count=0;size_t bytes=0;};
 std::vector<Entry> entries;
 size_t bytes=0;
 __strong id<MTLDevice> device=nil;
 const void* context=nullptr;
public:
 void Begin(RuntimeLease& lease) {
  if(device!=lease.Device()||context!=lease.ContextIdentity()){entries.clear();bytes=0;device=lease.Device();context=lease.ContextIdentity();}
 }
 template<class Create> id<MTLTexture> Acquire(id<MTLTexture> source,uint32_t base,uint32_t count,Create create) {
  for(const auto& entry:entries)if(entry.source==source&&entry.base==base&&entry.count==count)return entry.view;
  auto view=create();if(!view)return nil;
  const size_t footprint=source.allocatedSize;
  constexpr size_t ByteLimit=64u*1024u*1024u;
  // Views may not report an independent footprint. Do not retain sources of
  // unknown size, and never retain more than 64 MiB of reported source storage.
  if(!footprint||footprint>ByteLimit)return view;
  while(!entries.empty()&&(entries.size()>=256||footprint>ByteLimit-bytes)){bytes-=entries.front().bytes;entries.erase(entries.begin());}
  entries.push_back(Entry{source,view,base,count,footprint});bytes+=footprint;
  return view;
 }
};
}
