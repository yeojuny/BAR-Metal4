/* Bounded CPU-only MRT observation; no GPU commands or ownership changes. */
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <limits>
namespace rendering::metal4 {
inline bool MRT174Enabled(){static const bool on=[](){const char* v=std::getenv("BAR_NATIVE_GEOMETRY_MRT174");return v&&v[0]=='1'&&v[1]=='\0';}();return on;}
inline bool MRT174Window(uint64_t frame){return frame>=2048&&frame<2112;}
// Process-wide OUTPUT quota only; no cross-renderer cost aggregation.
inline std::atomic<unsigned>& MRT174RowQuota(){static std::atomic<unsigned> rows{0};return rows;}
inline bool MRT174IssueAllowed(){static std::atomic<unsigned> rows{0};return rows.fetch_add(1,std::memory_order_relaxed)<8;}
struct MRT174Observation {
 std::array<uint64_t,7> calls{},bytes{}; // color0..4,combined5,depth6
 uint64_t width=0,height=0,depth=0,unknownFormat=0,invalidExtent=0,overflow=0;
 bool copyEncoder=false;
 void Issued(unsigned slot,uint64_t w,uint64_t h,uint64_t d,uint64_t bpp)noexcept{
  if(slot>=7){++overflow;return;}++calls[slot];
  if(!width){width=w;height=h;depth=d;}else if(width!=w||height!=h||depth!=d)++invalidExtent;
  if(!w||!h||!d){++invalidExtent;return;}
  if(!bpp){++unknownFormat;return;}
  const auto max=std::numeric_limits<uint64_t>::max();uint64_t n=w;
  for(auto factor:{h,d,bpp}){if(n>max/factor){++overflow;return;}n*=factor;}
  if(bytes[slot]>max-n){++overflow;return;}bytes[slot]+=n;
 }
 uint64_t Calls()const noexcept{uint64_t n=0;for(auto v:calls)n+=v;return n;}
 uint64_t Bytes()const noexcept{uint64_t n=0;for(auto v:bytes)n+=v;return n;}
};
} // namespace rendering::metal4
