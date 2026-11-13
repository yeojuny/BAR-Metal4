/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <ctime>
namespace rendering::metal4 {
struct NativeThreadCPUStamp {std::uint64_t ns=0;bool valid=false;};
// Thread CPU time excludes time blocked or descheduled; it does not locate
// runnable slices inside a wall envelope. Failure stays explicit, never zero work.
inline NativeThreadCPUStamp ReadNativeThreadCPUTime() noexcept {
#if defined(CLOCK_THREAD_CPUTIME_ID) || defined(__APPLE__)
 timespec value{};
 if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0 && value.tv_sec>=0 && value.tv_nsec>=0 && value.tv_nsec<1000000000)
  return {std::uint64_t(value.tv_sec)*1000000000u+std::uint64_t(value.tv_nsec),true};
#endif
 return {};
}
}
